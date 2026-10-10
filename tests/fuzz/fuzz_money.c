/* Fuzzing of the money engine (init/money.c).
 *
 * The first input byte picks the mode:
 *   even -> a sequence of operations (receive, prepare Pix, quote, swap, authorize, cancel,
 *           advance time, emergency mode...) with invariants checked after each step;
 *   odd  -> the rest of the input is treated as a saved-state file and handed to
 *           m_deserialize(), then everything that reads that state is exercised.
 *
 * Amounts follow the callers' contract: in the UI the value comes from parse_cents(), which
 * only returns 0 to 100000000000 cents.
 *
 * ---- Português ----
 * Fuzzing do motor de dinheiro (init/money.c).
 *
 * Primeiro byte da entrada escolhe o modo:
 *   par   -> sequência de operações (receber, preparar Pix, cotar, trocar, autorizar,
 *            cancelar, avançar o tempo, modo de emergência...) com invariantes checadas;
 *   ímpar -> o resto da entrada é tratado como arquivo de estado salvo e entregue a
 *            m_deserialize(), depois se exercita o que lê esse estado.
 *
 * Valores seguem o contrato de quem chama: na interface, o valor vem de parse_cents(),
 * que só devolve de 0 a 100000000000 centavos. */
#include "money.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define NEED(c) do { if (!(c)) abort(); } while (0)
static int terminal(TxState s) { return s == S_CONFIRMED || s == S_FAILED || s == S_CANCELLED || s == S_EXPIRED; }

static void check_invariants(int64_t now, const TxState *prev, int prev_n) {
    NEED(M.n >= 0 && M.n <= MAX_TX);
    NEED(m_balance(A_BRL) >= 0);                                  /* never negative / nunca fica negativo */
    NEED(m_balance(A_USDC) >= 0);
    NEED(m_pix_spent_since(now - 86400) <= M.pol.daily);          /* daily cap / teto diário */
    for (int i = 0; i < M.n; i++) {
        const Tx *t = &M.tx[i];
        if (t->type == T_PIX_OUT) NEED(t->from_amt <= M.pol.per_tx);   /* per-Pix cap / teto por Pix */
        if (i < prev_n && terminal(prev[i])) NEED(t->st == prev[i]);   /* a final state never changes / estado final não volta */
    }
}

static void exercise_state_readers(void) {
    char out[96], big[200000];
    for (int a = 0; a < A_N; a++) { fmt_money(out, sizeof out, a, m_balance(a)); NEED(out[0]); }
    (void)m_total_brl();
    for (int i = 0; i < M.n; i++) NEED(m_state_name(M.tx[i].st)[0]);
    NEED(m_serialize(big, sizeof big) < sizeof big);
}

static int64_t amount(const uint8_t *d, size_t n, size_t *i) {
    uint64_t v = 0; for (int k = 0; k < 5 && *i < n; k++) v = (v << 8) | d[(*i)++];
    return (int64_t)(v % 100000000001ull);                        /* 0..parse_cents maximum / 0..máximo do parse_cents */
}

static void run_ops(const uint8_t *d, size_t n) {
    static TxState prev[MAX_TX];
    m_init();
    int prev_n = 0;
    int64_t now = 1000; Quote q; memset(&q, 0, sizeof q); int have_q = 0;
    size_t i = 0;
    while (i < n) {
        int op = d[i++] % 9; const char *err = NULL; Tx *t;
        int idx = (i < n && M.n) ? d[i] % M.n : 0;
        switch (op) {
        case 0: m_receive_pix(amount(d, n, &i), "Joao", now); break;
        case 1: m_prepare_pix(i < n && (d[i++] & 1) ? "Maria" : "ab", amount(d, n, &i), now, &err); break;
        case 2: { int f = i < n ? d[i++] % 3 - 1 : 0, to = i < n ? d[i++] % 3 - 1 : 0;
                  have_q = m_quote(f, to, amount(d, n, &i), now, &q); } break;
        case 3: if (have_q) m_prepare_swap(&q, now, &err); break;
        case 4: if (M.n) { i++; t = &M.tx[idx]; m_authorize(t, (Level)(1 + (i < n ? d[i++] % 3 : 0)), now); } break;
        case 5: if (M.n) { i++; m_cancel(&M.tx[idx], now); } break;
        case 6: now += i < n ? (int64_t)d[i++] * 7 : 1; break;
        case 7: if (i < n) { uint8_t f = d[i++]; M.emergency = f & 1; M.pix_offline = (f >> 1) & 1; } break;
        case 8: { int before = M.n; m_load_demo(now); int mid = M.n; m_load_demo(now); NEED(M.n == mid); (void)before; } break;
        }
        check_invariants(now, prev, prev_n);
        prev_n = M.n;
        for (int k = 0; k < M.n; k++) prev[k] = M.tx[k].st;
    }
    /* saving and reloading keeps the balances / salvar e recarregar mantém os saldos */
    int64_t brl = m_balance(A_BRL), usdc = m_balance(A_USDC);
    static char buf[200000];
    m_serialize(buf, sizeof buf);
    NEED(m_deserialize(buf));
    NEED(m_balance(A_BRL) == brl && m_balance(A_USDC) == usdc);
}

int LLVMFuzzerTestOneInput(const uint8_t *d, size_t n) {
    if (n < 1 || n > 4096) return 0;
    if (d[0] & 1) {
        char *s = malloc(n); memcpy(s, d + 1, n - 1); s[n - 1] = 0;
        int ok = m_deserialize(s); free(s);
        /* tudo-ou-nada: recusado => vazio; aceito => saldos não negativos, estados finais, tetos de valor */
        /* all-or-nothing: refused => empty; accepted => balances >= 0, valid states, amount caps */
        if (!ok) { NEED(M.n == 0); NEED(m_deserialize_error()[0] != 0); }
        else { NEED(m_balance(A_BRL) >= 0 && m_balance(A_USDC) >= 0); NEED(M.n <= MAX_TX);
               for (int i = 0; i < M.n; i++) { NEED(M.tx[i].from_amt >= 0 && M.tx[i].to_amt >= 0); NEED(M.tx[i].st != S_CREATED && M.tx[i].st != S_QUOTED && M.tx[i].st != S_AWAITING_AUTH); } }
        exercise_state_readers();
    } else run_ops(d + 1, n - 1);
    return 0;
}
