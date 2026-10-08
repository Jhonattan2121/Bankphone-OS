/*
 * BANKPHONE OS — ESTADO PERSISTENTE (F2).
 *
 * Regras que este arquivo implementa, e que são MECÂNICAS (não são conselho):
 *
 *  R1. Nada é presumido: a partição e a região são DESCOBERTAS e o que foi
 *      encontrado vai para o log (nome, nó, tamanho, erase_size, base, span).
 *  R2. NADA de caminho fixo no código: a partição vem da cmdline
 *      (`bankphone.state=<part>`); sem isso, o sistema roda só em memória e a
 *      tela diz "SÓ MEMÓRIA".
 *  R3. SEM BACKUP VERIFICADO, NÃO ESCREVE — e o momento em que isso importa é
 *      o da TOMADA DA ÁREA (a primeira escrita, quando a região ainda tem o
 *      conteúdo de fábrica). Nessa hora o hash do backup é obrigatório
 *      (`bankphone.statehash=<16 hex>`) e a região atual tem de bater com ele;
 *      senão o store RECUSA e registra os dois hashes. Depois que a área é
 *      nossa (slots com a assinatura 'BS01'), o conteúdo da região MUDA a cada
 *      gravação — então as aberturas seguintes reconhecem a assinatura e não
 *      exigem mais o hash (que só descreveria o conteúdo original, destruído de
 *      forma deliberada e reversível pelo backup).
 *  R4. Escrita nunca destrói o último estado bom: dois slots alternados, com
 *      marcador PENDING, e o último estado válido é sempre LIDO DE VOLTA e
 *      conferido (CRC) antes de a função retornar ok.
 *  R5. Estado é só o necessário (PIN + livro-razão). Nada de segredo: o PIN é
 *      derivado com sal; não existe chave privada neste sistema ainda.
 *
 * Formato no disco (little-endian, igual ao aparelho e ao host de teste):
 *   região = [ A: 256 KiB ][ separação = max(erase, 512 KiB) ][ B: 256 KiB ]
 *   cada slot = cabeçalho de 24 bytes + payload (CRC32 no payload, CRC32 no cabeçalho)
 *
 * A separação existe para que A e B caiam em BLOCOS DE APAGAMENTO
 * diferentes: uma queda de energia durante o apagamento de um bloco não pode
 * levar os dois slots junto.
 *
 * Backend de ARQUIVO ("file:/caminho") existe para o teste de host
 * (src/init/tests/test_store.c) exercitar queda de energia em CADA passo da
 * gravação. No aparelho, o backend é sempre o nó de bloco.
 */
#ifndef BANKPHONE_STORE_H
#define BANKPHONE_STORE_H

#include <stddef.h>
#include <stdint.h>

/* ---- geometria (publicada para docs/script de backup/armamento) ---- */
#define ST_SLOT_BYTES   (256u << 10)          /* 256 KiB por slot            */
#define ST_SEP_MIN      (512u << 10)          /* separação mínima A<->B       */
#define ST_TAIL_MARGIN  (64u << 10)           /* margem no fim da partição    */

typedef struct {
    /* o que foi pedido/encontrado */
    int  pedido;                              /* 1 = cmdline pediu persistência */
    int  armed;                               /* 1 = região liberada p/ escrita */
    int  ok;                                  /* 1 = pronto para ler/gravar     */
    char why[192];                            /* motivo, sempre preenchido      */
    char part[24];                            /* partição (da cmdline)          */
    char node[64];                            /* nó de bloco/arquivo usado      */
    unsigned long long part_bytes;            /* tamanho da partição            */
    unsigned long long base;                  /* início da região               */
    unsigned long long span;                  /* bytes da região                */
    long erase;                               /* erase_size lido (0 = não lido) */
    int  virgin;                              /* 1 = região toda 0x00/0xFF      */
    int  claimed;                             /* 1 = a área JÁ era nossa (BS01) */
    int  claim_now;                           /* 1 = assumida agora, c/ backup  */
    char hash_esperado[24];                   /* do backup (cmdline)            */
    char hash_atual[24];                      /* medido agora                   */

    /* slots */
    int  slot_ok[2];                          /* 1 ok · 0 inválido · 2 PENDENTE */
    uint32_t seq[2];
    unsigned len[2];
    int  recovered;                           /* 1 = veio do outro slot / cura  */

    /* gravação */
    unsigned saves, refusals, failures;
    int  last_rc;
    char last_msg[128];
} StoreState;

extern StoreState ST;

/* opções vêm da cmdline, via main.c (não lê /proc/cmdline aqui: testável) */
void store_config(const char *state_spec, const char *expected_hash_hex);

int  store_open(void);                        /* descobre e VALIDA (não grava)  */
int  store_load(void *buf, size_t cap, size_t *n);   /* 0 ok · 1 vazio · <0 erro */
int  store_save(const void *data, size_t n);  /* 0 ok · <0 erro (motivo em ST) */
int  store_heal(void);                        /* cura slot inválido/PENDENTE   */
void store_close(void);

/* log: quem usa (main.c) aponta para o bd_log; o teste de host imprime */
typedef void (*StoreLogFn)(const char *msg);
void store_set_log(StoreLogFn fn);

/* operações que o gancho de teste observa (definidas sempre: são só constantes) */
enum { ST_OP_WRITE = 1, ST_OP_SYNC = 2, ST_OP_READ = 3 };

#ifdef BANKPHONE_STORE_TEST
/* ---- gancho de teste (só existe quando compilado para o teste) ----
 * Devolver != 0 SIMULA queda de energia naquela operação. */
int bp_store_test_hook(int op, unsigned long long off, size_t n);
#endif

#endif /* BANKPHONE_STORE_H */
