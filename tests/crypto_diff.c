/* Differential test of the crypto primitives in init/sec.c against OpenSSL.
 * Reads the cases from tests/crypto_diff.py on stdin; prints one PASS/FAIL line per case.
 * Teste diferencial das primitivas de init/sec.c contra o OpenSSL.
 * Lê os casos de tests/crypto_diff.py na entrada padrão; imprime uma linha PASS/FAIL por caso. */
#include "../init/sec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXB 512

static size_t unhex(const char *s, uint8_t *o) {
    if (!strcmp(s, "-")) return 0;
    size_t n = strlen(s) / 2;
    for (size_t i = 0; i < n && i < MAXB; i++) { unsigned v; if (sscanf(s + 2 * i, "%2x", &v) != 1) return 0; o[i] = (uint8_t)v; }
    return n < MAXB ? n : MAXB;
}
static void hex(const uint8_t *b, size_t n, char *o) { for (size_t i = 0; i < n; i++) sprintf(o + 2 * i, "%02x", b[i]); o[2 * n] = 0; }

int main(void) {
    char kind[16], a[2 * MAXB + 8], b[2 * MAXB + 8], exp[2 * MAXB + 8];
    unsigned x, y, z, dk; int n = 0, bad = 0;
    static uint8_t A[MAXB], B[MAXB], I[MAXB], O[MAXB]; static char got[2 * MAXB + 8];
    while (scanf("%15s %1031s %1031s %u %u %u %u %1031s", kind, a, b, &x, &y, &z, &dk, exp) == 8) {
        if (dk == 0 || dk > 255) { puts("FAIL caso invalido"); bad++; continue; }
        size_t al = unhex(a, A); int rc = 0;
        if (!strcmp(kind, "scrypt")) { size_t bl = unhex(b, B); rc = scrypt_kdf(A, al, B, bl, x, y, z, O, dk); }
        else if (!strcmp(kind, "pbkdf2")) { size_t bl = unhex(b, B); pbkdf2_sha256(A, al, B, bl, x, O, dk); }
        else if (!strcmp(kind, "hmac")) { size_t bl = unhex(b, B); hmac_sha256(A, al, B, bl, O); }
        else if (!strcmp(kind, "hkdf")) {
            char *colon = strchr(b, ':'); if (!colon) { puts("FAIL formato"); bad++; continue; }
            *colon = 0; size_t sl = unhex(b, B), il = unhex(colon + 1, I);
            rc = hkdf_sha256(A, al, sl ? B : NULL, sl, il ? I : NULL, il, O, dk);
        } else { puts("FAIL tipo desconhecido"); bad++; continue; }
        hex(O, dk, got); n++;
        if (rc || strcmp(got, exp)) { bad++; printf("FAIL %s #%d (rc=%d)\n", kind, n, rc); } else printf("PASS %s #%d\n", kind, n);
    }
    if (n == 0 && bad == 0) { puts("SKIP: nenhum caso (o Python desta maquina nao tem hashlib.scrypt)"); return 0; }
    printf("%d casos, %d diferencas\n", n, bad);
    return bad != 0;
}
