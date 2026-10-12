/* BANKPHONE OS: loads the kernel command line from a file, and says when it could not be read whole.
 * BANKPHONE OS: carrega a linha de comando do kernel de um arquivo e diz quando ela não pôde ser lida inteira.
 *
 * Why: the write lock `bankphone.ro=1` is read from this text. The old code read /proc/cmdline into a 4096-byte buffer and
 * cut it silently; a command line of 4097 bytes whose last token is `bankphone.ro=1` became `...bankphone.ro=`, the lock was
 * read as "not set" and the system wrote to the partitions. An unreadable /proc/cmdline had the same effect. A lock must fail
 * CLOSED: when the text is not known whole, the caller treats the lock as ON (see cmdline_prop_safe in cmdline.h).
 * Por quê: a trava de gravação `bankphone.ro=1` é lida deste texto. O código antigo lia /proc/cmdline num buffer de 4096 bytes
 * e cortava em silêncio; uma cmdline de 4097 bytes cujo último token é `bankphone.ro=1` virava `...bankphone.ro=`, a trava era lida
 * como "não definida" e o sistema gravava nas partições. Um /proc/cmdline ilegível tinha o mesmo efeito. Uma trava tem de falhar
 * FECHADA: quando o texto não é conhecido inteiro, quem chama trata a trava como LIGADA (veja cmdline_prop_safe no cmdline.h). */
#pragma once
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <unistd.h>

#define CMDLINE_MAX 16384          /* the kernel's own limit is far below this (2048 or 4096) / o limite do próprio kernel é bem menor */
enum { CMDLINE_OK = 0, CMDLINE_TRUNCATED = 1, CMDLINE_UNREADABLE = 2 };

/* Reads `path` into buf (always NUL-terminated, at most cap-1 bytes kept).
 * Returns CMDLINE_OK, CMDLINE_TRUNCATED (the file has more bytes than fit) or CMDLINE_UNREADABLE (cannot open or read it).
 * Lê `path` em buf (sempre com NUL, no máximo cap-1 bytes guardados).
 * Devolve CMDLINE_OK, CMDLINE_TRUNCATED (o arquivo tem mais bytes do que cabe) ou CMDLINE_UNREADABLE (não abre ou não lê). */
static inline int cmdline_load(const char *path, char *buf, size_t cap)
{
    if (!buf || cap < 2) return CMDLINE_UNREADABLE;
    buf[0] = 0;
    if (!path) return CMDLINE_UNREADABLE;
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return CMDLINE_UNREADABLE;
    size_t got = 0; int st = CMDLINE_OK;
    for (;;) {
        if (got == cap - 1) {                               /* full: is there anything more? / cheio: tem mais alguma coisa? */
            char probe; ssize_t r;
            do r = read(fd, &probe, 1); while (r < 0 && errno == EINTR);
            if (r < 0) st = CMDLINE_UNREADABLE; else if (r > 0) st = CMDLINE_TRUNCATED;
            break;
        }
        ssize_t r = read(fd, buf + got, cap - 1 - got);
        if (r < 0) { if (errno == EINTR) continue; st = CMDLINE_UNREADABLE; break; }
        if (r == 0) break;
        got += (size_t)r;
    }
    buf[got] = 0; close(fd);
    return st;
}
