/*
 * BANKPHONE OS — ESTADO PERSISTENTE (F2). Ver store.h para o contrato.
 *
 * Região: os ÚLTIMOS bytes úteis da partição pedida (derivados do tamanho real
 * lido do kernel, nunca de um endereço fixo no código).
 *
 *   base  = part_bytes - span - ST_TAIL_MARGIN
 *   span  = sep + ST_SLOT_BYTES            (slot A em base, slot B em base+sep)
 *   sep   = max(erase_size lido do sysfs, ST_SEP_MIN)
 */
#define _GNU_SOURCE
#include "store.h"
#include "sec.h"            /* sha256() — a mesma do PIN */

#include <stdarg.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef __linux__
#include <linux/fs.h>
#endif

StoreState ST;

static int sfd = -1;
static int s_is_file;
static unsigned long long s_sep;
static StoreLogFn s_log;

void store_set_log(StoreLogFn fn) { s_log = fn; }

static void logf_(const char *fmt, ...)
{
    if (!s_log) return;
    char b[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    s_log(b);
}

/* ------------------------------------------------------------------ IO ---- */

#ifdef BANKPHONE_STORE_TEST
static int io_hook(int op, unsigned long long off, size_t n)
{
    return bp_store_test_hook(op, off, n);
}
#else
static int io_hook(int op, unsigned long long off, size_t n) { (void)op; (void)off; (void)n; return 0; }
#endif

static int io_read(void *b, size_t n, unsigned long long off)
{
    if (io_hook(ST_OP_READ, off, n)) return -1;
    ssize_t r = pread(sfd, b, n, (off_t)off);
    if (r == (ssize_t)n) return 0;
    if (r < 0) return -1;
    memset((char *)b + r, 0, n - (size_t)r);      /* curto: resto é tratado como 0 */
    return -1;
}
static int io_write(const void *b, size_t n, unsigned long long off)
{
    if (io_hook(ST_OP_WRITE, off, n)) return -1;
    ssize_t w = pwrite(sfd, b, n, (off_t)off);
    return w == (ssize_t)n ? 0 : -1;
}
static int io_sync(void)
{
    if (io_hook(ST_OP_SYNC, 0, 0)) return -1;
    return fsync(sfd);
}

/* ------------------------------------------------------------- cabeçalho -- */

#define ST_MAGIC   0x31305342u        /* 'BS01' */
#define ST_VER     2
#define ST_OK      0xA5A5
#define ST_PENDING 0x5A5A

typedef struct {
    uint32_t magic;
    uint16_t ver;
    uint16_t state;
    uint32_t seq;
    uint32_t len;
    uint32_t crc;      /* CRC32 do payload            */
    uint32_t hcrc;     /* CRC32 dos 20 bytes acima    */
} SHead;
typedef char st_head_size_check[(sizeof(SHead) == 24) ? 1 : -1];

static uint32_t crc32_(const uint8_t *p, size_t n)
{
    uint32_t c = ~0u;
    while (n--) { c ^= *p++; for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & -(c & 1)); }
    return ~c;
}
static void head_seal(SHead *h) { h->hcrc = crc32_((const uint8_t *)h, 20); }
static int  head_ok(const SHead *h)
{
    if (h->magic != ST_MAGIC || h->ver != ST_VER) return 0;
    SHead t = *h;
    uint32_t keep = t.hcrc;
    head_seal(&t);
    return keep == t.hcrc;
}

/* -------------------------------------------------------------- hex/hash -- */

static void hex16(const uint8_t *d, char out[24])
{
    static const char *H = "0123456789abcdef";
    for (int i = 0; i < 8; i++) { out[i * 2] = H[d[i] >> 4]; out[i * 2 + 1] = H[d[i] & 15]; }
    out[16] = 0;
}

/* ----------------------------------------------------------------- descoberta */

static int node_for_part(const char *part, char *out, size_t outn)
{
    /* 1) descoberta por NOME via sysfs: PARTNAME=<part> em /sys/class/block */
    DIR *d = opendir("/sys/class/block");
    if (d) {
        struct dirent *e;
        while ((e = readdir(d))) {
            if (e->d_name[0] == '.') continue;
            char p[200], l[200];
            snprintf(p, sizeof p, "/sys/class/block/%.64s/uevent", e->d_name);
            FILE *f = fopen(p, "r");
            if (!f) continue;
            char alvo[64];
            snprintf(alvo, sizeof alvo, "PARTNAME=%.24s", part);
            int achou = 0;
            while (fgets(l, sizeof l, f)) {
                char *nl = strchr(l, '\n'); if (nl) *nl = 0;
                if (!strcmp(l, alvo)) { achou = 1; break; }
            }
            fclose(f);
            if (achou) {
                snprintf(out, outn, "/dev/%.40s", e->d_name);
                closedir(d);
                return 0;
            }
        }
        closedir(d);
    }
    /* 2) o caminho estável do Android */
    struct stat st;
    char by[160];
    snprintf(by, sizeof by, "/dev/block/by-name/%.80s", part);
    if (stat(by, &st) == 0) { snprintf(out, outn, "%.*s", (int)outn - 1, by); return 0; }
    return -1;
}

static long read_long(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    long v = -1;
    if (fscanf(f, "%ld", &v) != 1) v = -1;
    fclose(f);
    return v;
}

/* ---------------------------------------------------------------- config -- */

static char cfg_part[24];
static char cfg_hash[24];
static char cfg_file[160];

void store_config(const char *state_spec, const char *expected_hash_hex)
{
    memset(cfg_part, 0, sizeof cfg_part);
    memset(cfg_hash, 0, sizeof cfg_hash);
    memset(cfg_file, 0, sizeof cfg_file);
    ST.pedido = 0;
    if (!state_spec || !state_spec[0]) return;
    ST.pedido = 1;
    if (!strncmp(state_spec, "file:", 5)) {
        /* backend de ARQUIVO: só para o teste de host (ver store.h) */
        snprintf(cfg_file, sizeof cfg_file, "%.*s", (int)sizeof cfg_file - 1, state_spec + 5);
        snprintf(cfg_part, sizeof cfg_part, "arquivo");
    } else {
        snprintf(cfg_part, sizeof cfg_part, "%.*s", (int)sizeof cfg_part - 1, state_spec);
    }
    if (expected_hash_hex && strlen(expected_hash_hex) >= 16)
        snprintf(cfg_hash, sizeof cfg_hash, "%.16s", expected_hash_hex);
}

/* ------------------------------------------------------------------ open -- */

static unsigned long long file_size_of(int fd)
{
#ifdef __linux__
    unsigned long long b = 0;
    if (ioctl(fd, BLKGETSIZE64, &b) == 0) return b;      /* nó de bloco */
#endif
    off_t e = lseek(fd, 0, SEEK_END);                    /* arquivo comum */
    return e > 0 ? (unsigned long long)e : 0;
}

int store_open(void)
{
    int pedido = ST.pedido;                  /* vem de store_config() */
    memset(&ST, 0, sizeof ST);
    ST.pedido = pedido;
    snprintf(ST.part, sizeof ST.part, "%.*s", (int)sizeof ST.part - 1, cfg_part);
    snprintf(ST.hash_esperado, sizeof ST.hash_esperado, "%.*s", (int)sizeof ST.hash_esperado - 1, cfg_hash);
    for (int i = 0; i < 2; i++) ST.slot_ok[i] = 0;

    if (!ST.pedido) {
        snprintf(ST.why, sizeof ST.why,
                 "persistência não pedida (falta 'bankphone.state=<partição>' na cmdline) — "
                 "o estado vive só na memória");
        return -1;
    }
    if (cfg_file[0]) {
        s_is_file = 1;
        snprintf(ST.node, sizeof ST.node, "%.*s", (int)sizeof ST.node - 1, cfg_file);
        sfd = open(cfg_file, O_RDWR);
    } else {
        s_is_file = 0;
        char node[160] = "";
        if (node_for_part(cfg_part, node, sizeof node) != 0) {
            snprintf(ST.why, sizeof ST.why,
                     "partição '%.20s' não existe neste aparelho (nenhum PARTNAME=%.20s em /sys/class/block)",
                     cfg_part, cfg_part);
            logf_("%s", ST.why);
            return -1;
        }
        snprintf(ST.node, sizeof ST.node, "%.*s", (int)sizeof ST.node - 1, node);
        sfd = open(node, O_RDWR | O_SYNC);
    }
    if (sfd < 0) {
        snprintf(ST.why, sizeof ST.why, "não consegui abrir '%.40s' (errno=%d)", ST.node, errno);
        logf_("%s", ST.why);
        return -1;
    }

    ST.part_bytes = file_size_of(sfd);
    if (ST.part_bytes == 0) {
        snprintf(ST.why, sizeof ST.why, "tamanho de '%.40s' desconhecido", ST.node);
        goto falha;
    }

    /* Geometria: separação FIXA entre os slots.
     * Motivo medido: eMMC não é flash cru — existe FTL, e uma pwrite interrompida
     * pode perder a PÁGINA lógica em escrita, não um grupo de apagamento inteiro.
     * Quem protege o estado é o protocolo (PENDING + CRC + leitura de volta), não
     * a geometria. A separação de 512 KiB é folga entre os dois slots e mantém a
     * conta igual à do scripts/state-region-hash.sh, que roda fora do aparelho.
     * O erase_size é LIDO e registrado só como informação. */
    s_sep = ST_SEP_MIN;
    {
        char p[200], par[80];
        const char *base = strrchr(ST.node, '/');
        snprintf(par, sizeof par, "%.*s", (int)sizeof par - 1, base ? base + 1 : ST.node);
        {   /* mmcblk0p12 -> mmcblk0 */
            char *pp = strrchr(par, 'p');
            if (pp && pp[1] >= '0' && pp[1] <= '9') *pp = 0;
        }
        long er = -1;
        if (!s_is_file) {
            snprintf(p, sizeof p, "/sys/class/block/%.40s/device/preferred_erase_size", par);
            er = read_long(p);
            if (er <= 0) { snprintf(p, sizeof p, "/sys/class/block/%.40s/queue/physical_block_size", par); er = read_long(p); }
        }
        ST.erase = er > 0 ? er : 0;
        logf_("store: erase/preferred do aparelho = %ld bytes (informativo; a separação é fixa em %llu KiB)",
              ST.erase, (unsigned long long)ST_SEP_MIN / 1024);
    }

    ST.span = s_sep + ST_SLOT_BYTES;
    if (ST.part_bytes < ST.span + ST_TAIL_MARGIN + (1u << 20)) {
        snprintf(ST.why, sizeof ST.why,
                 "partição '%.20s' tem %llu KiB — pequena demais para uma região de %llu KiB "
                 "com %llu KiB de margem",
                 ST.part, ST.part_bytes / 1024, ST.span / 1024, (unsigned long long)ST_TAIL_MARGIN / 1024);
        goto falha;
    }
    ST.base = ST.part_bytes - ST.span - ST_TAIL_MARGIN;

    /* ---- LEITURA COMPLETA DA REGIÃO + HASH (o que prova que houve backup) ---- */
    {
        unsigned char *reg = malloc((size_t)ST.span);
        if (!reg) { snprintf(ST.why, sizeof ST.why, "sem memória para ler %llu KiB da região", ST.span / 1024); goto falha; }
        if (io_read(reg, (size_t)ST.span, ST.base) != 0) {
            snprintf(ST.why, sizeof ST.why, "não consegui ler os %llu KiB da região em %.40s",
                     ST.span / 1024, ST.node);
            free(reg);
            goto falha;
        }
        unsigned char h[32];
        sha256(reg, (size_t)ST.span, h);
        hex16(h, ST.hash_atual);

        long nao_zerado = 0, nao_ff = 0;
        for (unsigned long long i = 0; i < ST.span; i++) { if (reg[i] != 0x00) nao_zerado = 1; if (reg[i] != 0xFF) nao_ff = 1; }
        ST.virgin = (!nao_zerado || !nao_ff);
        free(reg);

        logf_("store: '%.20s' = %llu KiB · erase=%ld · região = [%llu, %llu) KiB (span %llu KiB, %s)",
              ST.part, ST.part_bytes / 1024, ST.erase,
              ST.base / 1024, (ST.base + ST.span) / 1024, ST.span / 1024,
              ST.virgin ? "virgem" : "com conteúdo");
        logf_("store: hash da região agora = %s", ST.hash_atual);
    }

    /* ---- a área já é nossa? (assinatura nos slots) ---- */
    for (int s = 0; s < 2; s++) {
        SHead h;
        unsigned long long off = ST.base + (unsigned long long)s * s_sep;
        ST.slot_ok[s] = 0; ST.seq[s] = 0; ST.len[s] = 0;
        if (io_read(&h, sizeof h, off) != 0) continue;
        if (h.magic != ST_MAGIC) continue;                  /* vazio ou de outro dono */
        ST.claimed = 1;
        if (!head_ok(&h)) { ST.slot_ok[s] = 0; continue; }  /* cabeçalho rasgado */
        ST.seq[s] = h.seq; ST.len[s] = h.len;
        ST.slot_ok[s] = (h.state == ST_PENDING) ? 2 : 1;
    }

    if (ST.claimed) {
        ST.armed = 1;
        ST.ok = 1;
        snprintf(ST.why, sizeof ST.why,
                 "área já assumida por este sistema (assinatura BS01 nos slots) — "
                 "o hash do backup só é exigido na primeira tomada");
        logf_("store: ARMADO — %s", ST.why);
        logf_("store: slot A: %s seq=%u · slot B: %s seq=%u",
              ST.slot_ok[0] == 1 ? "ok" : ST.slot_ok[0] == 2 ? "PENDENTE" : "vazio/inválido", ST.seq[0],
              ST.slot_ok[1] == 1 ? "ok" : ST.slot_ok[1] == 2 ? "PENDENTE" : "vazio/inválido", ST.seq[1]);
        return 0;
    }

    /* ---- TOMADA DA ÁREA: sem backup conferido, NÃO ESCREVE ---- */
    if (cfg_hash[0] == 0) {
        ST.refusals++;
        snprintf(ST.why, sizeof ST.why,
                 "RECUSADO: falta 'bankphone.statehash=<16 hex>' (hash da região no backup). "
                 "Sem backup conferido eu não escrevo. Rode scripts/state-region-hash.sh "
                 "com o dump e use o hash que ele imprimir.");
        logf_("%s", ST.why);
        goto falha;
    }
    if (strcmp(cfg_hash, ST.hash_atual)) {
        ST.refusals++;
        snprintf(ST.why, sizeof ST.why,
                 "RECUSADO: a região MUDOU desde o backup (esperado %s, medido %s). "
                 "Refaça o dump da partição e o armamento com o hash novo.",
                 cfg_hash, ST.hash_atual);
        logf_("%s", ST.why);
        goto falha;
    }

    ST.armed = 1;
    ST.ok = 1;
    ST.claim_now = 1;
    snprintf(ST.why, sizeof ST.why,
             "área assumida AGORA, conferida contra o backup (%s) — os slots passam a ser deste sistema",
             ST.hash_atual);
    logf_("store: ARMADO — %s", ST.why);
    logf_("store: slot A: vazio/inválido · slot B: vazio/inválido (primeira tomada)");
    return 0;

falha:
    if (sfd >= 0) { close(sfd); sfd = -1; }
    ST.ok = 0;
    return -1;
}

/* ------------------------------------------------------------------ load -- */

static int slot_read(int s, unsigned char *buf, size_t cap, size_t *n, SHead *hout)
{
    unsigned long long off = ST.base + (unsigned long long)s * s_sep;
    SHead h;
    if (io_read(&h, sizeof h, off) != 0 || !head_ok(&h)) return -1;
    if (h.state != ST_OK) return -2;                       /* PENDENTE: não serve */
    if (h.len == 0 || h.len > ST_SLOT_BYTES - sizeof(SHead) || h.len > cap) return -1;
    if (io_read(buf, h.len, off + sizeof h) != 0) return -1;
    if (crc32_(buf, h.len) != h.crc) return -1;
    if (hout) *hout = h;
    *n = h.len;
    return 0;
}

int store_load(void *buf, size_t cap, size_t *n)
{
    if (!ST.ok || sfd < 0) return -1;
    unsigned char *tmp = malloc(cap);
    if (!tmp) return -2;

    SHead ha, hb;
    size_t na = 0, nb = 0;
    int a = slot_read(0, buf, cap, &na, &ha);
    int b = slot_read(1, tmp, cap, &nb, &hb);

    /* O slot EXISTE (tem a assinatura) mesmo que esteja inválido/PENDENTE. A
     * diferença importa: um slot VAZIO é só o primeiro boot da área (nada de
     * anormal), enquanto um slot existente e inutilizável é RECUPERAÇÃO — e a
     * tela precisa dizer a verdade em cada caso. */
    int existe[2] = { 0, 0 };
    for (int s = 0; s < 2; s++) {
        SHead h;
        unsigned long long off = ST.base + (unsigned long long)s * s_sep;
        if (io_read(&h, sizeof h, off) != 0) continue;
        if (h.magic != ST_MAGIC) continue;
        existe[s] = 1;
        if (head_ok(&h)) {
            ST.seq[s] = h.seq; ST.len[s] = h.len;
            ST.slot_ok[s] = (h.state == ST_PENDING) ? 2 : 1;
        }
    }

    int r = 1, usado = -1;
    ST.recovered = 0;
    if (a == 0 && (b != 0 || ha.seq >= hb.seq)) { *n = na; r = 0; usado = 0; }
    else if (b == 0) { memcpy(buf, tmp, nb); *n = nb; r = 0; usado = 1; }
    if (r == 0 && usado >= 0) {
        int outro = 1 - usado;
        int rc_outro = outro == 0 ? a : b;
        if (existe[outro] && rc_outro != 0) ST.recovered = 1;   /* o outro estava ruim/PENDENTE */
    }

    /* PENDENTE é sinal de escrita interrompida: registra, porque isso é um evento */
    if (ST.slot_ok[0] == 2 || ST.slot_ok[1] == 2)
        logf_("store: ATENÇÃO — slot %s ficou PENDENTE (escrita interrompida). "
              "O estado válido veio do outro slot; a cura reescreve o pendente.",
              ST.slot_ok[0] == 2 ? "A" : "B");
    if (r == 1 && (a == -1 || b == -1) && (ST.slot_ok[0] || ST.slot_ok[1]))
        logf_("store: slot com cabeçalho válido mas conteúdo inválido (CRC) — usando o outro");
    free(tmp);
    return r;
}

/* ------------------------------------------------------------------ save -- */

/* Sequência de compromisso de UM slot. A ordem é o que garante a propriedade:
 *   1. cabeçalho marca PENDENTE   (se a energia cair aqui, o slot é ignorado)
 *   2. conteúdo
 *   3. cabeçalho marca OK
 *   4. LEITURA DE VOLTA: sem isto, "salvou" seria palpite
 * Uma queda em qualquer ponto deixa o OUTRO slot intacto — e é de lá que o
 * próximo boot lê o estado bom. */
static int slot_commit(int s, uint32_t seq, const void *data, size_t n)
{
    unsigned long long off = ST.base + (unsigned long long)s * s_sep;
    SHead h;
    memset(&h, 0, sizeof h);
    h.magic = ST_MAGIC; h.ver = ST_VER; h.state = ST_OK;
    h.seq = seq; h.len = (uint32_t)n; h.crc = crc32_((const uint8_t *)data, n);

    /* 1. marcador PENDENTE */
    h.state = ST_PENDING; head_seal(&h);
    if (io_write(&h, sizeof h, off) != 0) return -1;
    if (io_sync() != 0) return -2;

    /* 2. conteúdo */
    if (io_write(data, n, off + sizeof h) != 0) return -3;
    if (io_sync() != 0) return -4;

    /* 3. cabeçalho OK (com o CRC do conteúdo deste slot) */
    h.state = ST_OK; head_seal(&h);
    if (io_write(&h, sizeof h, off) != 0) return -5;
    if (io_sync() != 0) return -6;

    /* 4. leitura de volta */
    SHead v;
    unsigned char *chk = malloc(n);
    if (!chk) return -7;
    int bad = 0;
    if (io_read(&v, sizeof v, off) != 0 || !head_ok(&v) || v.state != ST_OK || v.seq != seq || v.len != n) bad = 1;
    else if (io_read(chk, n, off + sizeof v) != 0) bad = 2;
    else if (crc32_(chk, n) != v.crc) bad = 3;
    free(chk);
    if (bad) return -7 - bad;
    return 0;
}

int store_save(const void *data, size_t n)
{
    ST.last_rc = 0;
    ST.last_msg[0] = 0;
    if (!ST.ok || sfd < 0) {
        ST.refusals++;
        ST.last_rc = -1;
        snprintf(ST.last_msg, sizeof ST.last_msg, "store não armado — não escrevi nada");
        return -1;
    }
    if (n > ST_SLOT_BYTES - sizeof(SHead)) {
        ST.last_rc = -2;
        snprintf(ST.last_msg, sizeof ST.last_msg, "estado grande demais (%u > %u bytes)",
                 (unsigned)n, ST_SLOT_BYTES - (unsigned)sizeof(SHead));
        return -2;
    }

    /* escolhe o slot MAIS VELHO (ou o vazio): o mais novo nunca é tocado */
    int novo = 0, velho = 1;
    uint32_t seq_max = 0;
    for (int s = 0; s < 2; s++) if (ST.slot_ok[s] == 1 && ST.seq[s] > seq_max) seq_max = ST.seq[s];
    if (ST.slot_ok[0] == 1 && ST.slot_ok[1] == 1) { novo = (ST.seq[0] <= ST.seq[1]) ? 0 : 1; velho = 1 - novo; }
    else if (ST.slot_ok[0] == 1) { novo = 1; velho = 0; }
    else { novo = 0; velho = 1; }

    uint32_t seq = seq_max + 1;
    int rc = slot_commit(novo, seq, data, n);
    if (rc != 0) goto falhou;

    ST.slot_ok[novo] = 1; ST.seq[novo] = seq; ST.len[novo] = (unsigned)n;
    ST.saves++;
    ST.last_rc = 0;
    snprintf(ST.last_msg, sizeof ST.last_msg,
             "slot %c seq=%u · %u bytes · lida de volta e CRC ok (o slot %c seguiu intacto)",
             'A' + novo, seq, (unsigned)n, 'A' + velho);
    logf_("store: gravado — %s", ST.last_msg);
    return 0;

falhou:
    ST.failures++;
    ST.last_rc = rc;
    snprintf(ST.last_msg, sizeof ST.last_msg,
             "falha na gravação do slot %c (código %d) — o slot %c continua sendo o estado bom",
             'A' + novo, rc, 'A' + velho);
    logf_("store: %s", ST.last_msg);
    return rc;
}

/* ------------------------------------------------------------------ heal -- */

int store_heal(void)
{
    if (!ST.ok || sfd < 0) return -1;
    /* Pequeno estado de reserva: quem chama já carregou o estado bom em memória
     * e vai salvar de novo; aqui só limpamos o slot estragado para ele não
     * voltar a contar como "pendente" no próximo boot. */
    int tocou = 0;
    for (int s = 0; s < 2; s++) {
        if (ST.slot_ok[s] == 0 || ST.slot_ok[s] == 2) {
            /* zera o cabeçalho do slot ruim: deixa de ser "pendente" e vira vazio */
            SHead h; memset(&h, 0, sizeof h);
            unsigned long long off = ST.base + (unsigned long long)s * s_sep;
            if (io_write(&h, sizeof h, off) == 0 && io_sync() == 0) {
                logf_("store: cura — slot %c marcado como vazio (estava %s)",
                      'A' + s, ST.slot_ok[s] == 2 ? "PENDENTE" : "inválido");
                ST.slot_ok[s] = 0; ST.seq[s] = 0; ST.len[s] = 0;
                tocou = 1;
            }
        }
    }
    return tocou ? 0 : 1;
}

void store_close(void)
{
    if (sfd >= 0) close(sfd);
    sfd = -1;
}
