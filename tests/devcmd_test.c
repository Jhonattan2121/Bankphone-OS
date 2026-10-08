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
    puts(fails ? "FALHOU" : "TODOS OK");
    return fails != 0;
}
