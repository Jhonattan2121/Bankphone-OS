/*
 * BANKPHONE OS — BIBLIOTECA DE COMPONENTES. Nenhuma tela desenha à mão.
 *
 * Todos os componentes:
 *   - leem exclusivamente os tokens de theme.h;
 *   - registram a própria área de toque via ui_hit_add (com ação, não com
 *     callback), então a UI continua dirigida por dados e testável no Mac;
 *   - devolvem a coordenada Y de onde o desenho terminou, para o layout ser
 *     composição pura (sem constantes espalhadas).
 *
 * Estados: normal / pressionado são resolvidos pelo chamador (a UI guarda o que
 * está pressionado); aqui existe o parâmetro `on` para seleção e `dim` para
 * desabilitado.
 */
#ifndef BANKPHONE_COMPONENTS_H
#define BANKPHONE_COMPONENTS_H

#include "theme.h"
#include <stdint.h>

typedef enum {
    BK_PRIMARY = 0,   /* fundo de acento, texto escuro — UMA por tela           */
    BK_NEUTRAL,       /* superfície elevada com borda                           */
    BK_DANGER,        /* contorno/ação destrutiva                               */
    BK_GHOST,         /* só texto, sem fundo (ações terciárias)                 */
    BK_ICON           /* botão quadrado só com ícone                            */
} BtnKind;

/* ------------------------------------------------------------------ texto -- */
int  comp_caps_w(const char *s);
void comp_caps(int x, int y, const char *s, uint32_t color);
void comp_caps_clip(int x, int y, int maxw, const char *s, uint32_t color);           /* rótulo em caixa alta */
void comp_label(int x, int y, const char *s, uint32_t color, int px, ThWeight w);
void comp_label_r(int xr, int y, const char *s, uint32_t color, int px, ThWeight w);
void comp_label_c(int cx, int y, const char *s, uint32_t color, int px, ThWeight w);

/* Números TABULARES: todos os dígitos com a mesma largura (o avanço do '0'),
 * para o valor não "tremer" a cada atualização. Devolve a largura usada. */
int  comp_num_w(int px, const char *s, ThWeight w);
void comp_num(int x, int y, int px, const char *s, uint32_t color, ThWeight w);
void comp_num_r(int xr, int y, int px, const char *s, uint32_t color, ThWeight w);
void comp_num_c(int cx, int y, int px, const char *s, uint32_t color, ThWeight w);

/* Quebra de texto simples (palavra a palavra). Devolve a altura usada. */
int  comp_wrap(int x, int y, int w, int px, const char *s, uint32_t color);

/* ------------------------------------------------------------------ selo --- */
int  comp_badge(int x, int y, const char *text, uint32_t color);   /* devolve largura */

/* ----------------------------------------------------------------- botões -- */
/* Devolve a altura ocupada (== h). `on` marca seleção, `dim` desabilita. */
int  comp_button(int x, int y, int w, int h, const char *label, int icon_id,
                 BtnKind kind, int act, int arg, int on, int dim);

/* ----------------------------------------------------------------- cartão -- */
/* Desenha o cartão e a etiqueta (opcional) e devolve o Y do primeiro conteúdo. */
int  comp_card_begin(int x, int y, int w, int h, const char *caps_label, int act, int arg);
void comp_divider(int x, int y, int w);
/* Linha de duas colunas: rótulo à esquerda, valor à direita. */
int  comp_row_kv(int x, int y, int w, const char *label, const char *value,
                 uint32_t vcolor, int px, ThWeight wgt);
/* Linha de lista com ícone e seta (lista tocável). */
int  comp_row_list(int x, int y, int w, int icon_id, const char *label,
                   const char *value, uint32_t vcolor, int act, int arg, int disabled);

/* --------------------------------------------------------------- destaque -- */
/* Saldo grande com moeda, em números tabulares. Devolve a altura usada. */
int  comp_balance(int x, int y, const char *asset, const char *value, uint32_t color, int px);
/* Linha de transação (histórico / resumo). */
int  comp_tx_row(int x, int y, int w, const char *title, const char *sub, const char *when,
                 const char *amount, uint32_t acolor, int act, int arg);

/* ------------------------------------------------------------ barra e nav -- */
/* Barra de status REAL: hora (ou --:--), bateria %, rede, selo DEMO e ações. */
int  comp_statusbar(const char *screen_title, int show_back, int show_gear);
/* Selo de ambiente do sistema (DEMO/TESTNET/SANDBOX) — sempre visível. */
int  comp_env_badge(int x, int y);
/* Navegação por abas + engrenagem. Devolve a altura usada. */
int  comp_navbar(int active_tab);

/* ------------------------------------------------------------------ teclado */
int  comp_keypad(int x, int y, int w);          /* devolve a altura usada */
void comp_dots(int cx, int y, int n, int total);

/* ------------------------------------------------------------------ folha -- */
/* Fundo escurecido + cartão da folha. Devolve o Y do conteúdo e, em *out_x /
 * *out_w, a área útil interna. */
int  comp_sheet_begin(int sheet_h, int *out_x, int *out_w);
void comp_sheet_title(int x, int y, const char *title, const char *badge_text);

#endif /* BANKPHONE_COMPONENTS_H */
