#include "../init/sec.h"
#include <stdio.h>
#include <string.h>
int main(void) {
    uint8_t h[32]; char o[65]; int bad = 0;
    sha256((const uint8_t *)"abc", 3, h); for (int i = 0; i < 32; i++) sprintf(o + 2 * i, "%02x", h[i]);
    if (strcmp(o, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")) { puts("FAIL sha256 abc"); bad++; } else puts("PASS sha256 abc");
    uint8_t salt[16] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16}; pin_set("123456", salt);
    puts(pin_check("123456", 100) == 0 ? "PASS pin certo" : (bad++, "FAIL pin certo"));
    int r = 0; for (int i = 0; i < 4; i++) r |= pin_check("000000", 100) != 1;
    puts(!r ? "PASS pin errado x4" : (bad++, "FAIL errado"));
    puts(pin_check("000000", 100) == 1 && pin_locked_s(100) > 0 ? "PASS 5o erro bloqueia" : (bad++, "FAIL bloqueio"));
    puts(pin_check("123456", 101) == 2 ? "PASS bloqueado ate passar tempo" : (bad++, "FAIL bloqueado"));
    puts(pin_check("123456", 200) == 0 ? "PASS libera depois" : (bad++, "FAIL libera"));
    char b[200]; pin_serialize(b, 200); Pin keep = PIN; memset(&PIN, 0, sizeof PIN);
    puts(pin_deserialize(b) && !memcmp(PIN.hash, keep.hash, 32) ? "PASS serializa" : (bad++, "FAIL serializa"));
    puts(bad ? "FALHAS" : "TODOS OK"); return bad;
}
