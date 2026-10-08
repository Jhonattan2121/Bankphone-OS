/*
 * Testes do núcleo de toque no HOST (gcc normal, sem hardware).
 * Prova a lógica com sequências sintéticas de input_event — inclusive os
 * protocolos que o driver de um MTK/Novatek pode usar.
 *
 * Build/run:  cc -O2 -Wall -Wextra -o /tmp/test_touchcore tests/test_touchcore.c && /tmp/test_touchcore
 */
#include <stdio.h>
#include <string.h>
#include "../touchcore.h"

static struct { int evt, x, y; } got[64];
static int ngot;

static void sink(void *ctx, int evt, int x, int y)
{
    (void)ctx;
    if (ngot < 64) { got[ngot].evt = evt; got[ngot].x = x; got[ngot].y = y; }
    ngot++;
}

static int fails, checks;

static void chk(int cond, const char *what)
{
    checks++;
    if (!cond) { fails++; printf("  FALHOU: %s\n", what); }
}

static struct input_event ev(int type, int code, int value)
{
    struct input_event e;
    memset(&e, 0, sizeof e);
    e.type = (unsigned short)type;
    e.code = (unsigned short)code;
    e.value = value;
    return e;
}

static void feed(touch_state *t, struct input_event e) { tc_event(t, &e); }

int main(void)
{
    printf("== touchcore: protocolo B (ABS_MT_TRACKING_ID) ==\n");
    {
        touch_state t;
        tc_reset(&t, 0, 719, 0, 1611, 720, 1612);
        t.emit = sink; ngot = 0;
        tc_now(&t, 1000);

        feed(&t, ev(EV_ABS, ABS_MT_TRACKING_ID, 7));
        feed(&t, ev(EV_ABS, ABS_MT_POSITION_X, 360));
        feed(&t, ev(EV_ABS, ABS_MT_POSITION_Y, 806));
        feed(&t, ev(EV_SYN, SYN_REPORT, 0));
        chk(ngot == 1 && got[0].evt == 0 && got[0].x == 360 && got[0].y == 806, "DOWN 360,806");

        feed(&t, ev(EV_ABS, ABS_MT_POSITION_X, 100));
        feed(&t, ev(EV_ABS, ABS_MT_POSITION_Y, 200));
        feed(&t, ev(EV_SYN, SYN_REPORT, 0));
        chk(ngot == 2 && got[1].evt == 1 && got[1].x == 100 && got[1].y == 200, "MOVE 100,200");

        feed(&t, ev(EV_ABS, ABS_MT_TRACKING_ID, -1));
        feed(&t, ev(EV_SYN, SYN_REPORT, 0));
        chk(ngot == 3 && got[2].evt == 2, "UP");

        feed(&t, ev(EV_SYN, SYN_REPORT, 0));   /* quadro vazio: nada de fantasma */
        chk(ngot == 3, "sem evento extra após UP");
        chk(t.downs == 1 && t.ups == 1 && t.moves == 1, "contadores 1/1/1");

        /* segundo toque: DOWN de novo (contadores não podem travar) */
        feed(&t, ev(EV_ABS, ABS_MT_TRACKING_ID, 8));
        feed(&t, ev(EV_ABS, ABS_MT_POSITION_X, 5));
        feed(&t, ev(EV_ABS, ABS_MT_POSITION_Y, 5));
        feed(&t, ev(EV_SYN, SYN_REPORT, 0));
        chk(ngot == 4 && got[3].evt == 0, "segundo toque = novo DOWN");
    }

    printf("== touchcore: protocolo A (sem tracking id, SYN_MT_REPORT) ==\n");
    {
        touch_state t;
        tc_reset(&t, 0, 719, 0, 1611, 720, 1612);
        t.emit = sink; ngot = 0;
        tc_now(&t, 1000);

        feed(&t, ev(EV_ABS, ABS_MT_POSITION_X, 10));
        feed(&t, ev(EV_ABS, ABS_MT_POSITION_Y, 20));
        feed(&t, ev(EV_SYN, SYN_MT_REPORT, 0));
        feed(&t, ev(EV_SYN, SYN_REPORT, 0));
        chk(ngot == 1 && got[0].evt == 0 && got[0].x == 10 && got[0].y == 20, "DOWN proto A");

        tc_now(&t, 1100);
        feed(&t, ev(EV_ABS, ABS_MT_POSITION_X, 30));
        feed(&t, ev(EV_ABS, ABS_MT_POSITION_Y, 40));
        feed(&t, ev(EV_SYN, SYN_MT_REPORT, 0));
        feed(&t, ev(EV_SYN, SYN_REPORT, 0));
        chk(ngot == 2 && got[1].evt == 1, "MOVE proto A");

        tc_now(&t, 1100 + 149);
        chk(tc_tick(&t, 150) == 0, "sem UP antes do timeout");
        tc_now(&t, 1100 + 151);
        chk(tc_tick(&t, 150) == 1, "UP por timeout");
        chk(ngot == 3 && got[2].evt == 2, "UP emitido");
        chk(tc_tick(&t, 150) == 0, "não repete o UP");
    }

    printf("== touchcore: só BTN_TOUCH + ABS_X/Y ==\n");
    {
        touch_state t;
        tc_reset(&t, 0, 719, 0, 1611, 720, 1612);
        t.emit = sink; ngot = 0;
        tc_now(&t, 1000);

        feed(&t, ev(EV_ABS, ABS_X, 719));
        feed(&t, ev(EV_ABS, ABS_Y, 1611));
        feed(&t, ev(EV_KEY, BTN_TOUCH, 1));
        feed(&t, ev(EV_SYN, SYN_REPORT, 0));
        chk(ngot == 1 && got[0].evt == 0 && got[0].x == 719 && got[0].y == 1611, "DOWN no canto");

        feed(&t, ev(EV_KEY, BTN_TOUCH, 0));
        feed(&t, ev(EV_SYN, SYN_REPORT, 0));
        chk(ngot == 2 && got[1].evt == 2, "UP via BTN_TOUCH");
    }

    printf("== touchcore: robustez (SYN_DROPPED, MOVE solto, fora da tela) ==\n");
    {
        touch_state t;
        tc_reset(&t, 0, 719, 0, 1611, 720, 1612);
        t.emit = sink; ngot = 0;
        tc_now(&t, 1000);

        /* posição sozinha, sem BTN_TOUCH e sem SYN_MT_REPORT: NÃO é um toque */
        feed(&t, ev(EV_ABS, ABS_MT_POSITION_X, 400));
        feed(&t, ev(EV_SYN, SYN_REPORT, 0));
        chk(ngot == 0, "MOVE sem DOWN ignorado");

        /* coordenada fora da faixa: recortada, não estoura o hit-test */
        feed(&t, ev(EV_ABS, ABS_MT_TRACKING_ID, 1));
        feed(&t, ev(EV_ABS, ABS_MT_POSITION_X, 5000));
        feed(&t, ev(EV_ABS, ABS_MT_POSITION_Y, -50));
        feed(&t, ev(EV_SYN, SYN_REPORT, 0));
        chk(ngot == 1 && got[0].evt == 0 && got[0].x == 719 && got[0].y == 0, "coordenada recortada");

        /* SYN_DROPPED com dedo em baixo: solta o dedo para a UI não travar */
        feed(&t, ev(EV_SYN, SYN_DROPPED, 0));
        chk(t.dropped == 1 && t.tracking == 0, "SYN_DROPPED zera estado");
        chk(ngot == 2 && got[1].evt == 2, "SYN_DROPPED emite UP");
        chk(t.downs == 1 && t.ups == 1, "ciclo fechado após SYN_DROPPED");
        feed(&t, ev(EV_ABS, ABS_MT_TRACKING_ID, -1));   /* UP sem DOWN: fantasma */
        feed(&t, ev(EV_SYN, SYN_REPORT, 0));
        chk(t.ghosts == 1 && ngot == 2, "UP sem DOWN = fantasma, não conta");
    }

    printf("== touchcore: sensor menor que a tela (escala) ==\n");
    {
        touch_state t;
        tc_reset(&t, 0, 359, 0, 805, 720, 1612);
        t.emit = sink; ngot = 0;
        tc_now(&t, 1000);
        feed(&t, ev(EV_ABS, ABS_MT_TRACKING_ID, 3));
        feed(&t, ev(EV_ABS, ABS_MT_POSITION_X, 359));
        feed(&t, ev(EV_ABS, ABS_MT_POSITION_Y, 805));
        feed(&t, ev(EV_SYN, SYN_REPORT, 0));
        chk(got[0].x == 719 && got[0].y == 1611, "escala 2x aplicada e recortada");
    }

    printf("\n%d verificacoes, %d falhas\n", checks, fails);
    return fails ? 1 : 0;
}
