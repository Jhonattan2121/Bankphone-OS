/*
 * BANKPHONE OS — Navegação própria (não é Android): pilha de telas + folhas.
 *
 * Modelo: as 4 abas são a BASE. Qualquer tela aberta de dentro de uma aba é
 * EMPURRADA numa pilha (SC_SETTINGS, SC_DEVELOPER) e o botão Voltar desempilha.
 * Folhas (sheets) vivem por cima, com uma única folha por vez.
 *
 * Aqui também ficam as AÇÕES: os componentes não mexem em estado, só registram
 * (ação, argumento). Quem executa é ui.c. Isso mantém a UI testável no Mac.
 */
#ifndef BANKPHONE_NAV_H
#define BANKPHONE_NAV_H

#include <stddef.h>

/* ------------------------------------------------------------------ telas --- */
enum {
    SC_MONEY = 0, SC_ACTIVITY, SC_WALLET, SC_SECURITY,   /* abas (base)      */
    SC_SETTINGS, SC_DEVELOPER,                            /* empilhadas       */
    SC_N
};

/* ----------------------------------------------------------------- folhas --- */
enum {
    SH_NONE = 0, SH_RECEIVE, SH_WHO, SH_AMOUNT, SH_REVIEW,
    SH_PIN, SH_CONFIRM2, SH_RECEIPT, SH_NOTICE, SH_N
};

/* ---------------------------------------------------------------- ações ---- */
enum {
    ACT_NONE = 0,
    ACT_KEY,        /* arg = caractere da tecla ('0'..'9', '<' = apagar)      */
    ACT_TAB,        /* arg = tela (aba)                                       */
    ACT_PUSH,       /* arg = tela empilhada                                   */
    ACT_BACK,       /* desempilha (ou fecha folha, se houver)                 */
    ACT_SHEET,      /* arg = folha                                            */
    ACT_CLOSE,      /* fecha folha                                            */
    ACT_WHO,        /* arg = índice do contato                                */
    ACT_CONT,       /* continua (revisão)                                     */
    ACT_AUTH,       /* pede PIN                                               */
    ACT_CONFIRM2,   /* 2ª confirmação                                         */
    ACT_CANCEL,     /* cancela transação pendente                             */
    ACT_LOADDEMO,   /* carrega saldo DEMO                                     */
    ACT_DEMOPIX,    /* arg = centavos                                         */
    ACT_FLIP,       /* inverte direção do swap                                */
    ACT_EMERG,      /* liga/desliga modo emergência                           */
    ACT_LOCK,       /* bloqueia a tela                                        */
    ACT_TX,         /* arg = índice da transação (recibo)                      */
    ACT_BRIGHT,     /* arg = 0..100 (brilho real)                             */
    ACT_BRIGHT_STEP,/* arg = -1 / +1 (ajuste fino)                            */
    ACT_HAPTIC,     /* alterna vibração ao toque                              */
    ACT_HW_REFRESH, /* relê o hardware agora                                  */
    ACT_VIB_TEST,   /* arg = ms: vibra de verdade (teste do usuário)          */
    ACT_BL_DISCOVER,
    ACT_NOTICE_TEST,   /* diagnóstico: mostra uma folha de aviso (teste de caminho) *//* redescobre o nó de backlight                           */
    ACT_SCROLL,     /* arg reservado                                          */
    ACT_N
};

/* -------------------------------------------------------------- folhas ---- */
/* Estado das folhas: fica no cabeçalho porque ui.c (dono do estado) e sheets.c
 * (dono do desenho) precisam do MESMO formato. */
typedef struct {
    int  amt_mode;          /* 0 = pix, 1 = troca                             */
    int  who;               /* contato escolhido                              */
    int  swap_from;         /* ativo de origem na troca                       */
    int  pend;              /* índice da transação pendente (-1 = nenhuma)     */
    int  rcpt;              /* índice da transação no comprovante (-1)         */
    int  purpose;           /* 0 = autorizar transação, 1 = desligar emergência*/
    int  pin_len;           /* dígitos do PIN já digitados                    */
    int  locked_s;          /* segundos de bloqueio do PIN (>0 = travado)      */
    char amt[16];           /* dígitos crus do valor                          */
    char msg[192];          /* mensagem de aviso                              */
} SheetState;

extern SheetState SH;
extern const char *SHEET_CONTACTS[3];
int  sheet_pend_tx(void);          /* índice pendente de autorização, ou -1  */
int  sheet_rcpt_tx(void);          /* índice do comprovante, ou -1            */
int  sheet_amt_value(int asset);
void sheet_amt_str(char *o, size_t n, int asset);

/* gancho de teste (host): acha um alvo registrado no quadro atual */
int  ui_hit_find(int act, int arg, int *cx, int *cy);
const char *screen_title(int scr);
int  screen_is_tab(int scr);
int  ui_haptics_on(void);
int  ui_hitcount(int *abaixo44, int *min_h_dp);
void ui_clock_str(char *o, size_t n);
int  ui_net_state(char *nome, size_t n);

/* ------------------------------------------------------------- registro ---- */
/* Toda área tocável é registrada AQUI (implementado em ui.c). O componente
 * informa a ação; a UI decide. Também é aqui que se AUDITA o tamanho mínimo de
 * 44 dp — o contador aparece na tela Developer. */
void ui_hit_add(int x, int y, int w, int h, int act, int arg);

/* Declaração de desenho da tela atual (implementado em screens.c).
 * Devolve a altura TOTAL do conteúdo (para o cálculo de rolagem). */
int  screens_draw(int scr, int scroll);

/* Declaração de desenho de folha (implementado em sheets.c). */
void sheets_draw(int sheet);

#endif /* BANKPHONE_NAV_H */
