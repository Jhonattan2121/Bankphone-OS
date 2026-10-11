/* BANKPHONE OS: vertical layout of the lock / PIN screen. No drawing, so it can be tested on a computer.
 * BANKPHONE OS: layout vertical da tela de bloqueio / PIN. Sem desenho, para poder ser testado no computador.
 *
 * Issue #18: the footer line was drawn at (height - 24 dp) while the keypad ended at (height - 16 dp), so the
 * footer overlapped the bottom of the "0" key. Now the footer box sits at the bottom and the keypad ends above it.
 * Issue #18: a linha do rodapé era desenhada em (altura - 24 dp) e o teclado terminava em (altura - 16 dp), então
 * o rodapé sobrepunha a parte de baixo da tecla "0". Agora o rodapé fica embaixo e o teclado termina acima dele.
 *
 * Needs theme.h and th_init() to have run. / Precisa do theme.h e do th_init() já executado. */
#pragma once
#include "theme.h"

typedef struct {
    int keypad_y, keypad_h;      /* top and height of the keypad / topo e altura do teclado               */
    int footer_y, footer_h;      /* box of the footer line / caixa da linha do rodapé                     */
    int content_bottom;          /* bottom of the last text above the keypad / fim do último texto acima  */
} LockGeom;

static inline LockGeom lock_geom(void)
{
    LockGeom g;
    g.keypad_h  = 4 * TH_DP(TH_H_TECLA) + 3 * TH_DP(TH_SP3);              /* same as comp_keypad() / igual ao comp_keypad() */
    g.footer_h  = TH_DP(TH_F_CAPS) + TH_DP(TH_SP1);                       /* text height plus room for descenders / altura do texto mais folga */
    g.footer_y  = th_H - TH_DP(TH_SP4) - g.footer_h;
    g.keypad_y  = g.footer_y - TH_DP(TH_SP3) - g.keypad_h;               /* a gap, then the keypad / uma folga, depois o teclado */
    /* the same steps lock_draw() takes from 30% of the height down to the "wrong tries" line
     * / os mesmos passos que o lock_draw() faz de 30% da altura até a linha de "tentativas erradas" */
    g.content_bottom = th_H * 30 / 100 + TH_DP(TH_F_L) + TH_DP(TH_F_M) + TH_DP(TH_SP5) + TH_DP(TH_SP6) + TH_DP(TH_F_XS);
    return g;
}
