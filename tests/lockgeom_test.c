/* Layout test of the lock / PIN screen (issue #18). No drawing: only rectangles.
 * Teste de layout da tela de bloqueio / PIN (issue #18). Sem desenho: só retângulos. */
#include "../init/lockgeom.h"
#include <stdio.h>
int th_W, th_H, th_SC;                      /* normally defined in components.c / normalmente definidos no components.c */
static int fails;
#define CHECK(n, c) do { int ok_ = (c); printf("%s %s\n", ok_ ? "PASS" : "FAIL", n); if (!ok_) fails++; } while (0)

int main(void) {
    /* 1. the bug, reproduced with the OLD formulas on the real screen (720x1612)
     *    o bug, reproduzido com as fórmulas ANTIGAS na tela real (720x1612) */
    th_init(720, 1612);
    { int old_keypad_bottom = th_H - TH_DP(TH_SP4);               /* keypad ended 16 dp above the bottom / teclado terminava 16 dp acima do fundo */
      int old_footer_top = th_H - TH_DP(TH_SP5);                  /* footer started 24 dp above the bottom / rodapé começava 24 dp acima do fundo */
      CHECK("o layout ANTIGO sobrepunha o rodape ao teclado em 720x1612", old_footer_top < old_keypad_bottom); }

    /* 2. the new layout, on the real screen and on other sizes the system or the preview may use
     *    o layout novo, na tela real e em outros tamanhos que o sistema ou a prévia podem usar */
    static const int sizes[][2] = { {720, 1612}, {1080, 2400}, {720, 1280}, {720, 1600}, {480, 854}, {360, 640} };
    for (unsigned i = 0; i < sizeof sizes / sizeof *sizes; i++) {
        th_init(sizes[i][0], sizes[i][1]);
        LockGeom g = lock_geom(); char n[96];
        int kb = g.keypad_y + g.keypad_h;
        snprintf(n, sizeof n, "%dx%d: o teclado termina antes do rodape (com folga)", sizes[i][0], sizes[i][1]);
        CHECK(n, kb + TH_DP(TH_SP3) <= g.footer_y);
        snprintf(n, sizeof n, "%dx%d: o rodape cabe na tela", sizes[i][0], sizes[i][1]);
        CHECK(n, g.footer_y >= 0 && g.footer_y + g.footer_h <= th_H);
        snprintf(n, sizeof n, "%dx%d: o teclado nao cobre os pontinhos nem o texto de cima", sizes[i][0], sizes[i][1]);
        CHECK(n, g.keypad_y > g.content_bottom);
        snprintf(n, sizeof n, "%dx%d: o teclado tem a altura do comp_keypad", sizes[i][0], sizes[i][1]);
        CHECK(n, g.keypad_h == 4 * TH_DP(TH_H_TECLA) + 3 * TH_DP(TH_SP3));
    }
    /* 3. the touch target of every key stays at least 44 dp / o alvo de toque das teclas segue com pelo menos 44 dp */
    CHECK("tecla com pelo menos 44 dp", TH_H_TECLA >= TH_TOUCH_MIN_DP);

    printf(fails ? "%d FALHAS\n" : "TODOS OK\n", fails);
    return fails != 0;
}
