/* Fuzzing of the kernel command line parser (init/cmdline.h), against an independent reference model.
 * Fuzzing do parser da linha de comando do kernel, contra um modelo de referência independente.
 *
 * Input: byte 0 picks the key from a table, byte 1 gives the output size (1..80), the rest is the cmdline.
 * Entrada: o byte 0 escolhe a chave de uma tabela, o byte 1 dá o tamanho da saída (1..80), o resto é a cmdline.
 *
 * The model tokenizes with strtok_r and splits at the first '=', a different route from the real parser, which
 * walks the text by hand. Checks: same result code, same text, always NUL-terminated, and nothing written
 * past the buffer (canary bytes).
 * O modelo separa com strtok_r e divide no primeiro '=', um caminho diferente do parser real, que anda no
 * texto à mão. Confere: mesmo código, mesmo texto, sempre com NUL, e nada escrito além do buffer (canário). */
#include "cmdline.h"
#include <stdint.h>
#include <stdlib.h>

#define NEED(c) do { if (!(c)) abort(); } while (0)

static const char *const KEYS[] = {
    "bankphone.ro", "bankphone.state", "bankphone.statehash", "bankphone.devcmd", "bankphone.fastboot",
    "a", "ro", "x", "androidboot.serialno", "", "has space", "with=equal", "bankphone.rox",
};
#define NKEYS ((int)(sizeof KEYS / sizeof KEYS[0]))

static int model(const char *cl, const char *key, char *want, size_t n) {
    size_t kl = strlen(key);
    if (!kl || strchr(key, '=') || strpbrk(key, " \t\r\n")) { want[0] = 0; return 0; }
    char *copy = strdup(cl), *save = NULL; int found = 0; const char *last = NULL;
    for (char *tok = strtok_r(copy, " \t\r\n", &save); tok; tok = strtok_r(NULL, " \t\r\n", &save)) {
        char *eq = strchr(tok, '=');
        if (eq && (size_t)(eq - tok) == kl && !strncmp(tok, key, kl)) { found = 1; last = eq + 1; }
    }
    int res = 0;
    if (found) { size_t vl = strlen(last), c = vl < n - 1 ? vl : n - 1; memcpy(want, last, c); want[c] = 0; res = vl > n - 1 ? -1 : 1; }
    else want[0] = 0;
    free(copy);
    return res;
}

int LLVMFuzzerTestOneInput(const uint8_t *d, size_t len) {
    if (len < 2 || len > 4096) return 0;
    const char *key = KEYS[d[0] % NKEYS];
    size_t n = 1u + d[1] % 80u;
    char *cl = malloc(len - 1); memcpy(cl, d + 2, len - 2); cl[len - 2] = 0;

    char want[128], *got = malloc(n + 8); memset(got, 0x5A, n + 8);
    int rm = model(cl, key, want, n);
    int rg = cmdline_get(cl, key, got, n);
    NEED(rg == rm);
    NEED(!strcmp(got, want));                              /* same text, and NUL-terminated inside n bytes */
    NEED(memchr(got, 0, n) != NULL);
    for (size_t i = n; i < n + 8; i++) NEED((unsigned char)got[i] == 0x5A);   /* the canary was not touched */

    const char *p = cmdline_prop(cl, key);                  /* the pointer version agrees (63 usable bytes) */
    char want63[CMDLINE_VALUE_MAX]; model(cl, key, want63, sizeof want63);
    NEED(!strcmp(p, want63));
    free(got); free(cl);
    return 0;
}
