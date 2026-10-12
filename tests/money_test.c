#include "../init/money.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int fails;
#define CHECK(n, c) do { int ok_ = (c); printf("%s %s\n", ok_ ? "PASS" : "FAIL", n); if (!ok_) fails++; } while (0)

/* The test goes through the REAL path: a verified PIN issues the grant, the engine reads it.
 * `confirm` is the person's explicit second tap. / O teste usa o caminho REAL: PIN conferido emite a concessão,
 * o motor a lê. `confirm` é o segundo toque explícito da pessoa. */
static const char *authz(Tx *t, int confirm, int64_t now) {
    auth_expect(t->id);
    if (pin_check("123456", now) != 0) return "test: pin_check failed";
    if (confirm) auth_confirm(t->id, now);
    return m_authorize(t, now);
}

int main(void) {
    int64_t now = 1000; const char *err = NULL; char b[64];
    { uint8_t salt[16]; memset(salt, 5, 16); pin_set("123456", salt); }
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
    CHECK("PIN nao basta", authz(t, 0, now) != NULL && t->st == S_AWAITING_AUTH && m_balance(A_BRL) == 1100000);
    CHECK("swap confirmado", authz(t, 1, now) == NULL && t->st == S_CONFIRMED && t->hash[0] == 0);
    CHECK("BRL debitado", m_balance(A_BRL) == 1000000);
    CHECK("USDC creditado", m_balance(A_USDC) == 100000000 + 177946428);
    CHECK("reexecutar nao duplica", authz(t, 1, now) != NULL && m_balance(A_BRL) == 1000000);

    Quote q2; m_quote(A_BRL, A_USDC, 50000, now, &q2); Tx *t2 = m_prepare_swap(&q2, now, &err);
    CHECK("cotacao expira", authz(t2, 0, now + 21) != NULL && t2->st == S_EXPIRED);
    CHECK("expirado nao move", m_balance(A_BRL) == 1000000);
    CHECK("prepare com cotacao velha recusa", m_prepare_swap(&q2, now + 21, &err) == NULL);

    Tx *p = m_prepare_pix("Maria", 30000, now, &err);
    CHECK("pix aguarda auth", p && p->st == S_AWAITING_AUTH);
    CHECK("pix R$300 exige so PIN", m_required(p) == L_PIN);
    CHECK("pix confirmado", authz(p, 0, now) == NULL && p->st == S_CONFIRMED);
    CHECK("saldo apos pix", m_balance(A_BRL) == 970000);
    CHECK("limite por tx", m_prepare_pix("Maria", 250000, now, &err) == NULL);
    CHECK("saldo insuficiente", m_prepare_pix("Maria", 99999999, now, &err) == NULL);
    CHECK("valor zero", m_prepare_pix("Maria", 0, now, &err) == NULL);
    CHECK("sem destino", m_prepare_pix("", 100, now, &err) == NULL);

    M.pix_offline = 1;
    Tx *pf = m_prepare_pix("Maria", 1000, now, &err);
    CHECK("pix offline -> FAILED sem mover", authz(pf, 0, now) != NULL && pf->st == S_FAILED && m_balance(A_BRL) == 970000);
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
    CHECK("cooldown segura", authz(big, 1, now + 5) != NULL && big->st == S_AWAITING_AUTH);
    CHECK("cooldown passa", authz(big, 1, now + 31) == NULL && big->st == S_CONFIRMED);

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

    /* ---- loading is idempotent, and a state cut at ANY byte is accepted whole or refused whole ----
     * ---- carregar é idempotente, e um estado cortado em QUALQUER byte é aceito inteiro ou recusado inteiro ---- */
    { static char sv[200000], cut[200000];
      m_init(); m_load_demo(now); m_serialize(sv, sizeof sv);
      int ok1 = m_deserialize(sv); int n1 = M.n; int64_t b1 = m_balance(A_BRL), u1 = m_balance(A_USDC);
      int ok2 = m_deserialize(sv);
      CHECK("carregar duas vezes o mesmo estado da o mesmo resultado",
            ok1 && ok2 && M.n == n1 && m_balance(A_BRL) == b1 && m_balance(A_USDC) == u1);
      size_t len = strlen(sv); int bad_cut = 0, accepted = 0, refused = 0;
      for (size_t k = 0; k < len; k++) {
          memcpy(cut, sv, k); cut[k] = 0;
          int ok = m_deserialize(cut);
          if (ok) { accepted++; if (m_balance(A_BRL) < 0 || m_balance(A_USDC) < 0 || M.n > MAX_TX) bad_cut++; }
          else { refused++; if (M.n != 0 || !m_deserialize_error()[0]) bad_cut++; }
      }
      CHECK("estado cortado em cada byte: aceito inteiro ou recusado vazio, com motivo", bad_cut == 0 && refused > 0);
      (void)accepted; }

    m_deserialize("V1 1 0\n");
    fmt_money(b, sizeof b, A_BRL, 1248032); CHECK("format BRL", !strcmp(b, "R$ 12.480,32"));
    fmt_money(b, sizeof b, A_USDC, 177946428); CHECK("format USDC", !strcmp(b, "$ 177.94"));
    fmt_money(b, sizeof b, A_BRL, 5); CHECK("format 0,05", !strcmp(b, "R$ 0,05"));
    CHECK("parse digitos", parse_cents("100050") == 100050 && parse_cents("12a") == -1);

    /* ---- N07: the engine authorises only with a grant issued when a PIN is verified ----
     * ---- N07: o motor só autoriza com a concessão emitida quando um PIN é conferido ---- */
    m_init(); m_load_demo(now); M.pol.per_tx = 10000000; M.pol.daily = 10000000; auth_clear();
    { Tx *a = m_prepare_pix("Maria", 1000, now, &err), *c = m_prepare_pix("Joao", 2000, now, &err);
      CHECK("sem concessao nao autoriza", m_authorize(a, now) != NULL && a->st == S_AWAITING_AUTH && m_balance(A_BRL) == 1000000);
      auth_expect(a->id);
      CHECK("so esperar um PIN nao e concessao", m_authorize(a, now) != NULL && a->st == S_AWAITING_AUTH);
      CHECK("PIN errado nao emite concessao", pin_check("000000", now) == 1 && auth_peek(a->id, now) == L_NONE && m_authorize(a, now) != NULL);
      CHECK("PIN certo emite concessao L_PIN so para esta tx", pin_check("123456", now) == 0 && auth_peek(a->id, now) == L_PIN && auth_peek(c->id, now) == L_NONE);
      CHECK("concessao de uma tx nao autoriza outra", m_authorize(c, now) != NULL && c->st == S_AWAITING_AUTH);
      CHECK("concessao certa autoriza", m_authorize(a, now) == NULL && a->st == S_CONFIRMED);
      CHECK("uso unico: concessao some depois de usada", auth_peek(a->id, now) == L_NONE);
      CHECK("a outra tx continua sem poder", m_authorize(c, now) != NULL && c->st == S_AWAITING_AUTH);
      m_cancel(c, now); }
    { Tx *a = m_prepare_pix("Maria", 3000, now, &err);
      auth_expect(a->id); pin_check("123456", now);
      CHECK("concessao vence depois do prazo", auth_peek(a->id, now + AUTH_TTL_S) == L_PIN && auth_peek(a->id, now + AUTH_TTL_S + 1) == L_NONE);
      auth_expect(a->id); pin_check("123456", now);
      CHECK("concessao vencida nao autoriza", m_authorize(a, now + AUTH_TTL_S + 1) != NULL && a->st == S_AWAITING_AUTH);
      auth_expect(a->id); pin_check("123456", now + 50);
      CHECK("relogio que volta invalida a concessao", m_authorize(a, now + 10) != NULL && a->st == S_AWAITING_AUTH && auth_peek(a->id, now + 60) == L_NONE);
      auth_expect(a->id); pin_check("123456", now);
      auth_clear();
      CHECK("auth_clear (bloqueio de tela) descarta a concessao", auth_peek(a->id, now) == L_NONE && m_authorize(a, now) != NULL);
      auth_expect(a->id); pin_check("123456", now);
      auth_expect(a->id);
      CHECK("nova expectativa descarta a concessao anterior", auth_peek(a->id, now) == L_NONE);
      auth_clear();
      CHECK("PIN sem pagamento esperando (tela de bloqueio) nao emite nada", pin_check("123456", now) == 0 && auth_peek(a->id, now) == L_NONE);
      CHECK("auth_confirm sem concessao viva nao cria nada", auth_confirm(a->id, now) == 0 && auth_peek(a->id, now) == L_NONE);
      m_cancel(a, now); }
    { Quote q2; m_quote(A_BRL, A_USDC, 100000, now, &q2); Tx *w = m_prepare_swap(&q2, now, &err);
      CHECK("R$1000 exige CONFIRM (N07)", m_required(w) == L_CONFIRM);
      auth_expect(w->id); pin_check("123456", now);
      CHECK("so o PIN nao basta para nivel CONFIRM", m_authorize(w, now) != NULL && w->st == S_AWAITING_AUTH && auth_peek(w->id, now) == L_PIN);
      CHECK("a concessao segue valendo depois de uma recusa por nivel", auth_confirm(w->id, now) == 1 && auth_peek(w->id, now) == L_COOLDOWN);
      CHECK("PIN + confirmacao autorizam", m_authorize(w, now) == NULL && w->st == S_CONFIRMED); }
    { Tx *big2 = m_prepare_pix("Maria", 600000, now, &err);
      M.pol.per_tx = 10000000; M.pol.daily = 10000000; m_receive_pix(900000, "Joao", now);
      big2 = m_prepare_pix("Maria", 600000, now, &err);
      auth_expect(big2->id); pin_check("123456", now); auth_confirm(big2->id, now);
      CHECK("espera de cooldown segue valendo e a concessao sobrevive a ela", m_authorize(big2, now + 5) != NULL && auth_peek(big2->id, now + 5) == L_COOLDOWN);
      CHECK("depois da espera autoriza", m_authorize(big2, now + 31) == NULL && big2->st == S_CONFIRMED);
      CHECK("a espera cabe na vida da concessao", M.pol.cooldown_s > 0 && M.pol.cooldown_s < AUTH_TTL_S); }
    { m_init(); m_load_demo(now); M.pol.per_tx = 10000000; M.pol.daily = 10000000;
      Tx *a = m_prepare_pix("Maria", 1000, now, &err); auth_expect(a->id); pin_check("123456", now);
      M.emergency = 1;
      CHECK("emergencia nega e consome a concessao", m_authorize(a, now) != NULL && a->st == S_FAILED && auth_peek(a->id, now) == L_NONE);
      M.emergency = 0; auth_clear(); }
    { m_init(); m_load_demo(now);
      Tx *a = m_prepare_pix("Maria", 1000, now, &err);
      /* the balance falls below the amount before it is authorised / o saldo cai abaixo do valor antes de autorizar */
      for (int i = 0; i < M.n; i++) if (M.tx[i].type == T_FUNDING && M.tx[i].to == A_BRL) M.tx[i].st = S_FAILED;
      auth_expect(a->id); pin_check("123456", now);
      CHECK("saldo insuficiente nega e consome a concessao", m_balance(A_BRL) < 1000 && m_authorize(a, now) != NULL && a->st == S_FAILED && auth_peek(a->id, now) == L_NONE); }


    /* ---- N11: arithmetic limits. Reproduced on 32932dd with UBSan before the fix (see docs/review/MONEY_INVARIANTS.md).
     * ---- N11: limites aritméticos. Reproduzido no 32932dd com UBSan antes da correção. ---- */
    m_init();
    { Quote q;
      /* BRL -> USDC: the product (amount - fee) * 1e6 must never be computed above the functional cap.
       * BRL -> USDC: o produto (valor - tarifa) * 1e6 nunca pode ser calculado acima do teto funcional. */
      CHECK("swap BRL no teto funcional e aceito", m_quote(A_BRL, A_USDC, MAX_SWAP_BRL, now, &q) && q.ok);
      CHECK("swap BRL no teto: saida exata", q.to_amt == (MAX_SWAP_BRL - 350) * 1000000LL / 560);
      CHECK("swap BRL no teto: a saida cabe no que o carregador aceita", q.to_amt > 0 && q.to_amt <= MAX_AMOUNT);
      CHECK("swap BRL um centavo acima do teto e recusado", !m_quote(A_BRL, A_USDC, MAX_SWAP_BRL + 1, now, &q) && !q.ok);
      CHECK("swap BRL no limite do int64 (9223372036855 + tarifa) e recusado", !m_quote(A_BRL, A_USDC, 9223372036855LL + 350, now, &q));
      CHECK("swap BRL de 1e15 (teto de sanidade) e recusado", !m_quote(A_BRL, A_USDC, 1000000000000000LL, now, &q));
      CHECK("swap BRL INT64_MAX e recusado", !m_quote(A_BRL, A_USDC, INT64_MAX, now, &q));
      CHECK("swap BRL negativo e recusado", !m_quote(A_BRL, A_USDC, -1, now, &q) && !m_quote(A_BRL, A_USDC, INT64_MIN, now, &q));
      CHECK("swap BRL zero e igual a tarifa sao recusados", !m_quote(A_BRL, A_USDC, 0, now, &q) && !m_quote(A_BRL, A_USDC, 350, now, &q));
      CHECK("swap BRL logo acima da tarifa e aceito", m_quote(A_BRL, A_USDC, 351, now, &q) && q.to_amt == 1 * 1000000LL / 560);
      /* USDC -> BRL */
      CHECK("swap USDC no teto funcional e aceito", m_quote(A_USDC, A_BRL, MAX_SWAP_USDC, now, &q) && q.ok);
      CHECK("swap USDC no teto: saida exata", q.to_amt == MAX_SWAP_USDC / 1000000LL * 560 - 350);
      CHECK("swap USDC no teto: a saida cabe no que o carregador aceita", q.to_amt > 0 && q.to_amt <= MAX_AMOUNT);
      CHECK("swap USDC uma unidade acima do teto e recusado", !m_quote(A_USDC, A_BRL, MAX_SWAP_USDC + 1, now, &q) && !q.ok);
      CHECK("swap USDC onde amt*560 estoura o int64 e recusado", !m_quote(A_USDC, A_BRL, 16469827927052811LL, now, &q));
      CHECK("swap USDC INT64_MAX e recusado", !m_quote(A_USDC, A_BRL, INT64_MAX, now, &q));
      CHECK("swap USDC negativo e zero sao recusados", !m_quote(A_USDC, A_BRL, -1, now, &q) && !m_quote(A_USDC, A_BRL, INT64_MIN, now, &q) && !m_quote(A_USDC, A_BRL, 0, now, &q));
      CHECK("swap entre a mesma moeda e recusado", !m_quote(A_BRL, A_BRL, 1000, now, &q) && !m_quote(A_USDC, A_USDC, 1000, now, &q));
      CHECK("recusa nao gasta numero de cotacao", (m_quote(A_BRL, A_USDC, MAX_SWAP_BRL + 1, now, &q), q.id[0] == 0)); }

    /* The engine's own output must be accepted by its own loader (closure). A swap at the cap, end to end.
     * A saida do motor tem que ser aceita pelo proprio carregador. Um swap no teto, de ponta a ponta. */
    { m_init(); m_load_demo(now); M.pol.pin_max = M.pol.confirm_max = M.pol.per_tx = M.pol.daily = INT64_C(1) << 60;
      m_receive_pix(MAX_SWAP_BRL, "x", now);
      Quote q; const char *e = NULL; M.pol.cooldown_s = 1;   /* the 30 s cooldown does not fit in the 20 s quote lifetime: see MONEY_INVARIANTS.md */
      CHECK("swap no teto: cotacao ok", m_quote(A_BRL, A_USDC, MAX_SWAP_BRL, now, &q));
      Tx *t = m_prepare_swap(&q, now, &e);
      CHECK("swap no teto: preparado", t != NULL);
      if (t) { auth_expect(t->id); pin_check("123456", now); auth_confirm(t->id, now);
               CHECK("swap no teto: confirmado", m_authorize(t, now + 2) == NULL && t->st == S_CONFIRMED); }
      static char big[200000]; m_serialize(big, sizeof big);
      int64_t brl = m_balance(A_BRL), usdc = m_balance(A_USDC);
      CHECK("swap no teto: o carregador aceita o estado que o motor gravou", m_deserialize(big));
      CHECK("swap no teto: saldos iguais depois de recarregar", m_balance(A_BRL) == brl && m_balance(A_USDC) == usdc); }

    /* m_total_brl: the USDC balance times the rate must not overflow, whatever a validated state holds.
     * m_total_brl: saldo em USDC vezes a taxa nao pode estourar, seja qual for o estado validado. */
    { static char st[40000]; char *p;
      m_init(); strcpy(st, "V1 1 0\n"); p = st + strlen(st);
      for (int i = 0; i < 17; i++) p += sprintf(p, "T|id%d|k%d|0|7|-1|1|0|1000000000000000|0|1|DEMO||\n", i, i);
      CHECK("17 financiamentos de 1e15 micro-USDC passam pelo carregador", m_deserialize(st) && m_balance(A_USDC) == 17000000000000000LL);
      CHECK("m_total_brl sem estouro: 1.7e16 micro-USDC = 9.52e12 centavos", m_total_brl() == 9520000000000LL);
      m_init(); strcpy(st, "V1 1 0\n"); p = st + strlen(st);
      for (int i = 0; i < 200; i++) p += sprintf(p, "T|a%d|ka%d|0|7|-1|0|0|1000000000000000|0|1|DEMO||\n", i, i);
      for (int i = 0; i < 200; i++) p += sprintf(p, "T|b%d|kb%d|0|7|-1|1|0|1000000000000000|0|1|DEMO||\n", i, i);
      CHECK("400 linhas de 1e15 passam pelo carregador (o maximo)", m_deserialize(st) && M.n == 400);
      CHECK("saldo maximo possivel: BRL 2e17 e USDC 2e17", m_balance(A_BRL) == 200000000000000000LL && m_balance(A_USDC) == 200000000000000000LL);
      CHECK("m_total_brl no saldo maximo: 2e17 + 1.12e14", m_total_brl() == 200112000000000000LL);
      m_init(); strcpy(st, "V1 1 0\n"); p = st + strlen(st);
      p += sprintf(p, "T|u1|ku1|0|7|-1|1|0|1999999|0|1|DEMO||\n");
      m_deserialize(st);
      CHECK("m_total_brl trunca como antes: 1999999 micro-USDC = 1119 centavos", m_total_brl() == 1119);
      m_init(); m_load_demo(now);
      CHECK("m_total_brl no demo: R$ 10.000 + 100 USDC = 1056000", m_total_brl() == 1000000 + 56000); }


    /* ---- N11: the engine does not trust a Quote the caller built. Reproduced on 32932dd: a negative from_amt created
     *      money, an inflated to_amt paid out 1e12 micro-USDC for R$ 1000, a one-year expiry never expired.
     * ---- N11: o motor não confia numa Quote montada pelo chamador. Reproduzido no 32932dd: from_amt negativo criava
     *      dinheiro, to_amt inflado pagava 1e12 micro-USDC por R$ 1000, validade de um ano nunca expirava. ---- */
    { const char *e = NULL; int64_t t0 = 1000;
      const int64_t good_out = (100000 - 350) * 1000000LL / 560;
      struct { const char *name; Quote q; } F[] = {
        { "from_amt negativo",            { .from = A_BRL,  .to = A_USDC, .from_amt = -100000, .to_amt = 100, .rate = 560, .fee_brl = 350, .expires = t0 + 20, .ok = 1 } },
        { "from_amt zero",                { .from = A_BRL,  .to = A_USDC, .from_amt = 0, .to_amt = 0, .rate = 560, .fee_brl = 350, .expires = t0 + 20, .ok = 1 } },
        { "to_amt inflado",               { .from = A_BRL,  .to = A_USDC, .from_amt = 100000, .to_amt = 1000000000000LL, .rate = 560, .fee_brl = 350, .expires = t0 + 20, .ok = 1 } },
        { "to_amt reduzido",              { .from = A_BRL,  .to = A_USDC, .from_amt = 100000, .to_amt = good_out - 1, .rate = 560, .fee_brl = 350, .expires = t0 + 20, .ok = 1 } },
        { "to_amt negativo",              { .from = A_BRL,  .to = A_USDC, .from_amt = 100000, .to_amt = -5, .rate = 560, .fee_brl = 350, .expires = t0 + 20, .ok = 1 } },
        { "taxa trocada",                 { .from = A_BRL,  .to = A_USDC, .from_amt = 100000, .to_amt = good_out, .rate = 1, .fee_brl = 350, .expires = t0 + 20, .ok = 1 } },
        { "tarifa trocada",               { .from = A_BRL,  .to = A_USDC, .from_amt = 100000, .to_amt = good_out, .rate = 560, .fee_brl = 0, .expires = t0 + 20, .ok = 1 } },
        { "validade de um ano",           { .from = A_BRL,  .to = A_USDC, .from_amt = 100000, .to_amt = good_out, .rate = 560, .fee_brl = 350, .expires = t0 + 31536000, .ok = 1 } },
        { "BRL para BRL",                 { .from = A_BRL,  .to = A_BRL,  .from_amt = 100000, .to_amt = 100000, .rate = 560, .fee_brl = 350, .expires = t0 + 20, .ok = 1 } },
        { "moeda inexistente de origem",  { .from = 7,      .to = A_USDC, .from_amt = 100000, .to_amt = 5, .rate = 560, .fee_brl = 350, .expires = t0 + 20, .ok = 1 } },
        { "moeda de destino none",        { .from = A_BRL,  .to = A_NONE, .from_amt = 100000, .to_amt = 5, .rate = 560, .fee_brl = 350, .expires = t0 + 20, .ok = 1 } },
        { "acima do teto de swap",        { .from = A_BRL,  .to = A_USDC, .from_amt = MAX_SWAP_BRL + 1, .to_amt = 1, .rate = 560, .fee_brl = 350, .expires = t0 + 20, .ok = 1 } },
        { "cotacao marcada como invalida",{ .from = A_BRL,  .to = A_USDC, .from_amt = 100000, .to_amt = good_out, .rate = 560, .fee_brl = 350, .expires = t0 + 20, .ok = 0 } },
      };
      for (unsigned i = 0; i < sizeof F / sizeof *F; i++) {
          m_init(); m_load_demo(t0); M.pol.cooldown_s = 0; M.pol.pin_max = M.pol.confirm_max = INT64_C(1) << 50; M.pol.per_tx = M.pol.daily = INT64_C(1) << 50;
          int64_t b0 = m_balance(A_BRL), u0 = m_balance(A_USDC); int n0 = M.n; char nm[96];
          Tx *tx = m_prepare_swap(&F[i].q, t0, &e);
          snprintf(nm, sizeof nm, "cotacao forjada recusada: %s", F[i].name);
          CHECK(nm, tx == NULL && e && e[0]);
          snprintf(nm, sizeof nm, "cotacao forjada nao cria transacao nem mexe nos saldos: %s", F[i].name);
          CHECK(nm, M.n == n0 && m_balance(A_BRL) == b0 && m_balance(A_USDC) == u0);
      }
      /* the control: an honest quote still goes through and moves exactly what it says / o controle: a cotação honesta passa */
      m_init(); m_load_demo(t0); M.pol.cooldown_s = 0; M.pol.pin_max = M.pol.confirm_max = INT64_C(1) << 50;
      Quote hq; CHECK("cotacao honesta: m_quote", m_quote(A_BRL, A_USDC, 100000, t0, &hq));
      Tx *ht = m_prepare_swap(&hq, t0, &e);
      CHECK("cotacao honesta: preparada", ht != NULL);
      if (ht) { auth_expect(ht->id); pin_check("123456", t0 + 1); auth_confirm(ht->id, t0 + 1);
                CHECK("cotacao honesta: autorizada e confirmada", m_authorize(ht, t0 + 1) == NULL && ht->st == S_CONFIRMED);
                CHECK("cotacao honesta: moveu exatamente o cotado", m_balance(A_BRL) == 1000000 - 100000 && m_balance(A_USDC) == 100000000 + good_out); }
      /* an expiry earlier than the engine's lifetime is allowed (shorter is safe) / validade menor que a do motor é permitida */
      m_init(); m_load_demo(t0); m_quote(A_BRL, A_USDC, 100000, t0, &hq); hq.expires = t0 + 5;
      CHECK("validade menor que a do motor e aceita", m_prepare_swap(&hq, t0, &e) != NULL); }

    printf(fails ? "%d FALHAS\n" : "TODOS OK\n", fails);
    return fails != 0;
}
