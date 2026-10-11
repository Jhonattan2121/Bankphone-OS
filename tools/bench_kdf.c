/* Measure the PIN KDF cost on THIS machine (or on the phone, cross-compiled).
 * Mede o custo do KDF do PIN NESTA máquina (ou no celular, compilado para ele).
 *
 *   make bench-kdf
 *
 * Prints the time per guess for the legacy verifier and for scrypt at a few costs, and how long
 * one core would need to try every 6-digit PIN. These are measurements of one CPU core, not of a
 * GPU or an ASIC, which are not measured here.
 * Mostra o tempo por tentativa do verificador antigo e do scrypt em alguns custos, e quanto tempo um
 * núcleo levaria para testar todos os PINs de 6 dígitos. São medidas de um núcleo de CPU, não de GPU
 * nem de ASIC, que não são medidos aqui. */
#include "../init/sec.h"
#include <stdio.h>
#include <time.h>

static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (double)t.tv_sec + (double)t.tv_nsec / 1e9; }

int main(void) {
    uint8_t salt[16] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16}, h[32], v[32], k[32];
    double t0 = now(); for (int i = 0; i < 5; i++) pin_derive("123456", salt, h);
    double leg = (now() - t0) / 5;
    printf("legacy (SHA-256 x 50,000):        %8.1f ms per guess | all 10^6 six-digit PINs: %.1f h on one core\n", leg * 1000, leg * 1e6 / 3600);
    static const uint32_t cfg[][3] = {{14, 8, 1}, {15, 8, 1}, {16, 8, 1}};
    for (int c = 0; c < 3; c++) {
        t0 = now(); for (int i = 0; i < 3; i++) if (pin_derive_v2("123456", salt, cfg[c][0], cfg[c][1], cfg[c][2], v, k)) { puts("scrypt failed"); return 1; }
        double t = (now() - t0) / 3;
        printf("scrypt N=2^%u r=%u p=%u (%3u MiB): %8.1f ms per guess | all 10^6 six-digit PINs: %.1f h on one core\n",
               cfg[c][0], cfg[c][1], cfg[c][2], (128u * cfg[c][1] << cfg[c][0]) >> 20, t * 1000, t * 1e6 / 3600);
    }
    return 0;
}
