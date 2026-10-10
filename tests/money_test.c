#include "../init/money.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int fails;
#define CHECK(n, c) do { int ok_ = (c); printf("%s %s\n", ok_ ? "PASS" : "FAIL", n); if (!ok_) fails++; } while (0)

int main(void) {
    int64_t now = 1000; const char *err = NULL; char b[64];
    m_init();
    CHECK("saldo inicial zero", m_balance(A_BRL) == 0 && m_balance(A_USDC) == 0);
    m_load_demo(now); m_load_demo(now);
    CHECK("demo funding idempotente R$10.000", m_balance(A_BRL) == 1000000);
    CHECK("demo funding USDC 100", m_balance(A_USDC) == 100000000);
    m_receive_pix(100000, "Joao", now);
    CHECK("pix recebido soma", m_balance(A_BRL) == 1100000);

    Quote q; CHECK("quote ok", m_quote(A_BRL, A_USDC, 100000, now, &q));
    CHECK("1000 BRL -> 177.946428 USDC", q.to_amt == 177946428);
    Tx *t = m_prepare_swap(&q, now, &err);
    CHECK("swap aguarda auth", t && t->st == S_AWAITING_AUTH);
    CHECK("R$1000 exige CONFIRM", m_required(t) == L_CONFIRM);
    CHECK("PIN nao basta", m_authorize(t, L_PIN, now) != NULL && t->st == S_AWAITING_AUTH && m_balance(A_BRL) == 1100000);
    CHECK("swap confirmado", m_authorize(t, L_CONFIRM, now) == NULL && t->st == S_CONFIRMED && t->hash[0] == 0);
    CHECK("BRL debitado", m_balance(A_BRL) == 1000000);
    CHECK("USDC creditado", m_balance(A_USDC) == 100000000 + 177946428);
    CHECK("reexecutar nao duplica", m_authorize(t, L_CONFIRM, now) != NULL && m_balance(A_BRL) == 1000000);

    Quote q2; m_quote(A_BRL, A_USDC, 50000, now, &q2); Tx *t2 = m_prepare_swap(&q2, now, &err);
    CHECK("cotacao expira", m_authorize(t2, L_PIN, now + 21) != NULL && t2->st == S_EXPIRED);
    CHECK("expirado nao move", m_balance(A_BRL) == 1000000);
    CHECK("prepare com cotacao velha recusa", m_prepare_swap(&q2, now + 21, &err) == NULL);

    Tx *p = m_prepare_pix("Maria", 30000, now, &err);
    CHECK("pix aguarda auth", p && p->st == S_AWAITING_AUTH);
    CHECK("pix R$300 exige so PIN", m_required(p) == L_PIN);
    CHECK("pix confirmado", m_authorize(p, L_PIN, now) == NULL && p->st == S_CONFIRMED);
    CHECK("saldo apos pix", m_balance(A_BRL) == 970000);
    CHECK("limite por tx", m_prepare_pix("Maria", 250000, now, &err) == NULL);
    CHECK("saldo insuficiente", m_prepare_pix("Maria", 99999999, now, &err) == NULL);
    CHECK("valor zero", m_prepare_pix("Maria", 0, now, &err) == NULL);
    CHECK("sem destino", m_prepare_pix("", 100, now, &err) == NULL);

    M.pix_offline = 1;
    Tx *pf = m_prepare_pix("Maria", 1000, now, &err);
    CHECK("pix offline -> FAILED sem mover", m_authorize(pf, L_PIN, now) != NULL && pf->st == S_FAILED && m_balance(A_BRL) == 970000);
    M.pix_offline = 0;

    M.emergency = 1;
    CHECK("emergency bloqueia pix", m_prepare_pix("Maria", 1000, now, &err) == NULL);
    Quote q3; m_quote(A_BRL, A_USDC, 10000, now, &q3);
    CHECK("emergency bloqueia swap", m_prepare_swap(&q3, now, &err) == NULL);
    M.emergency = 0;

    M.pol.per_tx = 10000000; M.pol.daily = 10000000;
    m_receive_pix(900000, "Joao", now);
    Tx *big = m_prepare_pix("Maria", 600000, now, &err);
    CHECK("R$6000 exige COOLDOWN", m_required(big) == L_COOLDOWN);
    CHECK("cooldown segura", m_authorize(big, L_COOLDOWN, now + 5) != NULL && big->st == S_AWAITING_AUTH);
    CHECK("cooldown passa", m_authorize(big, L_COOLDOWN, now + 31) == NULL && big->st == S_CONFIRMED);

    M.pol.per_tx = 200000; M.pol.daily = 500000;
    CHECK("limite diario conta Pix ja feito", m_prepare_pix("Maria", 100000, now + 100, &err) == NULL);

    char *buf = malloc(200000);
    int64_t brl = m_balance(A_BRL), usdc = m_balance(A_USDC), cnt = M.n;
    m_prepare_pix("Maria", 100, now + 90000, &err);
    m_serialize(buf, 200000);
    CHECK("deserializa ok", m_deserialize(buf));
    CHECK("saldos iguais apos recarregar", m_balance(A_BRL) == brl && m_balance(A_USDC) == usdc);
    CHECK("tx pendente vira EXPIRED ao recarregar", M.n == cnt + 1 && M.tx[M.n - 1].st == S_EXPIRED);
    free(buf);
    m_cancel(&M.tx[0], now);
    CHECK("estado terminal nao volta", M.tx[0].st == S_CONFIRMED);

    /* Lone "T" line: it used to read 1 byte past the end of the string (only ASan reports it: make test-asan).
     * Linha "T" sozinha: antes lia 1 byte depois do fim da string (só o ASan acusa: make test-asan). */
    m_deserialize("V1 1 0\nT");
    CHECK("estado com linha T sozinha e ignorado sem ler fora do buffer", M.n == 0);

    /* ---- m_receive_pix nao confia no chamador (achado pelo fuzzer da N15) ---- */
    m_init(); m_load_demo(now);
    { int n0 = M.n; int64_t brl0 = m_balance(A_BRL);
      CHECK("receber valor zero e recusado", m_receive_pix(0, "Joao", now) == NULL);
      CHECK("receber valor negativo e recusado", m_receive_pix(-500, "Joao", now) == NULL);
      CHECK("receber valor acima do teto de sanidade e recusado", m_receive_pix(1000000000000001LL, "Joao", now) == NULL);
      CHECK("recusas nao mudam saldo nem livro-razao", M.n == n0 && m_balance(A_BRL) == brl0);
      CHECK("receber valor normal continua funcionando", m_receive_pix(100, "Joao", now) != NULL && m_balance(A_BRL) == brl0 + 100); }

    /* ---- carga tudo-ou-nada: estado ruim e recusado por inteiro, com motivo ----
     * ---- all-or-nothing load: a bad state is refused whole, with a reason ---- */
    { const char *bad[] = {
        "V1 1 0\nlixo\n",
        "V1 1 0\nV1 1 0\n",
        "V1 1 2\n",
        "V9 1 0\n",
        "T|a|b\n",
        "V1 1 0\nT|id1|k1|0|0|0|0|1|0|0|0||\n",
        "V1 1 0\nT|id1|k1|2|10|-1|0|-5|0|0|0|||x\n" };
      for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
          m_init(); m_load_demo(now);
          int ok = m_deserialize(bad[i]);
          CHECK("estado invalido e recusado", !ok);
          CHECK("recusa deixa o livro-razao vazio", M.n == 0);
          CHECK("recusa informa o motivo", m_deserialize_error()[0] != 0);
      }
      m_init(); m_load_demo(now);
      { char sv[200000]; m_serialize(sv, sizeof sv);
        CHECK("estado valido continua sendo aceito", m_deserialize(sv) && M.n > 0); } }

    m_deserialize("V1 1 0\n");
    fmt_money(b, sizeof b, A_BRL, 1248032); CHECK("format BRL", !strcmp(b, "R$ 12.480,32"));
    fmt_money(b, sizeof b, A_USDC, 177946428); CHECK("format USDC", !strcmp(b, "$ 177.94"));
    fmt_money(b, sizeof b, A_BRL, 5); CHECK("format 0,05", !strcmp(b, "R$ 0,05"));
    CHECK("parse digitos", parse_cents("100050") == 100050 && parse_cents("12a") == -1);

    printf(fails ? "%d FALHAS\n" : "TODOS OK\n", fails);
    return fails != 0;
}
