/* Fuzzing of the saved-PIN-state parser (init/sec.c: pin_deserialize / pin_serialize).
 * The scrypt cost is read back from the stored state, so it must never be trusted blindly.
 * Invariants: it never crashes; a parsed v2 verifier always has an acceptable cost (a corrupted
 * state cannot ask for gigabytes); parse -> serialize -> parse is stable.
 *
 * ---- Português ----
 * Fuzzing do leitor do estado salvo do PIN (init/sec.c: pin_deserialize / pin_serialize).
 * O custo do scrypt é relido do estado gravado, então nunca pode ser aceito sem checar.
 * Invariantes: nunca quebra; um verificador v2 lido sempre tem custo aceitável (um estado corrompido
 * não consegue pedir gigabytes); ler -> gravar -> ler é estável. */
#include "sec.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define NEED(c) do { if (!(c)) abort(); } while (0)

int LLVMFuzzerTestOneInput(const uint8_t *d, size_t n) {
    if (n > 1024) return 0;
    char *s = malloc(n + 1); memcpy(s, d, n); s[n] = 0;
    memset(&PIN, 0, sizeof PIN);
    if (pin_deserialize(s)) {
        NEED(PIN.ver == 0 || PIN.ver == 1 || PIN.ver == 2);
        if (PIN.ver == 2) NEED(scrypt_params_ok(PIN.log2n, PIN.r, PIN.p));
        NEED(pin_state_key() == NULL);                       /* parsing never produces a key */
        char out[512]; size_t k = pin_serialize(out, sizeof out); NEED(k < sizeof out);
        Pin a = PIN; memset(&PIN, 0, sizeof PIN);
        NEED(pin_deserialize(out));
        NEED(!memcmp(PIN.salt, a.salt, 16) && !memcmp(PIN.hash, a.hash, 32) && PIN.ver == a.ver && PIN.set == a.set);
    }
    free(s);
    return 0;
}
