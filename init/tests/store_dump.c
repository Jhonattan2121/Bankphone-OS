/*
 * tests/store_dump.c — medidor de GEOMETRIA/HASH do store, para conferência.
 *
 * Por que existe: o script scripts/state-region-hash.sh (Python, roda no Mac)
 * calcula a região de estado e o hash dela a partir de um dump de partição. O
 * init do aparelho (store.c) calcula a mesma coisa em C. Duas implementações
 * independentes concordando no MESMO arquivo é o que autoriza confiar no
 * número que o aparelho mostra na tela. Este programa é o lado C.
 *
 * NÃO grava: usa o backend de arquivo, exige um caminho "file:...", e a saída
 * é só leitura de campos já calculados por store_open()/store_load().
 *
 * Uso:  tests/store_dump.c (compilado) file:/caminho/da/particao.img
 * Saída (uma linha por campo, fácil de parsear):
 *   PART bytes=<n>
 *   GEO base=<n> span=<n> sep=<n> erase=<n>
 *   HASH region16=<16 hex>
 *   SLOT A magic=<ascii> estado=<ok|pendente|vazio> seq=<n> len=<n>
 */
#include <stdio.h>
#include <string.h>

#include "../store.h"

/* O gancho de queda de energia existe no build de teste; aqui ele existe e
 * nunca dispara (este medidor não simula queda — só mede). */
#ifdef BANKPHONE_STORE_TEST
int bp_store_test_hook(int op, unsigned long long off, size_t n)
{ (void)op; (void)off; (void)n; return 0; }
#endif

int main(int argc, char **argv)
{
    if (argc < 2 || strncmp(argv[1], "file:", 5) != 0) {
        fprintf(stderr, "uso: %s file:/caminho/da/particao.img\n", argv[0]);
        fprintf(stderr, " (só o backend de arquivo: este medidor nunca abre nó de bloco)\n");
        return 2;
    }

    /* hash vazio de propósito: store_open() calcula a geometria e mede a região
     * e só então recusa a tomada. É exatamente o que queremos medir. */
    store_config(argv[1], "");
    store_open();

    printf("PART bytes=%llu\n", ST.part_bytes);
    printf("GEO base=%llu span=%llu sep=%llu erase=%ld\n",
           ST.base, ST.span, ST.part_bytes - ST.base - ST.span, ST.erase);
    printf("HASH region16=%s\n", ST.hash_atual);
    for (int s = 0; s < 2; s++) {
        const char *est = !ST.slot_ok[s] ? "vazio" : (ST.slot_ok[s] == 2 ? "pendente" : "ok");
        printf("SLOT %c magic=%s estado=%s seq=%u len=%u\n",
               s == 0 ? 'A' : 'B', ST.claimed ? "BS01" : "-", est, ST.seq[s], ST.len[s]);
    }
    printf("VIRGEM %d\n", ST.virgin);
    printf("ARMADO %d MOTIVO %s\n", ST.ok, ST.why);
    store_close();
    return 0;
}
