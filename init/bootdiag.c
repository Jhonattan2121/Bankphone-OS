/*
 * BANKPHONE OS — bootdiag.c  (implementação)
 * Ver src/init/README-bootdiag.md para a ordem de operações recomendada.
 */
#define _GNU_SOURCE
#include "bootdiag.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/reboot.h>
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/klog.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

/* klogctl existe em glibc/musl recentes; syscall direto é o caminho garantido */
#ifndef SYSLOG_ACTION_READ_ALL
#define SYSLOG_ACTION_READ_ALL   3
#endif
#ifndef SYSLOG_ACTION_SIZE_BUFFER
#define SYSLOG_ACTION_SIZE_BUFFER 10
#endif
#ifndef BLKGETSIZE64
#define BLKGETSIZE64 _IOR(0x12, 114, size_t)
#endif

/* ===================================================================== */
/* log em RAM (com retenção do começo E do fim)                          */
/* ===================================================================== */

#define BD_TEXT_MAX (96u * 1024u)
#define BD_KEEP_HEAD (16u * 1024u)
#define BD_TRIM_STEP (32u * 1024u)

static char   g_text[BD_TEXT_MAX];
static size_t g_len;
static int    g_kmsg = -1;
static int    g_ser  = -1;
static int    g_trimmed;

static long long bd_uptime_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (long long)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

/* append com política: quando enche, joga fora o MIOLO (mantém cabeça e cauda) */
static void bd_append(const char *s, size_t n)
{
    if (n >= BD_TEXT_MAX - 1) { s += n - (BD_TEXT_MAX - 2); n = BD_TEXT_MAX - 2; }
    if (g_len + n + 1 >= BD_TEXT_MAX) {
        size_t drop = BD_TRIM_STEP;
        if (drop > g_len - BD_KEEP_HEAD) drop = g_len - BD_KEEP_HEAD;
        memmove(g_text + BD_KEEP_HEAD, g_text + BD_KEEP_HEAD + drop, g_len - BD_KEEP_HEAD - drop);
        g_len -= drop;
        if (!g_trimmed) {
            g_trimmed = 1;
            if (g_len + 40 < BD_TEXT_MAX) {
                memcpy(g_text + g_len, "...[miolo do log descartado]...\n", 33);
                g_len += 33;
            }
        }
    }
    memcpy(g_text + g_len, s, n);
    g_len += n;
    g_text[g_len] = '\0';
}

/* fila de saída serial (drenada quando o host abre a porta) */
#define BD_SERQ_MAX (64u * 1024u)
static char   g_serq[BD_SERQ_MAX];
static size_t g_serq_len;

static void bd_serq_push(const char *s, size_t n)
{
    if (n >= BD_SERQ_MAX) { s += n - (BD_SERQ_MAX - 1); n = BD_SERQ_MAX - 1; }
    if (g_serq_len + n >= BD_SERQ_MAX) {           /* mantém a cauda recente */
        size_t drop = g_serq_len + n - (BD_SERQ_MAX - 1);
        memmove(g_serq, g_serq + drop, g_serq_len - drop);
        g_serq_len -= drop;
    }
    memcpy(g_serq + g_serq_len, s, n);
    g_serq_len += n;
}

void bd_log_raw(const char *s)                 /* sinal-safe: só write() */
{
    if (!s) return;
    if (g_ser >= 0) { ssize_t r = write(g_ser, s, strlen(s)); (void)r; }
    if (g_kmsg >= 0) {
        char k[256];
        int m = snprintf(k, sizeof k, "<3>bankphone: %s", s);
        if (m > 0) { ssize_t r = write(g_kmsg, k, (size_t)m); (void)r; }
    }
    /* fallback para stderr: só quando não há kmsg nem serial (teste no host,
     * emulação). No aparelho /dev/kmsg existe, então isto não escreve nada. */
    if (g_kmsg < 0 && g_ser < 0) { ssize_t r = write(STDERR_FILENO, s, strlen(s)); (void)r; }
}

void bd_log(const char *fmt, ...)
{
    char line[512];
    int n = snprintf(line, sizeof line - 2, "[%6lld.%03lld] ", bd_uptime_ms() / 1000, bd_uptime_ms() % 1000);
    va_list ap;
    va_start(ap, fmt);
    int m = vsnprintf(line + n, sizeof line - n - 2, fmt, ap);
    va_end(ap);
    if (m < 0) return;
    n += m;
    if (n > (int)sizeof line - 3) n = (int)sizeof line - 3;
    line[n++] = '\n';
    line[n] = 0;

    bd_append(line, (size_t)n);
    bd_serq_push(line, (size_t)n);
    bd_log_raw(line);
}

const char  *bd_report_text(void) { return g_text; }
size_t       bd_report_len(void)  { return g_len; }
int          bd_serial_fd(void)   { return g_ser; }

/* ===================================================================== */
/* captura do ring buffer do KERNEL (dmesg)                              */
/* ===================================================================== */

static char  *g_klog;
static size_t g_klog_len;
static char   g_cmdline[2048];

static long bd_syslog(int type, char *buf, int len)
{
    return syscall(SYS_syslog, type, buf, len);
}

size_t bd_capture_kernel_log(void)
{
    long size = bd_syslog(SYSLOG_ACTION_SIZE_BUFFER, NULL, 0);
    if (size <= 0) { bd_log("dmesg: SIZE_BUFFER falhou (%ld, errno=%d)", size, errno); return 0; }
    if (size > (long)(4u << 20)) size = 4u << 20;

    char *buf = malloc((size_t)size + 1);
    if (!buf) { bd_log("dmesg: sem memória para %ld bytes", size); return 0; }

    long got = bd_syslog(SYSLOG_ACTION_READ_ALL, buf, (int)size);
    if (got <= 0) {
        bd_log("dmesg: READ_ALL falhou (%ld, errno=%d) — dmesg_restrict?", got, errno);
        free(buf);
        return 0;
    }
    buf[got] = 0;
    free(g_klog);
    g_klog = buf;
    g_klog_len = (size_t)got;

    bd_log("dmesg: %ld bytes capturados do ring buffer do kernel", got);
    return (size_t)got;
}

/* Lê o ring buffer do kernel e devolve (via bd_log) SÓ as linhas que casam com
 * um dos assuntos, sem repetir o que já foi mostrado. É assim que se responde
 * "o driver de toque baixou o firmware?" lendo o próprio printk dele, depois do
 * boot e sem PC nenhum. Devolve o número de linhas novas emitidas. */
int bd_kmsg_grep(const char *tag, const char *const *keys, int nkeys, int max)
{
    long size = bd_syslog(SYSLOG_ACTION_SIZE_BUFFER, NULL, 0);
    if (size <= 0) return -1;
    if (size > (4L << 20)) size = 4L << 20;

    char *buf = malloc((size_t)size + 1);
    if (!buf) return -1;
    long got = bd_syslog(SYSLOG_ACTION_READ_ALL, buf, (int)size);
    if (got <= 0) { free(buf); return -1; }
    buf[got] = 0;

    static int ja_vistos;                  /* casamentos já reportados */
    int match = 0, printed = 0;
    char *p = buf;
    while (p && *p) {
        char *nl = strchr(p, '\n');
        if (nl) *nl = 0;
        int hit = 0;
        for (int k = 0; k < nkeys; k++)
            if (keys[k] && strstr(p, keys[k])) { hit = 1; break; }
        if (hit) {
            match++;
            if (match > ja_vistos && printed < max) {
                bd_log("%s| %.200s", tag, p);
                printed++;
            }
        }
        if (!nl) break;
        p = nl + 1;
    }
    ja_vistos = match;
    free(buf);
    return printed;
}

size_t bd_klog_snapshot(char *out, size_t cap)
{
    if (!g_klog || cap == 0) { if (cap) out[0] = 0; return 0; }
    size_t n = g_klog_len < cap - 1 ? g_klog_len : cap - 1;
    memcpy(out, g_klog, n);
    out[n] = 0;
    return n;
}

const char *bd_cmdline(void) { return g_cmdline; }

/* ===================================================================== */
/* relatório persistente (sobrevive a reset)                             */
/* ===================================================================== */

#define BD_BASE   (8ull << 20)          /* 8 MiB adentro           */
#define BD_SLOT   (64u << 10)           /* 64 KiB por slot         */
#define BD_MAGIC  0x474F4C42u           /* "BLOG"                  */

typedef struct { uint32_t magic, seq, len, crc; } bd_hdr;

static int      g_rep_fd = -1;
static uint32_t g_rep_seq;
static char     g_rep_loc[192];
static char     g_rep_part[32];

/* partições que NUNCA podem ser usadas como área de log */
static const char *const g_deny[] = {
    "preloader", "preloader_a", "preloader_b", "lk", "lk_a", "lk_b", "tee", "tee_a", "tee_b",
    "nvram", "nvdata", "persist", "misc", "super", "system", "vendor", "product", "userdata",
    "metadata", "boot_a", "boot_b", "vendor_boot_a", "dtbo", "dtbo_a", "vbmeta", "vbmeta_a",
    "seccfg", "proinfo", "frp", "protect1", "protect2", "md1img", "spmfw", "scp1", "scp2",
    "sspm_1", "sspm_2", "boot_para", "flashinfo", NULL
};

static int bd_denied(const char *name)
{
    for (int i = 0; g_deny[i]; i++) {
        size_t n = strlen(g_deny[i]);
        /* igualdade exata, ou prefixo + sufixo de slot (_a/_b) */
        if (!strncmp(name, g_deny[i], n) && (name[n] == 0 || !strcmp(name + n, "_a") || !strcmp(name + n, "_b")))
            return 1;
    }
    return 0;
}

static uint32_t bd_crc32(const uint8_t *p, size_t n)
{
    uint32_t c = ~0u;
    while (n--) { c ^= *p++; for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & -(c & 1)); }
    return ~c;
}

/* abre bloco por NOME: /dev/block/by-name/X, /dev/X, ou via sysfs + mknod */
static int bd_block_open(const char *name, int flags)
{
    char path[128];
    snprintf(path, sizeof path, "/dev/block/by-name/%s", name);
    int fd = open(path, flags);
    if (fd >= 0) return fd;

    snprintf(path, sizeof path, "/dev/%s", name);
    fd = open(path, flags);
    if (fd >= 0) return fd;

    DIR *d = opendir("/sys/class/block");
    if (!d) return -1;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char up[320], line[160];
        snprintf(up, sizeof up, "/sys/class/block/%s/uevent", e->d_name);
        FILE *f = fopen(up, "r");
        if (!f) continue;
        int hit = 0;
        while (fgets(line, sizeof line, f)) {
            size_t n = strlen(line);
            while (n && (line[n-1] == '\n' || line[n-1] == '\r')) line[--n] = 0;
            if (!strncmp(line, "PARTNAME=", 9) && !strcmp(line + 9, name)) hit = 1;
        }
        fclose(f);
        if (!hit) continue;
        char dev[320], majmin[64];
        snprintf(dev, sizeof dev, "/sys/class/block/%s/dev", e->d_name);
        FILE *g = fopen(dev, "r");
        if (!g) { closedir(d); return -1; }
        if (!fgets(majmin, sizeof majmin, g)) { fclose(g); closedir(d); return -1; }
        fclose(g);
        unsigned ma = 0, mi = 0;
        if (sscanf(majmin, "%u:%u", &ma, &mi) != 2) { closedir(d); return -1; }
        char node[320];
        snprintf(node, sizeof node, "/dev/%s", e->d_name);
        unlink(node);
        if (mknod(node, S_IFBLK | 0600, makedev(ma, mi)) && errno != EEXIST) { closedir(d); return -1; }
        fd = open(node, flags);
        closedir(d);
        return fd;
    }
    closedir(d);
    return -1;
}

static int bd_report_slot_read(int fd, int slot, uint32_t *seq, char *out, size_t cap, size_t *out_len)
{
    bd_hdr h;
    off_t off = (off_t)(BD_BASE + (unsigned long long)slot * BD_SLOT);
    if (pread(fd, &h, sizeof h, off) != (ssize_t)sizeof h) return -1;
    if (h.magic != BD_MAGIC || h.len == 0 || h.len > BD_SLOT - sizeof h || h.len > cap) return -1;
    if (pread(fd, out, h.len, off + (off_t)sizeof h) != (ssize_t)h.len) return -1;
    if (bd_crc32((const uint8_t *)out, h.len) != h.crc) return -1;
    out[h.len] = 0;
    *seq = h.seq;
    if (out_len) *out_len = h.len;
    return 0;
}

int bd_report_open(const char *partition)
{
    const char *part = partition;
    if (!part) {
        /* candidato natural em MTK: expdb (área de exceção/depuração da MediaTek,
         * não é lida no boot normal). Só é usado se existir. */
        part = "expdb";
    }
    if (bd_denied(part)) {
        bd_log("report: RECUSADO — '%s' é partição crítica (lista de preservação)", part);
        return -1;
    }

    int fd = bd_block_open(part, O_RDWR | O_SYNC);
    if (fd < 0) {
        snprintf(g_rep_loc, sizeof g_rep_loc,
                 "NOT AVAILABLE: partição '%s' não encontrada (errno=%d). "
                 "Informe outra com bd_report_open(\"nome\").", part, errno);
        bd_log("report: %s", g_rep_loc);
        return -1;
    }

    unsigned long long size = 0;
    if (ioctl(fd, BLKGETSIZE64, &size) == 0) {
        bd_log("report: '%s' = %.2f MiB", part, (double)size / 1048576.0);
        if (size < BD_BASE + 2ull * BD_SLOT) {
            bd_log("report: RECUSADO — partição menor que %llu bytes; escolha outra",
                   (unsigned long long)(BD_BASE + 2ull * BD_SLOT));
            close(fd);
            return -1;
        }
    } else {
        bd_log("report: BLKGETSIZE64 falhou (errno=%d) — seguindo sem checar tamanho", errno);
    }

    char *tmp = malloc(BD_SLOT);
    if (!tmp) { close(fd); return -1; }

    /* herda a cauda do relatório anterior (acumula entre boots) */
    uint32_t sa = 0, sb = 0; size_t la = 0, lb = 0;
    int a = bd_report_slot_read(fd, 0, &sa, g_text, BD_TEXT_MAX, &la);
    int b = bd_report_slot_read(fd, 1, &sb, tmp, BD_SLOT, &lb);
    if (a == 0 && (b != 0 || sa >= sb)) { g_len = la; g_rep_seq = sa; }
    else if (b == 0) { memcpy(g_text, tmp, lb); g_len = lb; g_rep_seq = sb; }
    else { g_len = 0; g_rep_seq = 0; }
    free(tmp);
    g_text[g_len] = 0;

    g_rep_fd = fd;
    snprintf(g_rep_part, sizeof g_rep_part, "%s", part);

    if (g_len + 128 >= BD_TEXT_MAX) g_len = 0;
    char sep[96];
    int n = snprintf(sep, sizeof sep, "\n===== NOVO BOOT (anterior: %ld bytes) =====\n", (long)g_len);
    bd_append(sep, (size_t)n);

    snprintf(g_rep_loc, sizeof g_rep_loc,
             "partição '%s' offset %llu (slot %u) — reler com: "
             "dd if=/dev/block/by-name/%s of=/tmp/blog.bin bs=4096 skip=%llu count=%llu",
             part, (unsigned long long)(BD_BASE / 4096), g_rep_seq & 1u, part,
             (unsigned long long)(BD_BASE / 4096), (unsigned long long)(BD_SLOT * 2 / 4096));
    bd_log("report: %s", g_rep_loc);
    return 0;
}

const char *bd_report_location(void) { return g_rep_loc[0] ? g_rep_loc : "NOT AVAILABLE"; }

void bd_flush(void)
{
    if (g_rep_fd < 0 || g_len == 0) return;
    size_t n = g_len;
    if (n > BD_SLOT - sizeof(bd_hdr)) n = BD_SLOT - sizeof(bd_hdr);

    bd_hdr h = { BD_MAGIC, ++g_rep_seq, (uint32_t)n, bd_crc32((const uint8_t *)g_text, n) };
    unsigned slot = g_rep_seq & 1u;
    off_t off = (off_t)(BD_BASE + (unsigned long long)slot * BD_SLOT);

    char *blob = malloc(sizeof h + n);
    if (!blob) return;
    memcpy(blob, &h, sizeof h);
    memcpy(blob + sizeof h, g_text, n);
    ssize_t w = pwrite(g_rep_fd, blob, sizeof h + n, off);
    free(blob);
    if (w != (ssize_t)(sizeof h + n)) {
        bd_log("report: gravação falhou (%zd, errno=%d)", w, errno);
        return;
    }
    fsync(g_rep_fd);
}

/* ===================================================================== */
/* serial: gadget já configurado; aqui só abrimos ttyGS0 e esperamos DTR  */
/* ===================================================================== */

static int g_ser_host_seen;
static int g_ser_logged_wait;

static int bd_mknod_from_sysfs(const char *name)
{
    char p[128], buf[64];
    snprintf(p, sizeof p, "/sys/class/tty/%s/dev", name);
    FILE *f = fopen(p, "r");
    if (!f) return -1;
    if (!fgets(buf, sizeof buf, f)) { fclose(f); return -1; }
    fclose(f);
    unsigned ma = 0, mi = 0;
    if (sscanf(buf, "%u:%u", &ma, &mi) != 2) return -1;
    char node[96];
    snprintf(node, sizeof node, "/dev/%s", name);
    if (access(node, F_OK) != 0) {
        if (mknod(node, S_IFCHR | 0666, makedev(ma, mi)) && errno != EEXIST) return -1;
    }
    return 0;
}

int bd_serial_attach_config(void)
{
    /* tenta os nomes possíveis do gadget ACM (acm.usb0 cria ttyGS0+. */
    const char *names[] = { "ttyGS0", "ttyGS1", "ttyGS2", "ttyGS3", NULL };
    for (int i = 0; names[i]; i++) {
        bd_mknod_from_sysfs(names[i]);
        char node[64];
        snprintf(node, sizeof node, "/dev/%s", names[i]);
        int fd = open(node, O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (fd < 0) continue;

        struct termios t;
        if (tcgetattr(fd, &t) == 0) {
            cfmakeraw(&t);
            t.c_cflag |= (CLOCAL | CREAD);   /* cfmakeraw NÃO liga CLOCAL: sem isso, */
            t.c_cflag &= ~CRTSCTS;           /* escrita pode falhar sem DCD          */
            t.c_cc[VMIN] = 0;
            t.c_cc[VTIME] = 0;
            tcsetattr(fd, TCSANOW, &t);
        }
        g_ser = fd;
        bd_log("serial: %s aberta (fd=%d) — aguardando o host abrir a porta (DTR)", node, fd);
        return 0;
    }
    return -1;
}

int bd_serial_bind(void)
{
    if (g_ser >= 0) return 0;
    if (bd_serial_attach_config() != 0) {
        bd_log("serial: nenhum ttyGS* disponível — o gadget configfs/UDC falhou antes");
        return -1;
    }
    return 0;
}

int bd_serial_host_open(void)
{
    if (g_ser < 0) return 0;
    int bits = 0;
    if (ioctl(g_ser, TIOCMGET, &bits) != 0) return 0;
    /* o gadget acm reporta TIOCM_DTR quando o host abriu a porta e asseriu DTR */
    return (bits & TIOCM_DTR) ? 1 : 0;
}

static void bd_serial_write_chunked(const char *s, size_t n)
{
    size_t off = 0;
    while (off < n) {
        size_t chunk = n - off > 1024 ? 1024 : n - off;
        ssize_t w = write(g_ser, s + off, chunk);
        if (w <= 0) return;                 /* EAGAIN: o resto fica na fila */
        off += (size_t)w;
    }
    /* remove o que foi enviado */
    if (off >= g_serq_len) g_serq_len = 0;
    else { memmove(g_serq, g_serq + off, g_serq_len - off); g_serq_len -= off; }
}

void bd_serial_pump(void)
{
    if (g_ser < 0) return;

    int host = bd_serial_host_open();
    if (host && !g_ser_host_seen) {
        g_ser_host_seen = 1;
        bd_log("serial: HOST ABRIU A PORTA — despejando relatório (%zu bytes)", g_len);
        char head[160];
        int n = snprintf(head, sizeof head,
                         "\r\n===== BANKPHONE BOOT REPORT (%zu bytes) =====\r\n", g_len);
        bd_serial_write_chunked(head, (size_t)n);
        /* despeja a CAUDA do relatório (o começo já foi para /dev/kmsg) */
        size_t start = g_len > BD_KEEP_HEAD ? g_len - BD_KEEP_HEAD : 0;
        bd_serial_write_chunked(g_text + start, g_len - start);
        bd_serial_write_chunked("===== FIM DO RELATÓRIO =====\r\n", 30);
        g_serq_len = 0;
        return;
    }
    if (g_serq_len) bd_serial_write_chunked(g_serq, g_serq_len);
    if (!host && !g_ser_logged_wait) {
        g_ser_logged_wait = 1;
        bd_log("serial: porta aberta no device, mas o host ainda não abriu a porta "
               "(use /dev/cu.usbmodem* no macOS, NÃO /dev/tty.usbmodem*)");
    }
}

/* ===================================================================== */
/* display                                                              */
/* ===================================================================== */

bd_fb g_bd_fb;

static uint32_t bd_pack_chan(uint32_t v8, uint32_t off, uint32_t len)
{
    if (len == 0) return 0;
    if (len > 8) len = 8;
    return (v8 >> (8 - len)) << off;
}

static void bd_put_px(bd_fb *o, int x, int y, uint32_t rgb)
{
    if (!o->ready || !o->mem) return;
    if (x < 0 || y < 0 || x >= o->w || y >= o->h) return;
    uint8_t *row = o->mem + (size_t)(y + o->yoff) * (size_t)o->stride;
    uint32_t r = (rgb >> 16) & 255u, g = (rgb >> 8) & 255u, b = rgb & 255u;

    switch (o->bpp) {
    case 32: {
        uint32_t *p = (uint32_t *)(void *)(row + (size_t)(x + o->xoff) * 4u);
        *p = bd_pack_chan(r, o->red_off, o->red_len) |
             bd_pack_chan(g, o->green_off, o->green_len) |
             bd_pack_chan(b, o->blue_off, o->blue_len);
        break;
    }
    case 24: {
        uint8_t *p = row + (size_t)(x + o->xoff) * 3u;
        p[0] = (uint8_t)b; p[1] = (uint8_t)g; p[2] = (uint8_t)r;   /* BGR em memória */
        break;
    }
    case 16: {
        uint16_t *p = (uint16_t *)(void *)(row + (size_t)(x + o->xoff) * 2u);
        uint32_t v = bd_pack_chan(r, o->red_off, o->red_len ? o->red_len : 5u) |
                     bd_pack_chan(g, o->green_off, o->green_len ? o->green_len : 6u) |
                     bd_pack_chan(b, o->blue_off, o->blue_len ? o->blue_len : 5u);
        *p = (uint16_t)v;
        break;
    }
    default:
        break;
    }
}

void bd_fb_fill(bd_fb *o, uint32_t rgb)
{
    for (int y = 0; y < o->h; y++)
        for (int x = 0; x < o->w; x++)
            bd_put_px(o, x, y, rgb);
}

void bd_fb_rect(bd_fb *o, int x, int y, int w, int h, uint32_t rgb)
{
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++)
            bd_put_px(o, i, j, rgb);
}

/* 7 segmentos: a=1 b=2 c=4 d=8 e=16 f=32 g=64 */
static const uint8_t BD_SEG[10] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
};

static void bd_digit(bd_fb *o, int x, int y, int dw, int dh, int thick, int d, uint32_t rgb)
{
    uint8_t s = BD_SEG[d % 10];
    int hw = dw, hh = dh / 2;
    if (s & 0x01) bd_fb_rect(o, x + thick, y, hw - 2 * thick, thick, rgb);                       /* a */
    if (s & 0x02) bd_fb_rect(o, x + hw - thick, y + thick, thick, hh - thick, rgb);              /* b */
    if (s & 0x04) bd_fb_rect(o, x + hw - thick, y + hh, thick, hh - thick, rgb);                 /* c */
    if (s & 0x08) bd_fb_rect(o, x + thick, y + dh - thick, hw - 2 * thick, thick, rgb);          /* d */
    if (s & 0x10) bd_fb_rect(o, x, y + hh, thick, hh - thick, rgb);                              /* e */
    if (s & 0x20) bd_fb_rect(o, x, y + thick, thick, hh - thick, rgb);                           /* f */
    if (s & 0x40) bd_fb_rect(o, x + thick, y + hh - thick / 2, hw - 2 * thick, thick, rgb);      /* g */
}

/* número grande no canto superior esquerdo sobre faixa escura */
void bd_fb_stage(bd_fb *o, int value, uint32_t rgb)
{
    if (!o->ready) return;
    int dh = o->h / 5;                 /* ~322 px em 1612 */
    if (dh > 260) dh = 260;
    if (dh < 60)  dh = o->h / 4;
    int dw = dh / 2, thick = dw / 5;
    if (thick < 3) thick = 3;

    char txt[8];
    int nd = snprintf(txt, sizeof txt, "%d", value);
    int pad = thick * 2;

    bd_fb_rect(o, 0, 0, dw * nd + pad * 2, dh + pad * 2, 0x000000);
    for (int i = 0; i < nd; i++)
        bd_digit(o, pad + i * dw, pad, dw, dh, thick, txt[i] - '0', rgb);
    bd_fb_rect(o, 0, dh + pad * 2, o->w, thick * 2, rgb);   /* régua do estágio */
}

void bd_fb_bars(bd_fb *o)
{
    if (!o->ready) return;
    static const uint32_t C[8] = {
        0xFFFFFF, 0xFF0000, 0x00FF00, 0x0000FF,
        0x000000, 0xFFFF00, 0x00FFFF, 0xFF00FF
    };
    int bw = o->w / 8;
    for (int i = 0; i < 8; i++) bd_fb_rect(o, i * bw, o->h / 3, bw, o->h / 3, C[i]);
    bd_fb_rect(o, 0, 0, o->w, 4, 0xFFFFFF);                 /* borda: 0,0 é o canto certo */
    bd_fb_rect(o, 0, o->h - 4, o->w, 4, 0xFFFFFF);
}

void bd_fb_flush(bd_fb *o)
{
    if (o->fd < 0) return;
    struct fb_var_screeninfo vi;
    memset(&vi, 0, sizeof vi);
    if (ioctl(o->fd, FBIOGET_VSCREENINFO, &vi) != 0) return;
    vi.yoffset = 0;
    vi.activate = FB_ACTIVATE_NOW;
    o->pan_rc = ioctl(o->fd, FBIOPAN_DISPLAY, &vi);
}

static int bd_fb_mknod(const char *name)
{
    char p[128], buf[64];
    snprintf(p, sizeof p, "/sys/class/graphics/%s/dev", name);
    FILE *f = fopen(p, "r");
    if (!f) return -1;
    if (!fgets(buf, sizeof buf, f)) { fclose(f); return -1; }
    fclose(f);
    unsigned ma = 0, mi = 0;
    if (sscanf(buf, "%u:%u", &ma, &mi) != 2) return -1;
    char dir[64], node[96];
    snprintf(dir, sizeof dir, "/dev/graphics");
    mkdir(dir, 0755);
    snprintf(node, sizeof node, "/dev/graphics/%s", name);
    if (access(node, F_OK) != 0 && mknod(node, S_IFCHR | 0600, makedev(ma, mi)) && errno != EEXIST)
        return -1;
    return 0;
}

int bd_fb_open(bd_fb *o)
{
    memset(o, 0, sizeof *o);
    o->fd = -1;
    o->unblank_rc = o->put_rc = o->pan_rc = -999;

    if (access("/sys/class/graphics", F_OK) != 0)
        bd_log("fb: /sys/class/graphics AUSENTE — o kernel não registrou nenhum fbdev");

    const char *cands[] = { "/dev/graphics/fb0", "/dev/fb0", NULL };
    for (int i = 0; cands[i] && o->fd < 0; i++) {
        o->fd = open(cands[i], O_RDWR);
        if (o->fd >= 0) snprintf(o->node, sizeof o->node, "%s", cands[i]);
    }
    if (o->fd < 0 && bd_fb_mknod("fb0") == 0) {
        o->fd = open("/dev/graphics/fb0", O_RDWR);
        if (o->fd >= 0) snprintf(o->node, sizeof o->node, "/dev/graphics/fb0");
    }
    if (o->fd < 0) {
        bd_log("fb: nenhum framebuffer abrível (errno=%d)", errno);
        return -BD_F_FB_NODE;
    }

    struct fb_fix_screeninfo fi;
    struct fb_var_screeninfo vi;
    if (ioctl(o->fd, FBIOGET_VSCREENINFO, &vi) != 0) { bd_log("fb: FBIOGET_VSCREENINFO errno=%d", errno); return -BD_F_FB_IOCTL; }
    if (ioctl(o->fd, FBIOGET_FSCREENINFO, &fi) != 0) { bd_log("fb: FBIOGET_FSCREENINFO errno=%d", errno); return -BD_F_FB_IOCTL; }

    o->w = (int)vi.xres; o->h = (int)vi.yres;
    o->bpp = (int)vi.bits_per_pixel;
    o->stride = (int)fi.line_length;
    o->xoff = (int)vi.xoffset; o->yoff = (int)vi.yoffset;
    o->yres_virtual = (int)vi.yres_virtual;
    o->red_off = vi.red.offset;   o->red_len = vi.red.length;
    o->green_off = vi.green.offset; o->green_len = vi.green.length;
    o->blue_off = vi.blue.offset;  o->blue_len = vi.blue.length;

    bd_log("fb: %s id=%s %dx%d bpp=%d stride=%d yres_virtual=%d red=%u/%u green=%u/%u blue=%u/%u",
           o->node, fi.id, o->w, o->h, o->bpp, o->stride, o->yres_virtual,
           o->red_off, o->red_len, o->green_off, o->green_len, o->blue_off, o->blue_len);

    if (o->bpp != 16 && o->bpp != 24 && o->bpp != 32) {
        bd_log("fb: bpp=%d não suportado por este diagnóstico", o->bpp);
        return -BD_F_FB_BPP;
    }
    if (o->stride <= 0) { o->stride = o->w * (o->bpp / 8); bd_log("fb: stride inválido, assumindo %d", o->stride); }

    size_t need = (size_t)o->stride * (size_t)(o->yoff + o->h);
    size_t map  = fi.smem_len > need ? fi.smem_len : need;
    o->mem = mmap(NULL, map, PROT_READ | PROT_WRITE, MAP_SHARED, o->fd, 0);
    if (o->mem == MAP_FAILED) { o->mem = NULL; bd_log("fb: mmap(%zu) falhou errno=%d", map, errno); return -BD_F_FB_MMAP; }
    o->map_len = map;
    o->ready = 1;
    return 0;
}

int bd_fb_power(bd_fb *o)
{
    if (o->fd < 0) return -BD_F_FB_NODE;

    /* 1) FBIOBLANK — no MTK é aqui que o LCM é inicializado/ligado */
    errno = 0;
    o->unblank_rc = ioctl(o->fd, FBIOBLANK, FB_BLANK_UNBLANK);
    bd_log("fb: FBIOBLANK(UNBLANK) = %d (errno=%d)", o->unblank_rc, o->unblank_rc ? errno : 0);

    /* 2) equivalente por sysfs (alguns drivers ignoram o ioctl) */
    {
        char p[128];
        DIR *d = opendir("/sys/class/graphics");
        struct dirent *e;
        char name[272] = "";
        if (d) {
            while ((e = readdir(d))) if (!strncmp(e->d_name, "fb", 2)) { snprintf(name, sizeof name, "%s", e->d_name); break; }
            closedir(d);
        }
        if (!name[0]) snprintf(name, sizeof name, "fb0");
        snprintf(p, sizeof p, "/sys/class/graphics/%s/blank", name);
        int f = open(p, O_WRONLY);
        if (f >= 0) { ssize_t w = write(f, "0", 1); close(f); bd_log("fb: %s <- 0 (write=%zd)", p, w); }
        else bd_log("fb: %s indisponível (errno=%d)", p, errno);
    }

    /* 3) FBIOPUT_VSCREENINFO com FORCE — ANTES de desenhar (o force pode
     *    re-inicializar o disp e descartar o que já estava na memória) */
    struct fb_var_screeninfo vi;
    memset(&vi, 0, sizeof vi);
    if (ioctl(o->fd, FBIOGET_VSCREENINFO, &vi) == 0) {
        vi.activate = FB_ACTIVATE_NOW | FB_ACTIVATE_FORCE;
        errno = 0;
        o->put_rc = ioctl(o->fd, FBIOPUT_VSCREENINFO, &vi);
        bd_log("fb: FBIOPUT_VSCREENINFO(FORCE) = %d (errno=%d)", o->put_rc, o->put_rc ? errno : 0);
    }
    usleep(120 * 1000);
    return (o->unblank_rc == 0 || o->put_rc == 0) ? 0 : -BD_F_FB_POWER;
}

/* FBIOBLANK cru, sem mais nada: quem precisa do ciclo POWERDOWN->UNBLANK é o
 * driver de toque (o resume do NT36xxx é o que baixa o firmware do chip). */
int bd_fb_blank(bd_fb *o, int blank)
{
    if (!o || o->fd < 0) return -BD_F_FB_NODE;
    const char *nome = blank == FB_BLANK_UNBLANK ? "UNBLANK" :
                       blank == FB_BLANK_POWERDOWN ? "POWERDOWN" : "?";
    errno = 0;
    int rc = ioctl(o->fd, FBIOBLANK, blank);
    bd_log("fb: FBIOBLANK(%s) = %d (errno=%d)", nome, rc, rc ? errno : 0);
    return rc;
}

/* ===================================================================== */
/* estágios                                                             */
/* ===================================================================== */

static int g_stage;

void bd_stage(int stage)
{
    g_stage = stage;
    uint32_t color = 0x00C0FF;                 /* ciano: progresso */
    if (stage >= BD_ST_FAIL) color = 0xFF3030; /* vermelho: falha   */
    bd_log("STAGE %d", stage);
    if (g_bd_fb.ready) {
        bd_fb_bars(&g_bd_fb);                  /* mantém as barras por baixo */
        bd_fb_stage(&g_bd_fb, stage, color);
        bd_fb_flush(&g_bd_fb);
    }
    bd_flush();
}

void bd_fail(int code, const char *why)
{
    bd_log("FAIL code=%d reason=%s", code, why ? why : "(sem motivo)");
    if (g_bd_fb.ready) {
        bd_fb_fill(&g_bd_fb, 0x300000);
        bd_fb_stage(&g_bd_fb, BD_ST_FAIL, 0xFF3030);
        bd_fb_stage(&g_bd_fb, code, 0xFF3030);
        bd_fb_flush(&g_bd_fb);
    }
    bd_flush();
}

/* ===================================================================== */
/* backlight                                                            */
/* ===================================================================== */

static int bd_write_and_read(const char *path, const char *val)
{
    int f = open(path, O_WRONLY);
    if (f < 0) return -1;
    ssize_t w1 = write(f, val, strlen(val));
    close(f);
    if (w1 <= 0) return -2;

    /* muitos drivers só aplicam na segunda escrita (latch) — e a leitura de volta
     * é a única prova de que o valor "pegou" */
    f = open(path, O_WRONLY);
    if (f >= 0) { ssize_t w2 = write(f, val, strlen(val)); (void)w2; close(f); }

    char buf[32] = "";
    f = open(path, O_RDONLY);
    if (f >= 0) { ssize_t r = read(f, buf, sizeof buf - 1); (void)r; close(f); }
    bd_log("bl: %s <- %s (lido de volta: %s)", path, val, buf[0] ? buf : "n/d");
    return 0;
}

static int bd_backlight_dir(const char *dir, int pct)
{
    DIR *d = opendir(dir);
    if (!d) return 0;
    struct dirent *e;
    int hits = 0;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char maxp[400], brp[400];
        snprintf(maxp, sizeof maxp, "%s/%s/max_brightness", dir, e->d_name);
        snprintf(brp,  sizeof brp,  "%s/%s/brightness",     dir, e->d_name);

        long mx = 255;
        FILE *f = fopen(maxp, "r");
        if (f) { if (fscanf(f, "%ld", &mx) != 1) mx = 255; fclose(f); }
        if (mx <= 0) mx = 255;

        long v = (long)mx * (pct < 0 ? 0 : pct > 100 ? 100 : pct) / 100;
        if (v < 1 && pct > 0) v = 1;
        char val[24];
        snprintf(val, sizeof val, "%ld", v);
        if (bd_write_and_read(brp, val) == 0) hits++;
    }
    closedir(d);
    return hits;
}

int bd_backlight(int percent)
{
    int hits = 0;
    hits += bd_backlight_dir("/sys/class/leds", percent);
    hits += bd_backlight_dir("/sys/class/backlight", percent);
    if (!hits) bd_log("bl: NENHUM node de backlight — tela ligada com brilho zero (preta)");
    return hits;
}

/* ===================================================================== */
/* inventário                                                           */
/* ===================================================================== */

static void bd_list_dir(const char *dir, const char *tag)
{
    DIR *d = opendir(dir);
    if (!d) { bd_log("inv: %s AUSENTE", dir); return; }
    char line[1200];
    int n = snprintf(line, sizeof line, "inv: %s =", dir);
    struct dirent *e;
    int c = 0;
    while ((e = readdir(d)) && n < (int)sizeof line - 64) {
        if (e->d_name[0] == '.') continue;
        n += snprintf(line + n, sizeof line - (size_t)n, " %s", e->d_name);
        c++;
    }
    closedir(d);
    if (!c) n += snprintf(line + n, sizeof line - (size_t)n, " (vazio)");
    bd_log("%s", line);
    (void)tag;
}

static void bd_dump_file(const char *path, const char *tag)
{
    FILE *f = fopen(path, "r");
    if (!f) { bd_log("inv: %s -> AUSENTE", path); return; }
    char buf[768];
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[n] = 0;
    for (size_t i = 0; i < n; i++) if (buf[i] == '\n' || buf[i] == '\r') buf[i] = ' ';
    bd_log("inv: %s = %s", tag ? tag : path, buf);
}

void bd_dump_inventory(void)
{
    bd_log("--- INVENTÁRIO ---");
    bd_dump_file("/proc/cmdline", "cmdline");
    bd_dump_file("/proc/device-tree/model", "dt.model");
    bd_dump_file("/proc/device-tree/compatible", "dt.compatible");
    bd_dump_file("/sys/class/graphics/fb0/name", "fb0.name");
    bd_dump_file("/sys/class/graphics/fb0/blank", "fb0.blank");
    bd_dump_file("/proc/sys/kernel/panic", "panic");
    bd_dump_file("/proc/sys/kernel/panic_on_oops", "panic_on_oops");

    bd_list_dir("/sys/class/graphics", "graphics");
    bd_list_dir("/sys/class/drm", "drm");
    bd_list_dir("/sys/class/backlight", "backlight");
    bd_list_dir("/sys/class/leds", "leds");
    bd_list_dir("/sys/class/udc", "udc");
    bd_list_dir("/sys/class/input", "input");
    bd_list_dir("/sys/class/watchdog", "watchdog");
    bd_list_dir("/sys/class/tty", "tty(muitos)");
    bd_list_dir("/sys/devices/platform", "platform");

    /* pstore / last_kmsg: o panic do boot ANTERIOR */
    DIR *d = opendir("/sys/fs/pstore");
    if (d) {
        struct dirent *e;
        int any = 0;
        while ((e = readdir(d))) {
            if (e->d_name[0] == '.') continue;
            any = 1;
            char p[320];
            snprintf(p, sizeof p, "/sys/fs/pstore/%s", e->d_name);
            bd_log("pstore: %s presente", e->d_name);
            bd_dump_file(p, e->d_name);
        }
        closedir(d);
        if (!any) bd_log("pstore: vazio (nenhum panic registrado no boot anterior)");
    } else {
        bd_log("pstore: /sys/fs/pstore AUSENTE (ramoops não configurado)");
    }
    bd_dump_file("/proc/last_kmsg", "last_kmsg");

    /* dispositivos de entrada: nome + capacidades (é isto que decide o touch) */
    {
        FILE *f = fopen("/proc/bus/input/devices", "r");
        if (f) {
            char line[512];
            while (fgets(line, sizeof line, f)) {
                size_t n = strlen(line);
                while (n && (line[n-1] == '\n' || line[n-1] == '\r')) line[--n] = 0;
                if (n) bd_log("input| %s", line);
            }
            fclose(f);
        } else bd_log("input: /proc/bus/input/devices AUSENTE");
    }

    bd_dump_file("/proc/partitions", "partitions");
    bd_dump_file("/proc/mounts", "mounts");
    bd_log("--- FIM DO INVENTÁRIO ---");
}

/* ===================================================================== */
/* watchdog                                                             */
/* ===================================================================== */

void bd_watchdog_probe(void)
{
    DIR *d = opendir("/sys/class/watchdog");
    if (!d) { bd_log("wdt: /sys/class/watchdog AUSENTE"); return; }
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char st[400], to[400], id[400];
        snprintf(st, sizeof st, "/sys/class/watchdog/%s/state", e->d_name);
        snprintf(to, sizeof to, "/sys/class/watchdog/%s/timeout", e->d_name);
        snprintf(id, sizeof id, "/sys/class/watchdog/%s/identity", e->d_name);
        char s[64] = "?", t[64] = "?", i[64] = "?";
        FILE *f = fopen(st, "r"); if (f) { if (!fgets(s, sizeof s, f)) {} fclose(f); }
        f = fopen(to, "r"); if (f) { if (!fgets(t, sizeof t, f)) {} fclose(f); }
        f = fopen(id, "r"); if (f) { if (!fgets(i, sizeof i, f)) {} fclose(f); }
        size_t n;
        n = strlen(s); if (n) s[n-1] = 0;
        n = strlen(t); if (n) t[n-1] = 0;
        n = strlen(i); if (n) i[n-1] = 0;
        bd_log("wdt: %s state=%s timeout=%s identity=%s", e->d_name, s, t, i);
    }
    closedir(d);
    if (access("/dev/watchdog", F_OK) == 0)
        bd_log("wdt: /dev/watchdog existe — NÃO abrir sem alimentar (abrir e não pet => reset); "
               "fechar sem o caractere 'V' também pode reiniciar");
    else
        bd_log("wdt: /dev/watchdog AUSENTE");
}

/* ===================================================================== */
/* reboot                                                               */
/* ===================================================================== */

/* bootloader_message (AOSP) — apenas o campo command é alterado;
 * o resto da partição `misc` é preservado byte a byte. */
struct bd_blmsg {
    char command[32];
    char status[32];
    char recovery[768];
    char stage[32];
    char reserved[1184];
};

static int bd_write_bcb(const char *command)
{
    int fd = bd_block_open("misc", O_RDWR);
    if (fd < 0) { bd_log("bcb: partição 'misc' não encontrada (errno=%d)", errno); return -1; }

    struct bd_blmsg msg;
    memset(&msg, 0, sizeof msg);
    ssize_t r = pread(fd, &msg, sizeof msg, 0);
    if (r != (ssize_t)sizeof msg) bd_log("bcb: leitura parcial (%zd) — seguindo", r);

    memset(msg.command, 0, sizeof msg.command);
    snprintf(msg.command, sizeof msg.command, "%s", command);

    ssize_t w = pwrite(fd, &msg, sizeof msg, 0);   /* só os primeiros 2 KiB */
    fsync(fd);
    close(fd);
    if (w != (ssize_t)sizeof msg) { bd_log("bcb: escrita falhou (%zd, errno=%d)", w, errno); return -1; }
    bd_log("bcb: command='%s' gravado em misc (resto da partição preservado)", command);
    return 0;
}

void bd_reboot_restart2(const char *target)
{
    bd_log("reboot: RESTART2 target='%s' (nenhuma partição foi gravada)", target ? target : "system");
    bd_flush();
    sync();
    if (target && *target) {
        syscall(SYS_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
                LINUX_REBOOT_CMD_RESTART2, target);
    }
    syscall(SYS_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_RESTART, NULL);
    for (;;) pause();
}

void bd_reboot_target(const char *target)
{
    bd_log("reboot: target='%s' — incluindo BCB em `misc`", target ? target : "system");
    bd_flush();

    if (target && (!strcmp(target, "bootloader") || !strcmp(target, "fastboot"))) {
        bd_write_bcb("bootonce-bootloader");   /* é assim que o LK/Android entra em fastboot */
    } else if (target && !strcmp(target, "recovery")) {
        bd_write_bcb("boot-recovery");
    }
    bd_reboot_restart2(target);
}

void bd_poweroff(void)
{
    bd_log("poweroff");
    bd_flush();
    bd_backlight(0);
    sync();
    syscall(SYS_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_POWER_OFF, NULL);
    for (;;) pause();
}

/* ===================================================================== */
/* init / dump                                                          */
/* ===================================================================== */

void bd_init(void)
{
    g_kmsg = open("/dev/kmsg", O_WRONLY);

    FILE *f = fopen("/proc/cmdline", "r");
    if (f) {
        if (!fgets(g_cmdline, sizeof g_cmdline, f)) g_cmdline[0] = 0;
        fclose(f);
        size_t n = strlen(g_cmdline);
        while (n && (g_cmdline[n-1] == '\n' || g_cmdline[n-1] == '\r')) g_cmdline[--n] = 0;
    }
    bd_log("=== BANKPHONE BOOT (bootdiag) ===");
    bd_log("cmdline: %s", g_cmdline[0] ? g_cmdline : "(vazio)");
}

void bd_report_dump(void)
{
    /* relatório inteiro para o serial (em pedaços) e para o kmsg (só o começo) */
    if (g_ser >= 0) {
        bd_serq_push(g_text, g_len);
        bd_serial_pump();
    }
    bd_log("report: %zu bytes no total; local: %s", g_len, bd_report_location());
    if (g_klog) bd_log("report: dmesg disponível (%zu bytes) via bd_klog_snapshot()", g_klog_len);
}

void bd_note_written_frames(unsigned long n)
{
    static unsigned long last;
    if (n - last >= 60) {           /* ~1 min de quadros */
        last = n;
        bd_log("hb: quadros=%lu stage=%d fb=%dx%d@%dbpp ser=%d persist=%s",
               n, g_stage, g_bd_fb.w, g_bd_fb.h, g_bd_fb.bpp, g_ser,
               g_rep_fd >= 0 ? "sim" : "nao");
        bd_flush();
    }
}
