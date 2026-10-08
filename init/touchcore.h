/*
 * BANKPHONE OS — máquina de estados do toque (evdev → eventos da UI).
 *
 * Fica separada do main.c DE PROPÓSITO: assim ela roda e é testada no host
 * (tests/test_touchcore.c) com sequências sintéticas de input_event, em vez de
 * depender de um dedo no aparelho para provar que funciona.
 *
 * Cobre os protocolos que um driver MTK/Novatek pode usar:
 *   - protocolo B (ABS_MT_TRACKING_ID)  → o caso normal;
 *   - protocolo A (SYN_MT_REPORT, sem tracking id) → DOWN quando o quadro traz
 *     posição, UP por timeout de silêncio (protocolo A nunca manda "soltou");
 *   - BTN_TOUCH (EV_KEY) como fonte de DOWN/UP quando não há tracking id;
 *   - ABS_X/ABS_Y (toque simples) como fonte de posição se não houver MT;
 *   - SYN_DROPPED: descarta o estado parcial em vez de ficar com coordenada velha.
 *
 * Regras: só emite com SYN (o quadro está completo); MOVE sem DOWN anterior é
 * ignorado; DOWN nunca sai sem coordenada; coordenadas SEMPRE recortadas para a
 * tela (não estoura o buffer do framebuffer no hit-test).
 *
 * O tempo entra por tf_now_ms: o chamador atualiza a cada volta do loop com o
 * relógio real (CLOCK_MONOTONIC) e chama tc_tick().
 */
#ifndef BANKPHONE_TOUCHCORE_H
#define BANKPHONE_TOUCHCORE_H

#include <linux/input.h>
#include <string.h>

/* evt: 0 = DOWN, 1 = MOVE, 2 = UP (mesma convenção de ui_touch) */
typedef void (*tc_emit_fn)(void *ctx, int evt, int x, int y);

typedef struct {
    /* geometria */
    int ax0, ax1, ay0, ay1;      /* faixa bruta do sensor   */
    int fw, fh;                  /* tela                    */

    /* estado do dedo */
    int tid_seen;                /* o driver já mandou ABS_MT_TRACKING_ID? */
    int tracking;                /* dedo tocando agora?                    */
    int btn_touch;               /* último BTN_TOUCH visto                 */
    int have_btn;                /* o driver manda BTN_TOUCH?              */
    int mt_report;               /* SYN_MT_REPORT visto neste quadro       */

    /* posição */
    int rawx, rawy, x, y;
    int pend_x, pend_y;          /* posições do quadro atual               */
    int pend_since_ev;           /* houve evento de posição neste quadro?  */
    int have_pos;                /* já recebemos alguma coordenada?        */

    /* tempo (CLOCK_MONOTONIC em ms, do chamador) */
    long long now_ms, last_report_ms;

    /* contadores/telemetria */
    int ev, syn, dropped, downs, ups, moves, ghosts, no_down;

    /* saída */
    tc_emit_fn emit;
    void *ctx;
} touch_state;

static inline void tc_reset(touch_state *t, int ax0, int ax1, int ay0, int ay1, int fw, int fh)
{
    memset(t, 0, sizeof *t);
    t->ax0 = ax0; t->ax1 = ax1; t->ay0 = ay0; t->ay1 = ay1;
    t->fw = fw; t->fh = fh;
}

static inline void tc_now(touch_state *t, long long ms) { t->now_ms = ms; }

/* converte bruto → tela, sempre dentro de [0, fw-1]/[0, fh-1] */
static inline void tc_convert(const touch_state *t, int rx, int ry, int *ox, int *oy)
{
    int x = t->ax1 > t->ax0 ? (rx - t->ax0) * t->fw / (t->ax1 - t->ax0) : rx;
    int y = t->ay1 > t->ay0 ? (ry - t->ay0) * t->fh / (t->ay1 - t->ay0) : ry;

    if (x < 0)
        x = 0;
    if (x > t->fw - 1)
        x = t->fw - 1;
    if (y < 0)
        y = 0;
    if (y > t->fh - 1)
        y = t->fh - 1;

    *ox = x; *oy = y;
}

/* fecha o quadro (chamado no SYN_REPORT) */
static inline void tc_flush(touch_state *t)
{
    if (t->pend_since_ev) {
        t->rawx = t->pend_x;
        t->rawy = t->pend_y;
        t->pend_since_ev = 0;
        t->have_pos = 1;
    }
    tc_convert(t, t->rawx, t->rawy, &t->x, &t->y);
    t->syn++;
    t->last_report_ms = t->now_ms;

    if (!t->tracking)
        return;

    if (t->downs == t->ups) {              /* primeiro quadro do toque = DOWN */
        if (!t->have_pos) {                /* ainda sem coordenada: não inventa */
            t->no_down++;
            return;
        }
        t->downs++;
        if (t->emit)
            t->emit(t->ctx, 0, t->x, t->y);
    } else {
        t->moves++;
        if (t->emit)
            t->emit(t->ctx, 1, t->x, t->y);
    }
}

static inline void tc_release(touch_state *t)
{
    if (t->tracking && t->downs > t->ups) {  /* só solta o que chegou a descer */
        t->ups++;
        if (t->emit)
            t->emit(t->ctx, 2, t->x, t->y);
    } else {
        t->ghosts++;                         /* soltou o que nunca desceu */
    }
    t->tracking = 0;
}

static inline void tc_event(touch_state *t, const struct input_event *e)
{
    t->ev++;

    switch (e->type) {
    case EV_ABS:
        switch (e->code) {
        case ABS_MT_TRACKING_ID:
            t->tid_seen = 1;
            if (e->value >= 0)
                t->tracking = 1;
            else
                tc_release(t);
            break;
        case ABS_MT_POSITION_X: t->pend_x = e->value; t->pend_since_ev = 1; break;
        case ABS_MT_POSITION_Y: t->pend_y = e->value; t->pend_since_ev = 1; break;
        case ABS_X: t->pend_x = e->value; t->pend_since_ev = 1; break;
        case ABS_Y: t->pend_y = e->value; t->pend_since_ev = 1; break;
        default: break;                    /* ABS_MT_SLOT, PRESSURE, TOUCH_MAJOR… */
        }
        break;

    case EV_KEY:
        if (e->code == BTN_TOUCH) {
            t->have_btn = 1;
            t->btn_touch = e->value ? 1 : 0;
            if (!t->tid_seen) {            /* sem tracking id: BTN_TOUCH manda */
                if (e->value)
                    t->tracking = 1;
                else
                    tc_release(t);
            }
        }
        break;

    case EV_SYN:
        if (e->code == SYN_DROPPED) {
            t->dropped++;                  /* buffer estourou: perde-se o parcial */
            if (t->tracking)
                tc_release(t);             /* solta o dedo: a UI não pode travar
                                            * pressionada com estado perdido     */
            t->pend_since_ev = 0;
            t->tracking = 0;
            t->btn_touch = 0;
            t->mt_report = 0;
        } else if (e->code == SYN_MT_REPORT) {
            t->mt_report = 1;
        } else if (e->code == SYN_REPORT) {
            if (!t->tid_seen && !t->tracking && (t->btn_touch || t->mt_report))
                t->tracking = 1;           /* protocolo A: dedo novo neste quadro */
            tc_flush(t);
            t->mt_report = 0;
        }
        break;

    default: break;
    }
}

/* Chamar em todo ciclo do loop com o relógio atualizado (tc_now). Sintetiza o
 * UP que o protocolo A nunca manda. Devolve 1 se soltou o dedo. */
static inline int tc_tick(touch_state *t, long long silence_ms)
{
    if (t->tid_seen)
        return 0;                          /* protocolo B manda o UP sozinho */
    if (t->tracking && (t->now_ms - t->last_report_ms) > silence_ms) {
        tc_release(t);
        return 1;
    }
    return 0;
}

#endif /* BANKPHONE_TOUCHCORE_H */
