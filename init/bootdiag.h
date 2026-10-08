/*
 * BANKPHONE OS — bootdiag: instrumentação de boot para o X669C (MT6765H).
 *
 * Objetivo único: transformar "tela preta, serial mudo" em EVIDÊNCIA.
 *
 * Três canais, independentes entre si:
 *   1. NÚMERO DE ESTÁGIO NA TELA  — pintado direto no framebuffer, sem fonte,
 *      sem UI, sem store. Uma FOTO da tela já diz onde o boot parou.
 *   2. RELATÓRIO PERSISTENTE      — texto com CRC em partição bruta, dual-slot,
 *      acumula entre boots. Sobrevive a reset e a pânico.
 *   3. SERIAL GATED POR DTR       — o gadget ACM só transmite quando o host abriu
 *      a porta; o relatório inteiro é despejado NESSE momento (antes disso os
 *      writes são descartados pelo gadget, o que explica "a serial não emite nada").
 *
 * Mais: captura o ring buffer do KERNEL (dmesg) — a única forma de ver o que o
 * mtkfb/lcm/input/regulador disseram sem console UART.
 *
 * Este arquivo é ADITIVO: não toca em nenhum arquivo existente do projeto.
 * Regras: nada de hardware presumido, toda falha é registrada, nada é "fake".
 */
#ifndef BANKPHONE_BOOTDIAG_H
#define BANKPHONE_BOOTDIAG_H

#include <stddef.h>
#include <stdint.h>

/* ------------------------------------------------------------------ estágios */
/* O número do estágio é pintado na TELA em 7 segmentos. Se o boot travar,
 * o número visível é o diagnóstico — mesmo sem serial, sem USB, sem log. */
enum bd_stage {
    BD_ST_MOUNTS    = 1,   /* /dev, /proc, /sys montados            */
    BD_ST_KMSG      = 2,   /* /dev/kmsg aberto                      */
    BD_ST_DMESG     = 3,   /* ring buffer do kernel capturado       */
    BD_ST_INVENTORY = 4,   /* inventário de /sys/class coletado     */
    BD_ST_REPORT    = 5,   /* relatório persistente aberto          */
    BD_ST_SERIAL    = 6,   /* gadget configfs + ttyGS0 abertos      */
    BD_ST_FB_OPEN   = 7,   /* framebuffer aberto (GET_VSCREENINFO)  */
    BD_ST_FB_POWER  = 8,   /* UNBLANK + PUT_VSCREENINFO(FORCE)      */
    BD_ST_FB_BARS   = 9,   /* barras de cor desenhadas (PROVA)      */
    BD_ST_BACKLIGHT = 10,  /* backlight aplicado                    */
    BD_ST_INPUT     = 11,  /* dispositivos de entrada abertos       */
    BD_ST_LOOP      = 12,  /* loop principal rodando                */
    BD_ST_FAIL      = 90,  /* falha registrada (ver código ao lado)  */
};

/* códigos de falha (aparecem no lugar do estágio) */
enum bd_fail {
    BD_F_NONE        = 0,
    BD_F_MOUNT       = 1,
    BD_F_REPORT_PART = 2,   /* partição de log ausente/curta */
    BD_F_SERIAL      = 3,
    BD_F_FB_NODE     = 4,   /* nenhum framebuffer em /dev    */
    BD_F_FB_IOCTL    = 5,
    BD_F_FB_BPP      = 6,   /* bpp não suportado             */
    BD_F_FB_MMAP     = 7,
    BD_F_FB_POWER    = 8,
    BD_F_BACKLIGHT   = 9,
    BD_F_STORE       = 10,
    BD_F_INPUT       = 11,
};

/* --------------------------------------------------------------------- log */

void bd_init(void);                     /* captura cmdline; abre /dev/kmsg */
void bd_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void bd_log_raw(const char *s);         /* somente write() — seguro em sinal */
const char *bd_report_text(void);       /* texto acumulado (NUL-terminado) */
size_t bd_report_len(void);

/* -------------------------------------------------------- relatório durável */

/* Abre a partição bruta de log por NOME (ex.: "expdb"). Recusa partições
 * críticas (preloader/lk/tee/nvram/persist/misc/super/...). 0 = ok. */
int  bd_report_open(const char *partition_or_null);
void bd_flush(void);                    /* grava (CRC + slots alternados) */
const char *bd_report_location(void);   /* onde o log está + como reler */
void bd_report_dump(void);              /* joga o relatório inteiro no serial/kmsg */

/* ------------------------------------------------------------- kernel ring */

size_t bd_capture_kernel_log(void);     /* lê o ring buffer inteiro (dmesg) */
int    bd_kmsg_grep(const char *tag, const char *const *keys, int nkeys, int max);
                                        /* loga SÓ as linhas novas que casam (ex.: [NVT-ts]) */
const char *bd_cmdline(void);

/* ------------------------------------------------------------------- serial */

int  bd_serial_bind(void);              /* gadget já deve estar configurado */
int  bd_serial_fd(void);
int  bd_serial_host_open(void);         /* TIOCMGET: host abriu a porta?     */
void bd_serial_pump(void);              /* chamar no loop: despeja quando dá */
int  bd_serial_attach_config(void);     /* só abre ttyGS0 (já configurado)   */

/* ------------------------------------------------------------------ display */

typedef struct {
    int      fd;
    int      w, h;              /* vi.xres / vi.yres                  */
    int      stride;            /* fi.line_length (bytes)             */
    int      bpp;               /* bits per pixel (16/24/32)          */
    int      xoff, yoff;        /* vi.xoffset / vi.yoffset            */
    int      yres_virtual;
    uint32_t red_off,   red_len;
    uint32_t green_off, green_len;
    uint32_t blue_off,  blue_len;
    uint8_t *mem;               /* mmap                               */
    size_t   map_len;
    char     node[64];          /* caminho usado                      */
    int      unblank_rc, put_rc, pan_rc;
    int      ready;             /* 1 = pode desenhar                  */
} bd_fb;

int  bd_fb_open(bd_fb *o);              /* descobre + abre + GET_* + mmap */
int  bd_fb_power(bd_fb *o);             /* UNBLANK -> sysfs blank="0" -> PUT(FORCE) */
int  bd_fb_blank(bd_fb *o, int blank);  /* FBIOBLANK cru: o ciclo POWERDOWN->UNBLANK é o
                                         * ÚNICO gatilho do download de firmware do
                                         * Novatek NT36xxx */
void bd_fb_fill(bd_fb *o, uint32_t rgb);
void bd_fb_rect(bd_fb *o, int x, int y, int w, int h, uint32_t rgb);
void bd_fb_bars(bd_fb *o);              /* 8 barras: valida ordem de canais */
void bd_fb_stage(bd_fb *o, int value, uint32_t rgb);  /* número em 7 segmentos */
void bd_fb_flush(bd_fb *o);

extern bd_fb g_bd_fb;                   /* usado por bd_stage() */

/* ----------------------------------------------------------------- estágio */

void bd_stage(int stage);               /* log + número na tela + flush */
void bd_fail(int code, const char *why);

/* --------------------------------------------------------------- backlight */

/* Descobre o node correto (leds/backlight), respeita max_brightness,
 * escreve DUAS vezes e LÊ de volta para confirmar. Devolve nº de sucessos. */
int  bd_backlight(int percent);

/* --------------------------------------------------------------- inventário */

void bd_dump_inventory(void);           /* /sys/class/{graphics,leds,backlight,
                                           drm,udc,tty,input} + input devices +
                                           partições + mounts + pstore */

/* ---------------------------------------------------------------- watchdog */

/* Só relata o que existe — NÃO abre /dev/watchdog (abrir e não alimentar
 * reinicia o aparelho; fechar sem o caractere 'V' também pode reiniciar). */
void bd_watchdog_probe(void);

/* ------------------------------------------------------------------ reboot */

/* target: NULL/"system" | "bootloader" | "recovery"
 * bd_reboot_restart2(): RESTART2 puro — NÃO escreve em nenhuma partição.
 * bd_reboot_target():    RESTART2 + BCB na partição `misc` (escreve 2 KiB em
 *                        `misc`, preservando o resto; é o caminho que o Android
 *                        usa de verdade — use só com confirmação explícita). */
void bd_reboot_restart2(const char *target);
void bd_reboot_target(const char *target);
void bd_poweroff(void);

/* ------------------------------------------------------------------ extras */

size_t bd_klog_snapshot(char *out, size_t cap);  /* cópia do dmesg capturado */
void   bd_note_written_frames(unsigned long n);

#endif /* BANKPHONE_BOOTDIAG_H */
