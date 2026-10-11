#include "../init/devcmd.h"
#include <stdio.h>
static int fails;
#define CHECK(n, c) do { int ok_ = (c); printf("%s %s\n", ok_ ? "PASS" : "FAIL", n); if (!ok_) fails++; } while (0)
static int feed(DevCmd *c, const char *s) { return devcmd_feed(c, s, (int)strlen(s)); }
int main(void) {
    DevCmd c; memset(&c, 0, sizeof c);
    CHECK("comando exato com LF", feed(&c, "BANKPHONE:REBOOT-BOOTLOADER\n") == DEVCMD_REBOOT_BOOTLOADER);
    CHECK("comando exato com CRLF", feed(&c, "BANKPHONE:REBOOT-BOOTLOADER\r\n") == DEVCMD_REBOOT_BOOTLOADER);
    CHECK("sem fim de linha nao dispara", feed(&c, "BANKPHONE:REBOOT-BOOTLOADER") == DEVCMD_NONE);
    CHECK("o fim de linha depois dispara", feed(&c, "\n") == DEVCMD_REBOOT_BOOTLOADER);
    memset(&c, 0, sizeof c);
    CHECK("chegando em pedacos", feed(&c, "BANKPHONE:REBO") == DEVCMD_NONE && feed(&c, "OT-BOOTLOADER\n") == DEVCMD_REBOOT_BOOTLOADER);
    CHECK("lixo antes na mesma linha nao dispara", feed(&c, "xxBANKPHONE:REBOOT-BOOTLOADER\n") == DEVCMD_NONE);
    CHECK("lixo depois na mesma linha nao dispara", feed(&c, "BANKPHONE:REBOOT-BOOTLOADERx\n") == DEVCMD_NONE);
    CHECK("minusculas nao disparam", feed(&c, "bankphone:reboot-bootloader\n") == DEVCMD_NONE);
    CHECK("comando desconhecido ignorado", feed(&c, "BANKPHONE:FORMAT\n") == DEVCMD_NONE);
    CHECK("linha vazia ignorada", feed(&c, "\n\r\n") == DEVCMD_NONE);
    CHECK("lixo e depois comando em outra linha", feed(&c, "\x01\xff\n") == DEVCMD_NONE && feed(&c, "BANKPHONE:REBOOT-BOOTLOADER\n") == DEVCMD_REBOOT_BOOTLOADER);
    char big[400]; memset(big, 'A', sizeof big - 1); big[sizeof big - 1] = 0;
    CHECK("linha enorme nao estoura e e ignorada", feed(&c, big) == DEVCMD_NONE && feed(&c, "\n") == DEVCMD_NONE);
    CHECK("depois da linha enorme o comando volta a funcionar", feed(&c, "BANKPHONE:REBOOT-BOOTLOADER\n") == DEVCMD_REBOOT_BOOTLOADER);
    memset(&c, 0, sizeof c);
    CHECK("linha enorme que termina com o comando nao dispara", feed(&c, big) == DEVCMD_NONE && feed(&c, "BANKPHONE:REBOOT-BOOTLOADER\n") == DEVCMD_NONE);
    memset(&c, 0, sizeof c);
    CHECK("NUL no fim da linha nao dispara", devcmd_feed(&c, "BANKPHONE:REBOOT-BOOTLOADER\0\n", 29) == DEVCMD_NONE);
    CHECK("NUL no meio com lixo depois nao dispara", devcmd_feed(&c, "BANKPHONE:REBOOT-BOOTLOADER\0xyz\n", 32) == DEVCMD_NONE);
    /* --- SCREENSHOT e TOUCH --- */
    memset(&c, 0, sizeof c);
    CHECK("screenshot exato", feed(&c, "BANKPHONE:SCREENSHOT\n") == DEVCMD_SCREENSHOT);
    CHECK("screenshot com lixo depois nao dispara", feed(&c, "BANKPHONE:SCREENSHOTx\n") == DEVCMD_NONE);
    CHECK("screenshot em minusculas nao dispara", feed(&c, "bankphone:screenshot\n") == DEVCMD_NONE);
    CHECK("screenshot com NUL nao dispara", devcmd_feed(&c, "BANKPHONE:SCREENSHOT\0x\n", 23) == DEVCMD_NONE);
    CHECK("touch valido", feed(&c, "BANKPHONE:TOUCH 0 360 800\n") == DEVCMD_TOUCH && c.ev == 0 && c.x == 360 && c.y == 800);
    CHECK("touch solta", feed(&c, "BANKPHONE:TOUCH 2 10 1611\n") == DEVCMD_TOUCH && c.ev == 2 && c.x == 10 && c.y == 1611);
    CHECK("touch nos limites", feed(&c, "BANKPHONE:TOUCH 1 0 4095\n") == DEVCMD_TOUCH && c.x == 0 && c.y == 4095);
    CHECK("touch evento invalido", feed(&c, "BANKPHONE:TOUCH 3 1 1\n") == DEVCMD_NONE);
    CHECK("touch x acima do limite", feed(&c, "BANKPHONE:TOUCH 0 4096 1\n") == DEVCMD_NONE);
    CHECK("touch y acima do limite", feed(&c, "BANKPHONE:TOUCH 0 1 4096\n") == DEVCMD_NONE);
    CHECK("touch negativo", feed(&c, "BANKPHONE:TOUCH 0 -1 5\n") == DEVCMD_NONE);
    CHECK("touch com letra", feed(&c, "BANKPHONE:TOUCH 0 1a 5\n") == DEVCMD_NONE);
    CHECK("touch sem argumento", feed(&c, "BANKPHONE:TOUCH 0 1\n") == DEVCMD_NONE);
    CHECK("touch com argumento a mais", feed(&c, "BANKPHONE:TOUCH 0 1 2 3\n") == DEVCMD_NONE);
    CHECK("touch com dois espacos", feed(&c, "BANKPHONE:TOUCH 0  1 2\n") == DEVCMD_NONE);
    CHECK("touch com espaco no fim", feed(&c, "BANKPHONE:TOUCH 0 1 2 \n") == DEVCMD_NONE);
    CHECK("touch com numero enorme", feed(&c, "BANKPHONE:TOUCH 0 99999999999 2\n") == DEVCMD_NONE);
    CHECK("touch com NUL nao dispara", devcmd_feed(&c, "BANKPHONE:TOUCH 0 1 2\0\n", 23) == DEVCMD_NONE);
    c.ev = 9; c.x = 9; c.y = 9;
    CHECK("touch invalido nao mexe nos argumentos", feed(&c, "BANKPHONE:TOUCH 7 1 1\n") == DEVCMD_NONE && c.ev == 9 && c.x == 9 && c.y == 9);
    CHECK("reboot vence screenshot na mesma chamada", feed(&c, "BANKPHONE:SCREENSHOT\nBANKPHONE:REBOOT-BOOTLOADER\n") == DEVCMD_REBOOT_BOOTLOADER);
    CHECK("reboot vence mesmo vindo primeiro", feed(&c, "BANKPHONE:REBOOT-BOOTLOADER\nBANKPHONE:SCREENSHOT\n") == DEVCMD_REBOOT_BOOTLOADER);
    /* byte a byte (como o init lê): cada linha dispara uma vez */
    {
        const char *seq = "BANKPHONE:TOUCH 0 5 6\nBANKPHONE:TOUCH 2 5 6\nBANKPHONE:SCREENSHOT\n";
        int touches = 0, shots = 0;
        for (const char *q = seq; *q; q++) { int r = devcmd_feed(&c, q, 1); if (r == DEVCMD_TOUCH) touches++; if (r == DEVCMD_SCREENSHOT) shots++; }
        CHECK("sequencia byte a byte: 2 toques e 1 screenshot", touches == 2 && shots == 1);
    }
    puts(fails ? "FALHOU" : "TODOS OK");
    return fails != 0;
}
