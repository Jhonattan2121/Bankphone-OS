// BANKPHONE OS — interface. Independente de hardware: compila no Mac (pré-visualização/testes) e no celular.
#pragma once
#include <stdint.h>

// ---- ganchos de plataforma (implementados em main.c no celular e em host.c no Mac) ----
int64_t plat_now(void);                         // segundos (relógio real se válido, senão uptime)
int     plat_clock_valid(void);                 // 1 se plat_now é hora real
void    plat_random(uint8_t *b, int n);
void    plat_save(void);                        // persistir Money + PIN agora
int     plat_persist_ok(void);                  // 1 se a gravação persistente funciona
const char *plat_boot_prop(const char *key);    // valor da linha de comando do kernel ou "" se ausente

void ui_init(int w, int h);
void ui_draw(void);
int  ui_touch(int ev, int x, int y);            // ev: 0 toca, 1 move, 2 solta. 1 = redesenhar
int  ui_tick(void);                             // 1 por segundo; 1 = redesenhar
void ui_lock(void);                             // tela apagou: volta ao bloqueio

// ---- informações de diagnóstico que o PID 1 publica para a tela Developer ----
typedef struct {
    int  touch_ok, touch_ev, touch_down, touch_move, touch_up, touch_kicks, touch_dropped;
    char touch_node[40], touch_name[64], fw_ver[128];
    long long uptime_s;
    unsigned long frames;
    int  persist_ok, ro, safe_mode, serial_ok;
} UiDev;

void ui_set_dev(const UiDev *d);        // chamado pelo main.c (1x/s)
const UiDev *ui_dev(void);

// ---- gancho de teste: encontra um alvo de toque registrado no quadro atual ----
// Serve ao host (Mac) para dirigir a UI por AÇÃO em vez de por pixel: as
// capturas de tela saem do mesmo caminho de toque do aparelho.
int ui_hit_find(int act, int arg, int *cx, int *cy);
