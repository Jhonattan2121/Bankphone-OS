/* Regression tests that drive the REAL UI code (ui.c, screens.c, sheets.c, components.c) with fake platform hooks and an in-memory screen.
 * Nothing is drawn to a device; a tap is found in the hit table and sent through ui_touch(), as a finger would.
 * Testes de regressão que dirigem o código REAL da interface (ui.c, screens.c, sheets.c, components.c) com ganchos de plataforma falsos e uma
 * tela em memória. Nada vai a um aparelho; o toque é achado na tabela de alvos e enviado por ui_touch(), como um dedo faria.
 * Each PASS/FAIL line is one check. / Cada linha PASS/FAIL é uma verificação. */
#define _GNU_SOURCE
#include "ui.h"
#include "nav.h"
#include "components.h"
#include "money.h"
#include "sec.h"
#include "hw.h"
#include "store.h"
#include "amounts.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
#define CHECK(n, c) do { int ok_ = (c); printf("%s %s\n", ok_ ? "PASS" : "FAIL", n); if (!ok_) fails++; } while (0)

/* ---- platform hooks the system normally gets from main.c or host.c / ganchos de plataforma ---- */
static int64_t g_now = 1759468800;
int64_t plat_now(void) { return g_now; }
int plat_clock_valid(void) { return 1; }
void plat_random(uint8_t *b, int n) { for (int i = 0; i < n; i++) b[i] = (uint8_t)(i * 37 + 11); }   /* void on this base; int once the entropy PR lands / void nesta base; int quando o PR da entropia entrar */
void plat_save(void) {}
int plat_persist_ok(void) { return 0; }
const char *plat_boot_prop(const char *k) { (void)k; return ""; }
void bd_log(const char *fmt, ...) { (void)fmt; }
void bd_flush(void) {}
int bd_serial_fd(void) { return -1; }

static unsigned char *read_all(const char *p, long *n) {
    FILE *f = fopen(p, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); *n = ftell(f); rewind(f);
    unsigned char *b = malloc((size_t)*n);
    if (fread(b, 1, (size_t)*n, f) != (size_t)*n) { fclose(f); free(b); return NULL; }
    fclose(f); return b;
}
static void tap(int act, int arg) {
    ui_draw();
    int x, y;
    if (!ui_hit_find(act, arg, &x, &y)) return;
    ui_touch(0, x, y); ui_touch(2, x, y);
}
static int has(int act, int arg) { int x, y; return ui_hit_find(act, arg, &x, &y); }
static void digits(const char *d) { for (; *d; d++) tap(ACT_KEY, *d); }
static void fresh_ui(int w, int h) {
    ui_init(w, h);
    uint8_t salt[16]; plat_random(salt, 16); pin_set("123456", salt);
    ui_lock(); digits("123456");
    m_load_demo(g_now);
}

int main(void) {
    const int W = 720, H = 1612;
    uint32_t *px = calloc((size_t)W * H, 4);
    Surf s = { px, W, H, W };
    long tn = 0; unsigned char *ttf = read_all("assets/Roboto-Regular.ttf", &tn);
    if (!ttf || gfx_init(ttf)) { puts("FAIL font assets/Roboto-Regular.ttf not found (run from the repo root)"); return 1; }
    gfx_set(s); hw_init();

    /* ---- 1. the amount typed in the swap sheet. sheet_amt_value() returned int: a USDC amount from 214749 digits (US$ 2,147.49) wrapped.
     *         Reproduced on 32932dd: 214749 showed "-$ 2147.47" and 429597 showed "$ 1.00" while the person had typed US$ 4,295.97.
     * ---- 1. o valor digitado na folha de troca. sheet_amt_value() devolvia int: um valor em USDC a partir de 214749 dígitos (US$ 2.147,49)
     *         dava a volta. Reproduzido no 32932dd: 214749 mostrava "-$ 2147.47" e 429597 mostrava "$ 1.00" quando a pessoa digitou US$ 4.295,97. */
    fresh_ui(W, H);
    tap(ACT_SHEET, SH_AMOUNT); tap(ACT_FLIP, 0);
    CHECK("a troca esta em USDC depois de inverter", SH.swap_from == A_USDC);
    { static const struct { const char *d; int64_t micro; } C[] = {
        { "100", 1000000LL }, { "214748", 2147480000LL }, { "214749", 2147490000LL }, { "214750", 2147500000LL },
        { "429597", 4295970000LL }, { "500000", 5000000000LL }, { "999999", 9999990000LL } };
      for (unsigned i = 0; i < sizeof C / sizeof *C; i++) {
          SH.amt[0] = 0;
          digits(C[i].d); char nm[96];
          int64_t v = sheet_amt_value(A_USDC);
          snprintf(nm, sizeof nm, "USDC digitado %s centavos = %lld micro (sem dar a volta no int)", C[i].d, (long long)C[i].micro);
          CHECK(nm, v == C[i].micro);
      } }
    SH.amt[0] = 0; digits("999999999");
    CHECK("USDC no maximo do teclado (9 digitos) cabe em int64", sheet_amt_value(A_USDC) == 9999999990000LL);
    CHECK("BRL no maximo do teclado segue exato", (SH.swap_from = A_BRL, sheet_amt_value(A_BRL) == 999999999));
    SH.amt[0] = 0; CHECK("vazio vale zero", sheet_amt_value(A_USDC) == 0 && sheet_amt_value(A_BRL) == 0);

    /* ---- 2. the hit table. It had 220 slots and the Activity tab registers one per ledger row FIRST, then the status bar and the navigation bar:
     *         with 216 rows the tab bar was gone and the person could not leave the screen until reboot (no error). Reproduced on 32932dd.
     * ---- 2. a tabela de alvos. Tinha 220 vagas e a aba Atividade registra um por linha do livro-razão PRIMEIRO, depois a barra de status e a de
     *         navegação: com 216 linhas a barra de abas sumia e a pessoa não saía da tela até reiniciar (sem erro). Reproduzido no 32932dd. */
    { static char st[200000]; size_t o = 0; int N = MAX_TX;
      o += (size_t)snprintf(st + o, sizeof st - o, "V1 1 0\n");
      for (int i = 0; i < N; i++) o += (size_t)snprintf(st + o, sizeof st - o, "T|id%d|f%d|1|7|-1|0|0|100|0|%d|Maria||\n", i, i, 1000 + i);
      fresh_ui(W, H); m_deserialize(st);
      CHECK("o livro-razao de 400 linhas carregou", M.n == MAX_TX);
      tap(ACT_TAB, SC_ACTIVITY); ui_draw();
      int tabs = 1; for (int t = 0; t < 4; t++) tabs &= has(ACT_TAB, t);
      CHECK("com 400 linhas a barra de abas continua clicavel (as 4 abas)", tabs);
      CHECK("com 400 linhas a engrenagem de configuracoes continua clicavel", has(ACT_PUSH, SC_SETTINGS));
      CHECK("com 400 linhas a ultima linha e a primeira sao clicaveis", has(ACT_TX, 0) && has(ACT_TX, MAX_TX - 1));
      tap(ACT_TAB, SC_MONEY); ui_draw();
      CHECK("a pessoa consegue sair da aba Atividade (a linha 0 so existe na aba Atividade)", !has(ACT_TX, 0) && has(ACT_TAB, SC_ACTIVITY)); }
    for (int rows = 200; rows <= 400; rows += 50) {
        static char st[200000]; size_t o = 0; char nm[96];
        o += (size_t)snprintf(st + o, sizeof st - o, "V1 1 0\n");
        for (int i = 0; i < rows; i++) o += (size_t)snprintf(st + o, sizeof st - o, "T|id%d|f%d|1|7|-1|0|0|100|0|%d|Maria||\n", i, i, 1000 + i);
        fresh_ui(W, H); m_deserialize(st); tap(ACT_TAB, SC_ACTIVITY); ui_draw();
        int tabs = 1; for (int t = 0; t < 4; t++) tabs &= has(ACT_TAB, t);
        snprintf(nm, sizeof nm, "%d linhas: as 4 abas e a engrenagem seguem clicaveis", rows);
        CHECK(nm, tabs && has(ACT_PUSH, SC_SETTINGS));
    }

    /* ---- 3. the PIN sheet during a lockout. SH.locked_s was never assigned, so the sheet showed no countdown and silently swallowed the
     *         correct PIN. Reproduced on 32932dd. / a folha do PIN durante um bloqueio. SH.locked_s nunca era atribuído, então a folha não
     *         mostrava contagem e engolia o PIN certo em silêncio. Reproduzido no 32932dd. */
    fresh_ui(W, H);
    tap(ACT_SHEET, SH_AMOUNT); digits("10000"); tap(ACT_CONT, 0); tap(ACT_AUTH, 0);
    for (int i = 0; i < 5; i++) digits("000000");
    CHECK("5 PINs errados na folha: o motor bloqueou por 30 s", PIN.fails >= 5 && pin_locked_s(plat_now()) == 30);
    ui_draw();
    CHECK("a folha do PIN mostra a contagem do bloqueio (SH.locked_s)", SH.locked_s == 30);
    g_now += 12; ui_draw();
    CHECK("a contagem desce com o tempo", SH.locked_s == 18);
    g_now += 100; ui_draw();
    CHECK("passado o bloqueio a contagem some", SH.locked_s == 0);

    /* ---- 4. the approximate USDC total on the Money tab. brl_cents * 1000000 overflowed above R$ 92 billion. / o total aproximado em USDC. ---- */
    { int64_t out = 0; int ok;
      ok = usdc_micro_from_brl(0, &out);                CHECK("0 centavos = 0 micro", ok && out == 0);
      ok = usdc_micro_from_brl(560, &out);              CHECK("560 centavos (R$ 5,60) = 1 USDC", ok && out == 1000000);
      ok = usdc_micro_from_brl(1000000, &out);          CHECK("R$ 10.000 = 1785714285 micro (trunca como antes)", ok && out == 1785714285LL);
      ok = usdc_micro_from_brl(9223372036854LL, &out);  CHECK("9223372036854 centavos (o ultimo valor que a conta antiga aguentava) segue exato", ok && out == 16470307208667857LL);
      ok = usdc_micro_from_brl(9223372036855LL, &out);  CHECK("9223372036855 centavos (onde a conta antiga estourava) agora e exato e positivo", ok && out > 16470307208667857LL);
      ok = usdc_micro_from_brl(1000000000000000LL, &out); CHECK("1e15 centavos (o teto de sanidade) cabe", ok && out == 1785714285714285714LL);
      ok = usdc_micro_from_brl(5160000000000000LL, &out); CHECK("5.16e15 centavos ainda cabe no int64", ok && out > 0);
      ok = usdc_micro_from_brl(5200000000000000LL, &out); CHECK("acima do que cabe: recusa em vez de estourar", !ok);
      ok = usdc_micro_from_brl(INT64_MAX, &out);        CHECK("INT64_MAX: recusa", !ok);
      ok = usdc_micro_from_brl(-1, &out);               CHECK("negativo: recusa", !ok);
      ok = usdc_micro_from_brl(INT64_MIN, &out);        CHECK("INT64_MIN: recusa", !ok);
      int64_t prev = 0; int mono = 1;                   /* never goes down as the amount goes up / nunca desce quando o valor sobe */
      for (int64_t b = 0; b < 3000; b += 7) { usdc_micro_from_brl(b, &out); if (out < prev) mono = 0; prev = out; }
      CHECK("monotonico e igual a conta direta nos valores pequenos", mono);
      int same = 1; for (int64_t b = 0; b < 100000; b += 13) { usdc_micro_from_brl(b, &out); if (out != b * 1000000 / 560) same = 0; }
      CHECK("igual a b*1000000/560 em todo valor onde a conta direta e segura", same); }
    fresh_ui(W, H);
    { static char st[512]; snprintf(st, sizeof st, "V1 1 0\nT|id1|fund1|0|7|-1|0|0|9223372036855|0|1000|DEMO||\n");
      m_deserialize(st); tap(ACT_TAB, SC_MONEY); ui_draw();
      CHECK("a aba Dinheiro desenha o total acima de R$ 92 bilhoes sem estourar (o teste roda com UBSan no make test-asan)", M.n == 1); }

    /* ---- 5. a number shown to the person must not be dropped by its own format: "%.0d" prints nothing for 0 (the stride showed as "stride= B").
     *         A source guard, because the diagnostics screen needs a real framebuffer to show it. / "%.0d" não imprime nada para 0 (o stride aparecia "stride= B"). ---- */
    { FILE *f = fopen("init/screens.c", "rb"); static char src[200000]; size_t n = f ? fread(src, 1, sizeof src - 1, f) : 0; if (f) fclose(f); src[n] = 0;
      CHECK("init/screens.c nao usa '%.0d' (que apaga o numero zero)", n > 0 && !strstr(src, "%.0d")); }

    free(px);
    puts(fails ? "FALHAS" : "TODOS OK");
    return fails != 0;
}
