/*
 * BANKPHONE OS — THEME. Arquivo ÚNICO com todos os tokens visuais.
 *
 * Regra deste arquivo: nenhuma tela escolhe cor, tamanho ou espaçamento por
 * conta própria. Tudo vem daqui. Se um valor não está aqui, ele não existe.
 *
 * Unidades: tudo é definido em dp (a tela tem 360 dp de largura; 720 px => 2 px
 * por dp). Use TH_DP(n) para converter. Alvos de toque têm mínimo obrigatório de
 * 44 dp (TH_TOUCH_MIN_DP) — a tela Developer mostra quantos alvos ficaram abaixo
 * disso, então a regra é VERIFICÁVEL, não uma promessa.
 *
 * Direção visual: "terminal financeiro pessoal" — escuro, denso, tipografia
 * técnica, números tabulares, linhas finas, cor só com significado. Sem neon,
 * sem gradiente decorativo, sem aparência de app de banco genérico.
 */
#ifndef BANKPHONE_THEME_H
#define BANKPHONE_THEME_H

#include "gfx.h"
#include <stdint.h>

/* ------------------------------------------------------------------ cor --- */
#define TH_BG        0x08090C   /* fundo da aplicação                        */
#define TH_SURF      0x101218   /* superfície: cartões, barras                */
#define TH_SURF2     0x171A22   /* superfície elevada: botões neutros, linhas */
#define TH_SURF3     0x1F232D   /* pressionado / selecionado                  */
#define TH_LINE      0x22262F   /* divisor de 1 px                            */
#define TH_LINE_S    0x2E3340   /* divisor forte (borda de cartão em foco)    */
#define TH_TXT       0xEDEFF5   /* texto primário                             */
#define TH_TXT2      0x9BA1B0   /* texto secundário                           */
#define TH_TXT3      0x666C7C   /* texto terciário / rótulo de apoio          */
#define TH_ACC       0x4ED4BC   /* acento: ação primária, valor em destaque   */
#define TH_ACC_DIM   0x1E3B3A   /* acento em fundo (estado ativo)             */
#define TH_OK        0x46C97E
#define TH_WARN      0xE3A93C
#define TH_BAD       0xE5605F
#define TH_DEMO      0xE3A93C   /* selo DEMO/SANDBOX/TESTNET: sempre âmbar    */

/* ----------------------------------------------------------------- tipo --- */
/* Só existe Roboto Regular no aparelho. Hierarquia é feita com TAMANHO,
 * ESPAÇAMENTO e PESO SINTÉTICO (duplo traço de 1 px quando weight>=600).
 * Fontes Medium/Bold/Mono reais não fazem parte do projeto: nada disso é
 * dependência, o sistema funciona só com a Regular. */
#define TH_F_XXL     40    /* saldo principal (dp)                              */
#define TH_F_XL      28    /* título de tela                                    */
#define TH_F_L       21    /* número de seção / valor de transação              */
#define TH_F_M       17    /* corpo                                             */
#define TH_F_S       14    /* rótulo, linha de lista                            */
#define TH_F_XS      12    /* texto de apoio                                    */
#define TH_F_CAPS    10    /* rótulo em CAIXA ALTA com espaçamento              */
#define TH_TR_CAPS    1    /* tracking do rótulo em caixa alta (dp)             */
#define TH_TR_NUM     0    /* números: Roboto já tem dígitos de largura fixa    */

/* ----------------------------------------------------------------- grade -- */
/* Espaçamento em dp, múltiplos de 4: 4 8 12 16 24 32                 */
#define TH_SP1        4
#define TH_SP2        8
#define TH_SP3       12
#define TH_SP4       16
#define TH_SP5       24
#define TH_SP6       32
#define TH_MARGIN    16    /* margem lateral padrão                            */
#define TH_RAIO      12    /* raio de cartão/botão                             */
#define TH_RAIO_S     8
#define TH_BORDA      1    /* espessura de borda (1 px real)                   */

/* alturas de componente em dp */
#define TH_H_STATUS  24    /* barra de status (padrão de sistema)              */
#define TH_H_NAV     60    /* barra de navegação                               */
#define TH_H_LISTA   52    /* linha de lista com duas linhas (>= 44 dp)         */
#define TH_H_BOTAO   44    /* botão padrão (>= 44 dp)                          */
#define TH_H_BOTAO_G 52    /* botão grande (ação primária)                     */
#define TH_H_TECLA   56    /* tecla do teclado numérico (>= 44 dp)             */
#define TH_TOUCH_MIN_DP 44 /* MÍNIMO obrigatório de alvo de toque              */

/* ------------------------------------------------------------------ API --- */

/* O tema precisa saber o tamanho da tela antes de qualquer coisa. */
extern int th_W, th_H, th_SC;      /* largura, altura, px por dp */
/* As três variáveis acima são DEFINIDAS em components.c (o módulo que desenha). */

static inline void th_init(int w, int h)
{
    th_W = w; th_H = h;
    th_SC = w >= 640 ? 2 : 1;      /* 720 px => 2 px/dp; telas pequenas => 1 */
    if (th_SC < 1) th_SC = 1;
}

/* dp -> pixels (arredonda para o inteiro mais próximo) */
static inline int th_dp(int dp) { return dp * th_SC; }
#define TH_DP(n) th_dp(n)

/* área útil: abaixo da barra de status e acima da navegação */
static inline int th_top(void)  { return TH_DP(TH_H_STATUS); }
static inline int th_bottom(void) { return th_H - TH_DP(TH_H_NAV); }

/* conteúdo com margem lateral */
static inline int th_cx(void)   { return TH_DP(TH_MARGIN); }
static inline int th_cw(void)   { return th_W - 2 * TH_DP(TH_MARGIN); }

/* ------------------------------------------------------------------ texto -- */

typedef enum { TH_W_REG = 400, TH_W_STRONG = 700 } ThWeight;

/* Cache simples das quatro métricas que o layout usa o tempo todo. */
static inline int th_text_w(int px, const char *s, int track, ThWeight wgt)
{
    int v = g_text_w(px, s, track);
    return wgt >= TH_W_STRONG ? v + 1 : v;         /* duplo traço alarga 1 px */
}
static inline void th_text(int x, int y, int px, const char *s, uint32_t c, int track, ThWeight wgt)
{
    g_text(x, y, px, s, c, track);
    if (wgt >= TH_W_STRONG) g_text(x + 1, y, px, s, c, track);   /* peso sintético */
}
/* centralizado / à direita, respeitando o peso sintético */
static inline void th_text_c(int cx, int y, int px, const char *s, uint32_t c, int track, ThWeight wgt)
{
    th_text(cx - th_text_w(px, s, track, wgt) / 2, y, px, s, c, track, wgt);
}
static inline void th_text_r(int xr, int y, int px, const char *s, uint32_t c, int track, ThWeight wgt)
{
    th_text(xr - th_text_w(px, s, track, wgt), y, px, s, c, track, wgt);
}

#endif /* BANKPHONE_THEME_H */
