/*
 * tests/store_fill.c — enche um arquivo com um estado REAL do BANKPHONE.
 *
 * Para que serve: renderizar, no Mac, a tela de Diagnóstico com a área de
 * estado JÁ ARMADA (é o que o aparelho vai mostrar depois do armamento) sem
 * tocar em partição nenhuma. O payload gravado é o de verdade, montado pelas
 * mesmas funções que o aparelho usa (pin_serialize + m_serialize), não é texto
 * de enfeite.
 *
 * Primeiro mede o hash da região, depois TOMA a área com esse hash — o mesmo
 * ritual que o init faz no primeiro boot com `bankphone.statehash=`.
 *
 * Uso:  tests/store_fill.c (compilado) /caminho/da/imagem.img [MiB]
 * Saída: PART/GEO/HASH/ARMADO + o hash16 a usar na cmdline.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>   /* ftruncate/fileno */

#include "../store.h"
#include "../sec.h"
#include "../money.h"

#ifdef BANKPHONE_STORE_TEST
int bp_store_test_hook(int op, unsigned long long off, size_t n)
{ (void)op; (void)off; (void)n; return 0; }
#endif

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "uso: %s /caminho/imagem.img [MiB=20]\n", argv[0]); return 2; }
    const char *caminho = argv[1];
    int mib = argc > 2 ? atoi(argv[2]) : 20;
    if (mib < 2) mib = 2;

    char spec[512];
    snprintf(spec, sizeof spec, "file:%s", caminho);

    FILE *f = fopen(caminho, "r+b");
    if (!f) f = fopen(caminho, "wb");
    if (!f) { perror(caminho); return 1; }
    if (ftruncate(fileno(f), (off_t)mib << 20) != 0) { perror("ftruncate"); fclose(f); return 1; }
    fclose(f);

    /* 1. só mede (sem hash o store recusa a tomada e diz o motivo) */
    store_config(spec, "");
    store_open();
    char hash[24];
    snprintf(hash, sizeof hash, "%s", ST.hash_atual);
    printf("PART bytes=%llu\nGEO base=%llu span=%llu\nHASH region16=%s (virgem=%d)\n",
           ST.part_bytes, ST.base, ST.span, hash, ST.virgin);
    store_close();

    /* 2. toma a área com o hash medido — o mesmo que virá na cmdline */
    store_config(spec, hash);
    if (store_open() != 0) { printf("ARMADO 0 MOTIVO %s\n", ST.why); return 1; }

    /* 3. estado de verdade: PIN (sal aleatório) + livro-razão com histórico */
    uint8_t salt[16];
    for (int i = 0; i < 16; i++) salt[i] = (uint8_t)(rand() & 0xff);
    pin_set("123456", salt);

    time_t agora = time(NULL);
    m_load_demo((int64_t)agora);
    m_receive_pix(25000, "Contato DEMO", (int64_t)agora - 3600 * 5);

    static char buf[200000];
    char pin[8192], mon[190000];
    size_t np = pin_serialize(pin, sizeof pin);
    size_t nm = m_serialize(mon, sizeof mon);
    int o = snprintf(buf, sizeof buf, "%.*s\n%.*s", (int)np, pin, (int)nm, mon);

    int r = store_save(buf, (size_t)o);
    printf("ARMADO %d SAVE %d seqA=%u seqB=%u erro=%s\n", ST.ok, r, ST.seq[0], ST.seq[1], r ? ST.last_msg : "-");
    printf("PAYLOAD %d bytes (pin %zu + ledger %zu)\n", o, np, nm);
    printf("CMDLINE bankphone.state=<partição> bankphone.statehash=%s\n", hash);
    store_close();
    return r == 0 ? 0 : 1;
}
