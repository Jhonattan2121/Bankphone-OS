/*
 * BANKPHONE OS — ÍCONES. Só primitivas vetoriais (linha, disco, arco), nada de
 * bitmap, nada de fonte de ícone. Assim o ícone fica nítido em qualquer tamanho
 * e não entra nenhuma dependência nova.
 *
 * Todos os ícones são desenhados numa caixa conceptual de 24x24 dp (a Stroke
 * segue 1,75 dp). Chamar sempre com o CENTRO e o tamanho em px.
 */
#ifndef BANKPHONE_ICONS_H
#define BANKPHONE_ICONS_H

#include <stdint.h>

enum {
    IC_SEND = 0, IC_RECEIVE, IC_SWAP, IC_WALLET, IC_SECURITY, IC_SETTINGS,
    IC_BACK, IC_CLOSE, IC_CHECK, IC_WARNING, IC_LOCK, IC_WIFI, IC_BATTERY,
    IC_CLOCK, IC_CHEVRON, IC_PLUS, IC_INFO, IC_N,  /* N = fim */
};

/* Desenha o ícone `id` centrado em (cx,cy) com altura útil `size` px. */
void icon(int id, int cx, int cy, int size, uint32_t color);

/* Nome curto (para a tela Developer / relatório). */
const char *icon_name(int id);

#endif
