/*
 * BANKPHONE OS — TESTE DO ESTADO PERSISTENTE (F2), roda no HOST (x86_64/macOS).
 *
 * Sem aparelho não dá para desligar a energia no meio de um pwrite. Este teste
 * faz o equivalente: liga um GANCHO que falha a operação de I/O número N, o que
 * é exatamente o que uma queda de energia provoca naquele ponto. Depois
 * "reinicia" (store_close + store_open) e confere o que o sistema leria.
 *
 * Propriedade que o teste exige, para TODOS os passos de gravação:
 *     depois de qualquer queda, a leitura devolve o ÚLTIMO ESTADO BOM
 *     (ou vazio, se nunca houve gravação completa) — nunca lixo, nunca metade.
 */
#define _GNU_SOURCE
#include "../store.h"
#include "../sec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

/* ------------------------------------------------------------- contadores -- */
static int falhas, checks;
#define CHK(cond, ...) do { checks++; if (!(cond)) { falhas++; \
    printf("  [FALHA] %s:%d  ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* ---------------------------------------------------- gancho de queda ------ */
static int g_fail_op  = 0;     /* 1 escrita · 2 fsync · 3 leitura          */
static int g_fail_at  = -1;    /* qual operação DAQUELE TIPO deve falhar   */
static int g_cnt[4];           /* contador por tipo (escrita/fsync/leitura) */
static int g_armed    = 0;     /* liga/desliga o gancho                     */

int bp_store_test_hook(int op, unsigned long long off, size_t n)
{
    (void)off; (void)n;
    if (!g_armed) return 0;
    if (op < 1 || op > 3) return 0;
    g_cnt[op]++;
    if (op == g_fail_op && g_cnt[op] == g_fail_at) {
        g_armed = 0;                 /* queda de energia: acabou o "processo" */
        return 1;
    }
    return 0;
}

static void hook_arm(int op, int k)
{
    g_fail_op = op; g_fail_at = k; g_armed = 1; g_cnt[1] = g_cnt[2] = g_cnt[3] = 0;
}

/* --------------------------------------------------------------- arquivo --- */
#define PART_BYTES (20ull << 20)     /* 20 MiB: o tamanho real do expdb */
#define PATH "/tmp/bp-store-test.img"

static void mkpart(int preencher)
{
    FILE *f = fopen(PATH, "wb");
    if (!f) { printf("não consegui criar %s\n", PATH); exit(2); }
    unsigned char blk[4096];
    memset(blk, preencher ? 0xFF : 0x00, sizeof blk);
    for (unsigned long long o = 0; o < PART_BYTES; o += sizeof blk) fwrite(blk, 1, sizeof blk, f);
    fclose(f);
}

static void sujar(int seed)
{
    /* imita o expdb real: lixo de crash log ANTES da região e também DENTRO dela
     * (a região é a última parte da partição, então pode ter sobrado lixo lá) */
    FILE *f = fopen(PATH, "r+b");
    unsigned char *blk = malloc((size_t)ST.span);
    srand(seed);
    for (size_t k = 0; k < (size_t)ST.span; k++) blk[k] = (unsigned char)rand();
    fseek(f, (long)ST.base, SEEK_SET);
    fwrite(blk, 1, (size_t)ST.span, f);
    free(blk);
    unsigned char b2[4096];
    for (int i = 0; i < 50; i++) {
        for (size_t k = 0; k < sizeof b2; k++) b2[k] = (unsigned char)rand();
        fseek(f, (long)(rand() % (int)(PART_BYTES / 2)), SEEK_SET);
        fwrite(b2, 1, sizeof b2, f);
    }
    fclose(f);
}

static void regiao_hash(char out[24])
{
    unsigned char *reg = malloc((size_t)ST.span);
    FILE *f = fopen(PATH, "rb");
    fseek(f, (long)ST.base, SEEK_SET);
    if (fread(reg, 1, (size_t)ST.span, f) != ST.span) { printf("leitura curta\n"); exit(2); }
    fclose(f);
    unsigned char h[32];
    sha256(reg, (size_t)ST.span, h);
    static const char *H = "0123456789abcdef";
    for (int i = 0; i < 8; i++) { out[i*2] = H[h[i] >> 4]; out[i*2+1] = H[h[i] & 15]; }
    out[16] = 0;
    free(reg);
}

/* lê tudo que está FORA da região, para provar que o store não escreve lá */
static unsigned long long fora_hash(void)
{
    FILE *f = fopen(PATH, "rb");
    unsigned char buf[65536];
    unsigned long long h = 1469598103934665603ull;   /* FNV-1a, só para comparar */
    unsigned long long lidos = 0;
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) {
        for (size_t i = 0; i < n; i++) {
            unsigned long long pos = lidos + i;
            if (pos >= ST.base && pos < ST.base + ST.span) continue;
            h ^= buf[i]; h *= 1099511628211ull;
        }
        lidos += n;
    }
    fclose(f);
    return h;
}

static void log_print(const char *m) { printf("    store: %s\n", m); }

static int abrir(const char *hash)
{
    store_close();
    store_config("file:" PATH, hash);
    return store_open();
}

/* simula o reinício: fecha e reabre com o MESMO hash (a região não mudou) */
static int reiniciar(void)
{
    char h[24];
    /* o hash de armamento segue valendo porque o store não muda o conteúdo
     * fora dos slots... mas os slots MUDAM. Então o armamento só é aceito com o
     * hash original: no aparelho isso é o hash do backup. Aqui guardamos. */
    FILE *f = fopen("/tmp/bp-store-hash.txt", "r");
    if (!f) return -1;
    if (fscanf(f, "%23s", h) != 1) { fclose(f); return -1; }
    fclose(f);
    return abrir(h);
}

int main(void)
{
    printf("=== TESTE DO ESTADO PERSISTENTE (F2) — host ===\n");
    store_set_log(log_print);

    /* ---------------------------------------------------------------- 1 -- */
    printf("\n[1] sem cmdline: só memória\n");
    store_config(NULL, NULL);
    CHK(store_open() != 0, "deveria recusar sem cmdline");
    CHK(!ST.pedido, "pedido deveria ser 0");
    CHK(strstr(ST.why, "não pedida") != NULL, "motivo deveria citar a cmdline: %s", ST.why);

    /* ---------------------------------------------------------------- 2 -- */
    printf("\n[2] com partição, mas SEM hash do backup: RECUSA\n");
    mkpart(0);
    CHK(abrir(NULL) != 0, "deveria recusar sem hash");
    CHK(ST.pedido && !ST.armed && !ST.ok, "estado deveria ser pedido+nao armado");
    CHK(strstr(ST.why, "RECUSADO") && strstr(ST.why, "statehash"), "motivo: %s", ST.why);
    CHK(ST.refusals >= 1, "recusas deveriam ser contadas");

    /* ---------------------------------------------------------------- 3 -- */
    printf("\n[3] com hash ERRADO: RECUSA e não escreve nada\n");
    unsigned long long antes = fora_hash();
    {
        char h[24]; regiao_hash(h);
        char errado[24]; memcpy(errado, h, 24); errado[0] = (errado[0] == '0') ? '1' : '0';
        CHK(abrir(errado) != 0, "deveria recusar com hash diferente");
        CHK(strstr(ST.why, "MUDOU desde o backup") != NULL, "motivo: %s", ST.why);
        /* e o arquivo tem que estar intacto: a recusa não pode ter escrito */
        char h2[24]; regiao_hash(h2);
        CHK(!strcmp(h, h2), "a região mudou apesar da recusa (%s -> %s)", h, h2);
    }
    CHK(fora_hash() == antes, "escreveu fora da região durante a recusa");

    /* ---------------------------------------------------------------- 4 -- */
    printf("\n[4] região virgem + hash certo: ARMA (tomada da área)\n");
    char hash[24]; regiao_hash(hash);
    CHK(abrir(hash) == 0, "deveria armar: %s", ST.why);
    CHK(ST.armed && ST.ok, "deveria estar armado");
    CHK(ST.claim_now == 1, "deveria ser marcada como TOMADA AGORA");
    CHK(ST.virgin == 1, "região zerada deveria ser detectada como virgem");
    CHK(ST.slot_ok[0] == 0 && ST.slot_ok[1] == 0, "slots deveriam começar vazios");
    CHK(!strcmp(ST.hash_atual, hash), "hash medido deveria ser o do backup");
    printf("    geometria: part=%llu KiB base=%llu KiB span=%llu KiB sep=%llu KiB erase=%ld\n",
           ST.part_bytes / 1024, ST.base / 1024, ST.span / 1024, (ST.span - ST_SLOT_BYTES) / 1024, ST.erase);

    /* guarda o hash para os "reinícios" */
    { FILE *f = fopen("/tmp/bp-store-hash.txt", "w"); fprintf(f, "%s\n", hash); fclose(f); }

    /* ---------------------------------------------------------------- 5 -- */
    printf("\n[5] primeira gravação: slot A, seq=1, lida de volta\n");
    const char *A = "P1|pin=1|tx=3|estado=A";
    CHK(store_save(A, strlen(A)) == 0, "gravação 1 falhou: %s", ST.last_msg);
    CHK(ST.slot_ok[0] == 1 && ST.seq[0] == 1, "slot A deveria ficar ok seq=1 (%d/%u)", ST.slot_ok[0], ST.seq[0]);
    CHK(strstr(ST.last_msg, "lida de volta") != NULL, "mensagem deveria citar leitura de volta: %s", ST.last_msg);
    {   /* slot B ainda VAZIO não é recuperação: é o primeiro boot da área */
        char buf[512]; size_t n = 0;
        CHK(store_load(buf, sizeof buf, &n) == 0, "load falhou");
        CHK(ST.recovered == 0, "área nova com um slot só não é RECUPERAÇÃO (recovered=%d)", ST.recovered);
    }

    /* ---------------------------------------------------------------- 6 -- */
    printf("\n[6] segunda gravação: slot B, seq=2, A intacto\n");
    const char *B = "P1|pin=1|tx=4|estado=B";
    CHK(store_save(B, strlen(B)) == 0, "gravação 2 falhou: %s", ST.last_msg);
    CHK(ST.slot_ok[1] == 1 && ST.seq[1] == 2, "slot B deveria ser seq=2 (%d/%u)", ST.slot_ok[1], ST.seq[1]);
    CHK(ST.slot_ok[0] == 1 && ST.seq[0] == 1, "slot A deveria seguir intacto");

    /* ---------------------------------------------------------------- 7 -- */
    printf("\n[7] leitura devolve o estado mais novo\n");
    {
        char buf[512]; size_t n = 0;
        CHK(store_load(buf, sizeof buf, &n) == 0, "load falhou");
        buf[n] = 0;
        CHK(strcmp(buf, B) == 0, "deveria ler '%s', leu '%s'", B, buf);
    }

    /* ---------------------------------------------------------------- 8 -- */
    printf("\n[8] conteúdo do slot mais novo corrompido -> cai para o anterior\n");
    {
        FILE *f = fopen(PATH, "r+b");
        unsigned long long off = ST.base + (ST.span - ST_SLOT_BYTES) + 24 + 3;
        fseek(f, (long)off, SEEK_SET);
        unsigned char x = 0x77; fwrite(&x, 1, 1, f);
        fclose(f);
        CHK(reiniciar() == 0, "reabrir falhou");
        char buf[512]; size_t n = 0;
        int r = store_load(buf, sizeof buf, &n);
        buf[n > 0 ? n : 0] = 0;
        CHK(r == 0, "load deveria funcionar pelo slot bom");
        CHK(strcmp(buf, A) == 0, "deveria voltar ao estado A, veio '%s'", buf);
        CHK(ST.recovered == 1, "deveria marcar recuperação");
    }

    /* ---------------------------------------------------------------- 9 -- */
    printf("\n[9] cabeçalho do slot mais novo destruído -> mesmo comportamento\n");
    {
        FILE *f = fopen(PATH, "r+b");
        fseek(f, (long)(ST.base + (ST.span - ST_SLOT_BYTES)), SEEK_SET);
        unsigned char lixo[8] = { 0xDE, 0xAD, 0xBE, 0xEF, 0, 0, 0, 0 };
        fwrite(lixo, 1, sizeof lixo, f);
        fclose(f);
        CHK(reiniciar() == 0, "reabrir falhou");
        char buf[512]; size_t n = 0;
        CHK(store_load(buf, sizeof buf, &n) == 0, "load deveria vir do slot A");
        buf[n] = 0;
        CHK(strcmp(buf, A) == 0, "deveria ser A, veio '%s'", buf);
    }

    /* --------------------------------------------------------------- 10 -- */
    printf("\n[10] slot PENDENTE (escrita interrompida) é detectado e o outro é usado\n");
    {
        mkpart(0);
        char h[24]; regiao_hash(h);
        CHK(abrir(h) == 0, "armar de novo falhou: %s", ST.why);
        CHK(store_save(A, strlen(A)) == 0, "gravar A falhou");
        CHK(store_save(B, strlen(B)) == 0, "gravar B falhou");
        /* deixa o slot A com marcador PENDENTE, como ficaria após uma queda */
        hook_arm(ST_OP_WRITE, 2);          /* 1ª escrita = cabeçalho PENDENTE; 2ª = payload */
        int rc = store_save("C|meio-escrito", 14);
        g_armed = 0;
        CHK(rc != 0, "a gravação interrompida deveria falhar");
        CHK(ST.failures >= 1, "falhas deveriam ser contadas");
        CHK(reiniciar() == 0, "reabrir falhou");
        CHK(ST.slot_ok[0] == 2, "slot A deveria estar PENDENTE (ok=%d)", ST.slot_ok[0]);
        CHK(ST.slot_ok[1] == 1, "slot B deveria estar ok");
        char buf[512]; size_t n = 0;
        CHK(store_load(buf, sizeof buf, &n) == 0, "load deveria usar o slot B");
        buf[n] = 0;
        CHK(strcmp(buf, B) == 0, "deveria ser B, veio '%s'", buf);
        CHK(store_heal() == 0, "cura deveria agir no slot pendente");
        CHK(ST.slot_ok[0] == 0, "slot A deveria ficar vazio depois da cura");
    }

    /* --------------------------------------------------------------- 11 -- */
    printf("\n[11] QUEDA DE ENERGIA EM CADA PASSO da gravação\n");
    {
        /* Estado anterior conhecido: A no slot mais antigo, B no mais novo.
         * (No reinício o armamento vale pela assinatura BS01, então o hash
         *  antigo continua servindo — é o mesmo que acontece no aparelho.) */
        const char *NOVO = "P1|pin=1|tx=9|novo";
        int disparou[4] = {0,0,0,0};
        for (int op = 1; op <= 3; op++) {
            for (int k = 1; k <= 6; k++) {
                mkpart(0);
                char hh[24]; regiao_hash(hh);
                CHK(abrir(hh) == 0, "armar falhou: %s", ST.why);
                CHK(store_save(A, strlen(A)) == 0, "gravar A falhou");
                CHK(store_save(B, strlen(B)) == 0, "gravar B falhou");

                hook_arm(op, k);
                int rc = store_save(NOVO, strlen(NOVO));
                int fize = g_armed == 0;      /* o gancho disparou em algum momento? */
                g_armed = 0;
                if (fize) disparou[op]++;

                /* "reinício": fecha, reabre, lê */
                CHK(reiniciar() == 0, "reabrir falhou");
                char buf[512]; size_t n = 0;
                int r = store_load(buf, sizeof buf, &n);
                buf[n > 0 ? n : 0] = 0;

                /* I1 — NUNCA um estado rasgado: só A, B ou o novo completo */
                CHK(r == 0 && (!strcmp(buf, A) || !strcmp(buf, B) || !strcmp(buf, NOVO)),
                    "op=%d k=%d: leu estado inválido (rc=%d) '%s'", op, k, r, buf);

                /* I2 — gravação que diz OK tem de ser legível como o estado novo */
                if (rc == 0)
                    CHK(strcmp(buf, NOVO) == 0, "op=%d k=%d: save disse OK mas leu '%s'", op, k, buf);

                /* I3 — queda ANTES do conteúdo ficar completo não pode virar o novo estado */
                int antes = (op == 1 && k <= 3) || (op == 2 && k <= 2);
                if (fize && antes && rc != 0)
                    CHK(strcmp(buf, NOVO) != 0,
                        "op=%d k=%d: queda antes do fim e mesmo assim leu o novo estado", op, k);

                /* I4 — queda em ESCRITA (dado ainda não no ar) tem de preservar o estado
                 * anterior. Para fsync a partir do 3º, o conteúdo já foi escrito: no
                 * backend de arquivo o "desligamento" não apaga o que já está no arquivo,
                 * então aí o novo estado também é aceito (é limitação do teste, não do
                 * protocolo — na memória flash o fsync é justamente o que fecha a conta). */
                if (fize && rc != 0 && (op == 1 || (op == 2 && k <= 2)))
                    CHK(!strcmp(buf, A) || !strcmp(buf, B),
                        "op=%d k=%d: perdeu o estado anterior (leu '%s')", op, k, buf);
            }
        }
        printf("    (quedas efetivas por tipo de operação: escrita=%d fsync=%d leitura=%d)\n",
               disparou[1], disparou[2], disparou[3]);
        CHK(disparou[1] >= 3 && disparou[2] >= 2, "o teste precisa disparar quedas de escrita e fsync");
    }

    /* --------------------------------------------------------------- 12 -- */
    printf("\n[12] região com conteúdo (não virgem) mas hash certo: ARMA; e depois já-nossa dispensa hash\n");
    {
        /* descobre a geometria primeiro (armando com hash vazio não escreve nada) */
        mkpart(0);
        { char h0[24]; regiao_hash(h0); abrir(h0); }
        sujar(1234);
        char h[24]; regiao_hash(h);
        CHK(abrir(h) == 0, "deveria armar região suja com hash do backup: %s", ST.why);
        CHK(ST.virgin == 0, "deveria detectar que a região NÃO é virgem");
        CHK(ST.claim_now == 1, "primeira tomada");
        CHK(store_save(A, strlen(A)) == 0, "gravação sobre região suja falhou");
        printf("    (é exatamente o caso do expdb real: lixo do LK + backup conferido)\n");

        /* reinício com hash ERRADO: a área já é nossa, então arma e não escreve nada de errado */
        char errado[24]; memcpy(errado, h, 24); errado[0] = (errado[0] == '0') ? '1' : '0';
        CHK(abrir(errado) == 0, "área já nossa deveria armar mesmo com hash velho: %s", ST.why);
        CHK(ST.claimed == 1, "deveria estar marcada como JÁ era nossa");
        {
            char buf[512]; size_t n = 0;
            CHK(store_load(buf, sizeof buf, &n) == 0, "load deveria funcionar");
            buf[n] = 0;
            CHK(strcmp(buf, A) == 0, "deveria ler A, leu '%s'", buf);
        }
    }

    /* --------------------------------------------------------------- 13 -- */
    printf("\n[13] partição pequena demais: RECUSA com motivo\n");
    {
        FILE *f = fopen("/tmp/bp-store-tiny.img", "wb");
        unsigned char z[4096] = {0};
        for (int i = 0; i < 8; i++) fwrite(z, 1, sizeof z, f);     /* 32 KiB */
        fclose(f);
        store_close();
        store_config("file:/tmp/bp-store-tiny.img", "0000000000000000");
        CHK(store_open() != 0, "deveria recusar partição pequena");
        CHK(strstr(ST.why, "pequena demais") != NULL, "motivo: %s", ST.why);
        unlink("/tmp/bp-store-tiny.img");
    }

    /* --------------------------------------------------------------- 14 -- */
    printf("\n[14] o store NUNCA escreve fora da região\n");
    {
        mkpart(0);
        char h[24]; regiao_hash(h);
        abrir(h);
        unsigned long long antes2 = fora_hash();
        for (int i = 0; i < 6; i++) { char b[64]; snprintf(b, sizeof b, "P1|pin=1|tx=%d", i); store_save(b, strlen(b)); }
        CHK(fora_hash() == antes2, "escreveu fora da região de estado");
        CHK(ST.saves >= 6, "contador de gravações deveria ser >= 6 (%u)", ST.saves);
    }

    /* --------------------------------------------------------------- 15 -- */
    printf("\n[15] alternância dos slots em 6 gravações seguidas\n");
    {
        CHK(ST.seq[0] != ST.seq[1], "os dois slots não deveriam ter a mesma seq");
        char buf[512]; size_t n = 0;
        store_load(buf, sizeof buf, &n); buf[n] = 0;
        CHK(strcmp(buf, "P1|pin=1|tx=5") == 0, "deveria ler a 6ª gravação, leu '%s'", buf);
    }

    /* ------------------------------------------------------------- 16 -- */
    printf("\n[16] store_open repetido não vaza descritor / repeated store_open does not leak an fd\n");
    {
        mkpart(0);
        char h[24]; regiao_hash(h);
        abrir(h);
        int antes = 0; for (int fd = 0; fd < 1024; fd++) if (fcntl(fd, F_GETFD) != -1) antes++;
        for (int i = 0; i < 20; i++) { store_config("file:" PATH, h); store_open(); }   /* sem store_close, como o main.c / no store_close, like main.c */
        int depois = 0; for (int fd = 0; fd < 1024; fd++) if (fcntl(fd, F_GETFD) != -1) depois++;
        CHK(depois == antes, "descritores abertos: %d antes, %d depois", antes, depois);
    }

    unlink(PATH);
    unlink("/tmp/bp-store-hash.txt");

    printf("\n=== RESULTADO: %d verificações, %d falhas ===\n", checks, falhas);
    return falhas ? 1 : 0;
}
