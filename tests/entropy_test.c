/* Tests for init/entropy.h / Testes do init/entropy.h */
#include "entropy.h"
#include <stdio.h>
#include <string.h>
static int fails;
#define CHECK(n, c) do { int ok_ = (c); printf("%s %s\n", ok_ ? "PASS" : "FAIL", n); if (!ok_) fails++; } while (0)

int main(void)
{
    uint8_t a[32], b[32], z[32] = {0};
    memset(a, 0, sizeof a); memset(b, 0, sizeof b);
    CHECK("entropy_fill ok", entropy_fill(a, sizeof a) == 0);
    CHECK("second call ok", entropy_fill(b, sizeof b) == 0);
    CHECK("two draws differ", memcmp(a, b, sizeof a) != 0);
    CHECK("draw is not all zero", memcmp(a, z, sizeof a) != 0);
    CHECK("zero length ok", entropy_fill(a, 0) == 0);

    /* file source: a file with fewer bytes than asked is a failure, never padded */
    const char *p = "/tmp/bp-entropy-test.bin";
    FILE *f = fopen(p, "wb"); fwrite("0123456789", 1, 10, f); fclose(f);
    uint8_t c[16]; memset(c, 0xAA, sizeof c);
    CHECK("short file is refused", entropy_read_file(p, c, 16) == -1);
    CHECK("exact file is accepted", entropy_read_file(p, c, 10) == 0 && !memcmp(c, "0123456789", 10));
    CHECK("missing file is refused", entropy_read_file("/tmp/bp-entropy-nao-existe", c, 4) == -1);
    CHECK("empty file is refused", (f = fopen(p, "wb"), fclose(f), entropy_read_file(p, c, 1) == -1));
    remove(p);

    printf(fails ? "%d FALHAS\n" : "TODOS OK\n", fails);
    return fails != 0;
}
