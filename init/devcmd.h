/* BANKPHONE OS: comandos pela porta serial USB (só imagens de teste).
 *
 * O instalador (install.sh) manda uma linha de texto pela USB para o aparelho
 * reiniciar sozinho no bootloader (fastboot), sem ninguém apertar botão.
 * Só vale se a cmdline tiver bankphone.devcmd=1 (a imagem de teste liga isso).
 * Uma imagem de produção não aceita nenhum comando.
 *
 * O leitor é puro (sem I/O) para poder ser testado no computador. */
#pragma once
#include <string.h>

#define DEVCMD_NONE              0
#define DEVCMD_REBOOT_BOOTLOADER 1

typedef struct { char buf[96]; int n; int overflow; } DevCmd;

/* Alimenta o leitor com bytes lidos da serial. Devolve o comando se uma linha
 * COMPLETA e EXATA foi reconhecida; qualquer outra coisa é ignorada. */
static inline int devcmd_feed(DevCmd *c, const char *p, int len)
{
    int r = DEVCMD_NONE;
    for (int i = 0; i < len; i++) {
        char ch = p[i];
        if (ch == '\n' || ch == '\r') {
            if (!c->overflow && c->n > 0) {
                c->buf[c->n] = 0;
                if (!strcmp(c->buf, "BANKPHONE:REBOOT-BOOTLOADER")) r = DEVCMD_REBOOT_BOOTLOADER;
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
