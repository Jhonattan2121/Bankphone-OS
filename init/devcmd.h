/* BANKPHONE OS: commands over the USB serial port (test images only).
 *
 * The installer (install.sh) sends a text line over USB so the phone reboots into the
 * bootloader (fastboot) by itself, with nobody pressing a button. tools/screenshot.py sends
 * SCREENSHOT and TOUCH lines so the real screen can be captured over the cable.
 * It only works when the kernel command line has bankphone.devcmd=1 (the test image sets it).
 * A production image accepts no commands at all.
 *
 * The reader is pure (no I/O) so it can be tested on a computer.
 *
 * ---- Português ----
 * BANKPHONE OS: comandos pela porta serial USB (só imagens de teste).
 *
 * O instalador (install.sh) manda uma linha de texto pela USB para o aparelho reiniciar sozinho
 * no bootloader (fastboot), sem ninguém apertar botão. O tools/screenshot.py manda linhas
 * SCREENSHOT e TOUCH para capturar a tela de verdade pelo cabo.
 * Só vale se a cmdline tiver bankphone.devcmd=1 (a imagem de teste liga isso).
 * Uma imagem de produção não aceita nenhum comando.
 *
 * O leitor é puro (sem I/O) para poder ser testado no computador. */
#pragma once
#include <string.h>

#define DEVCMD_NONE              0
#define DEVCMD_REBOOT_BOOTLOADER 1
#define DEVCMD_SCREENSHOT        2
#define DEVCMD_TOUCH             3   /* arguments in ev, x, y / argumentos em ev, x, y */

#define DEVCMD_TOUCH_MAX 4095        /* upper bound for x and y / limite de x e y */

typedef struct { char buf[96]; int n; int overflow; int ev, x, y; } DevCmd;

/* Strict "BANKPHONE:TOUCH <ev> <x> <y>": decimal digits only, single spaces, ev 0..2,
 * x and y 0..DEVCMD_TOUCH_MAX. Anything else is rejected.
 * "BANKPHONE:TOUCH <ev> <x> <y>" estrito: só dígitos decimais, espaços simples, ev 0..2,
 * x e y de 0 a DEVCMD_TOUCH_MAX. Qualquer outra coisa é recusada. */
static inline int devcmd_parse_touch(DevCmd *c)
{
    static const char P[] = "BANKPHONE:TOUCH ";
    const size_t pl = sizeof P - 1;
    if ((size_t)c->n <= pl || memcmp(c->buf, P, pl)) return 0;
    long v[3]; int k = 0; size_t i = pl;
    while (k < 3) {
        size_t s = i; long x = 0;
        while (i < (size_t)c->n && c->buf[i] >= '0' && c->buf[i] <= '9' && i - s < 5) { x = x * 10 + (c->buf[i] - '0'); i++; }
        if (i == s) return 0;                              /* no digits / sem dígitos */
        v[k++] = x;
        if (k < 3) { if (i >= (size_t)c->n || c->buf[i] != ' ') return 0; i++; }
    }
    if (i != (size_t)c->n) return 0;                       /* trailing bytes / sobra no fim */
    if (v[0] > 2 || v[1] > DEVCMD_TOUCH_MAX || v[2] > DEVCMD_TOUCH_MAX) return 0;
    c->ev = (int)v[0]; c->x = (int)v[1]; c->y = (int)v[2];
    return 1;
}

/* Feeds the reader with bytes read from the serial port. Returns the command if a COMPLETE and
 * EXACT line was recognised; anything else is ignored. The reboot command wins over the others
 * within one call.
 * Alimenta o leitor com bytes lidos da serial. Devolve o comando se uma linha COMPLETA e EXATA
 * foi reconhecida; qualquer outra coisa é ignorada. O reboot vence os outros numa mesma chamada. */
static inline int devcmd_feed(DevCmd *c, const char *p, int len)
{
    int r = DEVCMD_NONE;
    for (int i = 0; i < len; i++) {
        char ch = p[i];
        if (ch == '\n' || ch == '\r') {
            if (!c->overflow && c->n > 0) {
                c->buf[c->n] = 0;
                /* Compare by length with memcmp: a NUL inside the line must not hide trailing bytes.
                 * memcmp com o tamanho: um NUL no meio da linha não pode esconder lixo depois dele. */
                if (c->n == (int)sizeof "BANKPHONE:REBOOT-BOOTLOADER" - 1 && !memcmp(c->buf, "BANKPHONE:REBOOT-BOOTLOADER", (size_t)c->n)) r = DEVCMD_REBOOT_BOOTLOADER;
                else if (r != DEVCMD_REBOOT_BOOTLOADER && c->n == (int)sizeof "BANKPHONE:SCREENSHOT" - 1 && !memcmp(c->buf, "BANKPHONE:SCREENSHOT", (size_t)c->n)) r = DEVCMD_SCREENSHOT;
                else if (r != DEVCMD_REBOOT_BOOTLOADER && devcmd_parse_touch(c)) r = DEVCMD_TOUCH;
            }
            c->n = 0; c->overflow = 0;
        } else if (c->n < (int)sizeof c->buf - 1) {
            c->buf[c->n++] = ch;
        } else {
            c->overflow = 1;
        }
    }
    return r;
}
