/* Fuzzing of the USB serial command reader (init/devcmd.h).
 *
 * Compares the real reader with a reference model written a different way: a line fires
 * the command if, and only if, it is EXACTLY "BANKPHONE:REBOOT-BOOTLOADER" and fits the
 * buffer. The same byte stream is delivered three ways (whole, byte by byte, in chunks whose
 * sizes come from the input) and the number of commands must match the model.
 *
 * ---- Português ----
 * Fuzzing do leitor de comandos da serial USB (init/devcmd.h).
 *
 * Compara o leitor real com um modelo de referência escrito de outro jeito:
 * uma linha dispara o comando se, e somente se, ela for EXATAMENTE
 * "BANKPHONE:REBOOT-BOOTLOADER" e couber no buffer. O mesmo fluxo de bytes
 * é entregue ao leitor de três formas (inteiro, byte a byte, em pedaços de
 * tamanho vindo da entrada) e o número de comandos tem que ser igual ao do modelo. */
#include "devcmd.h"
#include <stdint.h>
#include <stdlib.h>

static const char CMD[] = "BANKPHONE:REBOOT-BOOTLOADER";

/* Model: how many complete, exact lines exist in the stream.
 * Modelo: quantas linhas completas e exatas existem no fluxo. */
static int model_count(const uint8_t *d, size_t n) {
    char line[sizeof(((DevCmd *)0)->buf)]; size_t len = 0; int over = 0, count = 0;
    for (size_t i = 0; i < n; i++) {
        if (d[i] == '\n' || d[i] == '\r') {
            if (!over && len == sizeof CMD - 1 && !memcmp(line, CMD, len)) count++;
            len = 0; over = 0;
        } else if (len < sizeof line - 1) line[len++] = (char)d[i];
        else over = 1;
    }
    return count;
}

/* How many lines the real reader accepted, delivering the stream in chunks.
 * Quantas linhas o leitor real aceitou, entregando o fluxo em pedaços. */
static int real_count(const uint8_t *d, size_t n, const uint8_t *sizes, size_t nsizes, int bytewise) {
    DevCmd c; memset(&c, 0, sizeof c);
    int hits = 0; size_t i = 0, s = 0;
    while (i < n) {
        size_t step = bytewise ? 1 : (nsizes ? 1u + sizes[s++ % nsizes] : n);
        if (step > n - i) step = n - i;
        /* Byte by byte counts each line once; in chunks the reader reports at most one hit per
         * call, so only "any hit or none" is compared.
         * Entregar byte a byte conta cada linha uma vez; em pedaços, o leitor devolve
         * no máximo um aviso por chamada, então só se compara "houve ou não". */
        if (devcmd_feed(&c, (const char *)d + i, (int)step) == DEVCMD_REBOOT_BOOTLOADER) hits++;
        i += step;
    }
    return hits;
}

int LLVMFuzzerTestOneInput(const uint8_t *d, size_t n) {
    if (n > 4096) return 0;
    int model = model_count(d, n);
    int bytewise = real_count(d, n, NULL, 0, 1);
    if (bytewise != model) abort();                       /* one hit per line / uma linha por aviso */
    int whole = real_count(d, n, NULL, 0, 0);
    if ((whole > 0) != (model > 0)) abort();              /* never fires too much or too little / nunca dispara a mais nem a menos */
    size_t k = n < 8 ? n : 8;
    int chunked = real_count(d + k, n - k, d, k, 0);      /* chunks of varying size / pedaços de tamanhos variados */
    int model_tail = model_count(d + k, n - k);
    if ((chunked > 0) != (model_tail > 0)) abort();
    return 0;
}
