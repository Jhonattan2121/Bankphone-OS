/* Minimal fuzzing driver: works with any C compiler (gcc, clang, Apple clang), no libFuzzer
 * needed. Used by 'make fuzz' and CI.
 *
 *   fuzz_x <seconds> <dir-or-file>...
 *
 * 1. runs every corpus file once;
 * 2. then, for <seconds>, mutates corpus inputs (byte flips, insertion, removal, chunk copy,
 *    edge values) and runs each result.
 * The seed is fixed by default, so a failure repeats; change it with FUZZ_SEED=n.
 * If an input crashes the program it is written to crash-<hash> in the current directory.
 * With clang you can use real libFuzzer: make fuzz FUZZ_ENGINE=libfuzzer
 *
 * ---- Português ----
 * Driver mínimo de fuzzing: roda com qualquer compilador C (gcc, clang, Apple clang),
 * sem libFuzzer. Serve para o 'make fuzz' e para o CI.
 *
 *   fuzz_x <segundos> <pasta-ou-arquivo>...
 *
 * 1. roda cada arquivo do corpus uma vez;
 * 2. depois, durante <segundos>, muta entradas do corpus (troca de bytes, inserção,
 *    remoção, cópia de trechos, valores de borda) e roda cada resultado.
 * A semente é fixa por padrão, então uma falha se repete. Mude com FUZZ_SEED=n.
 * Se uma entrada derruba o programa, ela é gravada em crash-<hash> na pasta atual.
 *
 * Com clang dá para usar o libFuzzer de verdade (make fuzz FUZZ_ENGINE=libfuzzer). */
#include <dirent.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

int LLVMFuzzerTestOneInput(const uint8_t *d, size_t n);

#define MAXLEN 4096
#define MAXCORPUS 4096
static uint8_t *corp[MAXCORPUS]; static size_t clen[MAXCORPUS]; static int ncorp;
static uint8_t cur[MAXLEN]; static size_t curn;
static uint64_t rs = 0x9e3779b97f4a7c15ull;
static uint64_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return rs; }

static void on_crash(int sig) {
    char name[64]; uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < curn; i++) { h ^= cur[i]; h *= 1099511628211ull; }
    snprintf(name, sizeof name, "crash-%016llx", (unsigned long long)h);
    FILE *f = fopen(name, "wb"); if (f) { fwrite(cur, 1, curn, f); fclose(f); }
    fprintf(stderr, "\nFALHA (sinal %d). Entrada gravada em %s (%zu bytes)\n", sig, name, curn);
    _exit(1);
}

static void add(const uint8_t *d, size_t n) {
    if (ncorp >= MAXCORPUS || n > MAXLEN) return;
    corp[ncorp] = malloc(n ? n : 1); memcpy(corp[ncorp], d, n); clen[ncorp++] = n;
}
static void load(const char *path) {
    struct stat st; if (stat(path, &st)) return;
    if (S_ISDIR(st.st_mode)) {
        DIR *dp = opendir(path); struct dirent *e; if (!dp) return;
        while ((e = readdir(dp))) {
            if (e->d_name[0] == '.') continue;
            char p[1024]; snprintf(p, sizeof p, "%s/%s", path, e->d_name); load(p);
        }
        closedir(dp);
    } else {
        FILE *f = fopen(path, "rb"); if (!f) return;
        uint8_t *b = malloc(MAXLEN); size_t n = fread(b, 1, MAXLEN, f); fclose(f);
        add(b, n); free(b);
    }
}
static void run(const uint8_t *d, size_t n) {
    memcpy(cur, d, n); curn = n;
    LLVMFuzzerTestOneInput(cur, curn);
}
static const uint8_t EDGE[] = {0x00, 0x01, 0x7f, 0x80, 0xff, '\n', '\r', '|', '-', '0', '9', 'A', ' '};
static void mutate(void) {
    if (ncorp) { size_t i = rnd() % ncorp; curn = clen[i]; memcpy(cur, corp[i], curn); } else curn = 0;
    int rounds = 1 + (int)(rnd() % 4);
    while (rounds--) {
        switch (rnd() % 6) {
        case 0: if (curn) cur[rnd() % curn] = (uint8_t)rnd(); break;
        case 1: if (curn) cur[rnd() % curn] = EDGE[rnd() % sizeof EDGE]; break;
        case 2: if (curn < MAXLEN - 1) { size_t p = curn ? rnd() % (curn + 1) : 0; memmove(cur + p + 1, cur + p, curn - p); cur[p] = EDGE[rnd() % sizeof EDGE]; curn++; } break;
        case 3: if (curn) { size_t p = rnd() % curn; memmove(cur + p, cur + p + 1, curn - p - 1); curn--; } break;
        case 4: if (curn > 1 && curn < MAXLEN / 2) { size_t a = rnd() % curn, l = 1 + rnd() % (curn - a), p = rnd() % (curn + 1); if (curn + l < MAXLEN) { memmove(cur + p + l, cur + p, curn - p); memcpy(cur + p, cur + (a >= p ? a + l : a), l); curn += l; } } break;
        case 5: if (curn) cur[rnd() % curn] ^= (uint8_t)(1u << (rnd() % 8)); break;
        }
    }
}
int main(int argc, char **argv) {
    int secs = argc > 1 ? atoi(argv[1]) : 10;
    if (getenv("FUZZ_SEED")) rs ^= strtoull(getenv("FUZZ_SEED"), NULL, 10) * 0x2545f4914f6cdd1dull;
    for (int i = 2; i < argc; i++) load(argv[i]);
    signal(SIGABRT, on_crash); signal(SIGSEGV, on_crash); signal(SIGFPE, on_crash); signal(SIGILL, on_crash); signal(SIGBUS, on_crash);
    for (int i = 0; i < ncorp; i++) run(corp[i], clen[i]);
    unsigned long long execs = ncorp; time_t end = time(NULL) + secs;
    while (time(NULL) < end) {
        for (int k = 0; k < 2000; k++) { mutate(); LLVMFuzzerTestOneInput(cur, curn); execs++; }
        if (curn < MAXLEN && rnd() % 8 == 0) add(cur, curn);   /* keep some mutations as new seeds / guarda algumas mutações como novas sementes */
    }
    printf("ok: %llu execucoes, %d sementes\n", execs, ncorp);
    return 0;
}
