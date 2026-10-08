/*
 * BANKPHONE OS — HardwareService.
 *
 * Regra: TODO item tem estado explícito e motivo. Nada é presumido (Regra Zero)
 * e nada é inventado (R4): se não deu para ler, a tela mostra UNKNOWN/UNAVAILABLE
 * com o motivo, nunca um número plausível.
 *
 *   HW_AVAILABLE   — o nó existe E devolveu um valor válido (com prova no log)
 *   HW_UNAVAILABLE — não existe nó nenhum para este item neste kernel/aparelho
 *   HW_UNKNOWN     — o nó existe mas não leu (vazio, erro, valor fora de faixa)
 *
 * Descoberta: varredura de /sys/class/power_supply, /sys/class/backlight,
 * /sys/class/leds, /sys/class/timed_output e /sys/class/net. Nenhum caminho é
 * fixo no código; os caminhos encontrados vão para o log e para a tela.
 */
#ifndef BANKPHONE_HW_H
#define BANKPHONE_HW_H

#include <stddef.h>

typedef enum { HW_UNKNOWN = 0, HW_AVAILABLE, HW_UNAVAILABLE } HwState;

typedef struct {
    HwState st;
    char why[72];
} HwItem;

typedef struct {
    /* ---- bateria ---- */
    HwItem batt;
    char   batt_node[128];
    int    capacity;          /* % (lido)                                  */
    char   status[24];        /* Charging / Discharging / Full / Not charging */
    char   health[24];
    char   tech[24];
    long   voltage_uv;        /* µV                                        */
    long   temp_dc;           /* décimos de °C                             */
    long   current_ua;        /* µA (positivo = carregando)                */
    long   charge_counter_uah;
    int    present;

    /* ---- carregador ---- */
    HwItem chg;
    char   chg_node[128];
    int    chg_online;        /* -1 = desconhecido, 0 = fora, 1 = conectado */
    char   chg_type[24];

    /* ---- brilho ---- */
    HwItem bl;
    char   bl_node[160];      /* .../brightness                            */
    int    bl_max;            /* max_brightness lido do aparelho           */
    int    bl_raw;            /* valor atual                               */
    int    bl_pct;            /* 0..100 calculado do raw                   */

    /* ---- vibração ---- */
    HwItem vib;
    char   vib_node[160];
    int    vib_style;         /* 1 = leds (+duration)  2 = timed_output    */
    int    vib_last_ms;
    int    vib_writes;        /* quantas vezes o write deu certo           */

    /* ---- tela ---- */
    HwItem scr;
    char   fb_node[64];
    int    scr_w, scr_h, scr_bpp, scr_stride;

    /* ---- entrada ---- */
    HwItem touch;
    char   touch_node[40], touch_name[64];
    HwItem keys;
    int    keys_n;
    char   keys_desc[128];

    /* ---- rede (interface real do kernel; Wi-Fi é F3) ---- */
    HwItem net;
    char   net_if[16], net_state[16];
    int    net_up;

    long long last_read_ms;
} HwInfo;

extern HwInfo HW;

void hw_init(void);                              /* descoberta (uma vez)     */
/* Relê o volátil (1x/s). Devolve 1 quando ALGUM valor exibido mudou — é o que
 * faz a tela se redesenhar sozinha quando o usuário pluga o carregador, por
 * exemplo. Devolve 0 quando nada mudou. */
int  hw_tick(long long now_ms);
void hw_set_display(int w, int h, int bpp, int stride, const char *fbnode);
void hw_note_touch(const char *node, const char *name, int ok);
void hw_note_keys(int n, const char *desc);

int  hw_brightness_set(int pct);                 /* escreve e LÊ de volta    */
int  hw_brightness_get(void);
int  hw_vibrate(int ms);                         /* 0 = ok                   */

const char *hw_batt_pct_str(void);               /* "87%" ou "--"           */
const char *hw_batt_status_str(void);            /* "Carregando" etc.       */
const char *hw_net_str(void);                    /* "wlan0" ou ""           */
const char *hw_state_str(HwState s);
void hw_report(void);                            /* despeja tudo no log     */

#endif /* BANKPHONE_HW_H */
