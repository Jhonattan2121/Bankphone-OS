/* Secure random bytes: fail instead of falling back to something predictable.
 *
 * The PIN salt must be unpredictable. The old plat_random() fell back to a value computed from
 * the uptime when /dev/urandom could not be opened, and ignored short reads, so the caller could
 * not tell a bad salt from a good one. Here every path returns 0 only if ALL the bytes came from
 * the kernel, and -1 otherwise.
 *
 * ---- Português ----
 * Bytes aleatórios seguros: falha em vez de cair num valor previsível.
 *
 * O sal do PIN precisa ser imprevisível. O plat_random() antigo, se não abrisse /dev/urandom,
 * usava um valor calculado a partir do uptime, e ignorava leituras curtas; quem chamava não
 * distinguia sal ruim de sal bom. Aqui todo caminho devolve 0 só se TODOS os bytes vieram do
 * kernel, e -1 caso contrário. */
#pragma once
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/random.h>
#endif

/* Read exactly n bytes from a file (retries on EINTR and short reads). 0 ok, -1 failure.
 * Lê exatamente n bytes de um arquivo (repete em EINTR e leitura curta). 0 ok, -1 falha. */
static inline int entropy_read_file(const char *path, uint8_t *b, size_t n)
{
    int f = open(path, O_RDONLY | O_CLOEXEC);
    if (f < 0) return -1;
    size_t got = 0;
    while (got < n) {
        ssize_t r = read(f, b + got, n - got);
        if (r < 0 && errno == EINTR) continue;
        if (r <= 0) { close(f); return -1; }            /* error or EOF: never pad / erro ou EOF: nunca completa */
        got += (size_t)r;
    }
    close(f);
    return 0;
}

/* Fill b with n random bytes from the kernel. 0 ok, -1 failure (b is zeroed on failure).
 * Enche b com n bytes aleatórios do kernel. 0 ok, -1 falha (b é zerado na falha). */
static inline int entropy_fill(uint8_t *b, size_t n)
{
    int rc = -1;
#ifdef __linux__
    size_t got = 0; int usable = 1;
    while (got < n) {                                   /* getrandom() blocks until the pool is seeded / bloqueia até o pool estar semeado */
        ssize_t r = getrandom(b + got, n - got, 0);
        if (r < 0 && errno == EINTR) continue;
        if (r <= 0) { usable = 0; break; }
        got += (size_t)r;
    }
    if (usable) rc = 0;
#endif
    if (rc != 0) rc = entropy_read_file("/dev/urandom", b, n);
    if (rc != 0) for (size_t i = 0; i < n; i++) ((volatile uint8_t *)b)[i] = 0;
    return rc;
}
