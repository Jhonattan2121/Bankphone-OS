// BANKPHONE OS — PID 1 do Infinix Hot 30i X669C (MT6765H). Kernel Linux do próprio aparelho, userspace nosso. Sem Android.
//
// Integração do bootdiag (bootdiag.c). Ordem de operações e códigos de estágio: veja os STAGE no log de boot.
//
// Garantias deste arquivo:
//   1. NUNCA reinicia sozinho. Toda falha fica viva, registrada na TELA, no relatório persistente e na serial.
//   2. Barras de cor sólidas (prova de display) ANTES de qualquer UI ou fonte.
//   3. /sys/fs/pstore lido no boot e anexado ao relatório (o console que sobrevive a panic/reboot).
//   4. Vol+ sustentado >= 3 s => bootloader; Vol- => reiniciar (escape imediato); Power => tela.
//      Por padrão o reboot usa RESTART2 e NÃO grava nenhuma partição. Com -DBANKPHONE_BCB_REBOOT
//      (ou o arquivo /tmp/bcb-reboot presente) ele também grava o BCB em `misc` — que é o caminho
//      confiável para o LK entrar em fastboot no Android.
//   5. Com "bankphone.ro=1" na cmdline, o store NÃO abre a partição: zero gravação, ideal no primeiro teste.
//   6. PID 1 nunca retorna (se /init sai, o kernel entra em panic).
#define _GNU_SOURCE
#include "bootdiag.h"
#include "devcmd.h"
#include "gfx.h"
#include "money.h"
#include "sec.h"
#include "store.h"
#include "touchcore.h"
#include "ui.h"
#include "hw.h"
#include "nav.h"
#include "components.h"
#include "store.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

// ------------------------------------------------------------------ estágios
// O número é pintado na tela. Foto da tela parada = diagnóstico sem PC.
enum {
    ST_MOUNTS = 1, ST_KMSG, ST_DMESG, ST_REPORT, ST_PSTORE, ST_INVENTORY,
    ST_USB, ST_SERIAL, ST_FB_OPEN, ST_FB_POWER, ST_BARS, ST_BACKLIGHT,
    ST_FONT, ST_UI, ST_INPUT, ST_LOOP
};
// códigos de FALHA (aparecem como "90" + código; ver bootdiag.h para 1..11)
#define F_FONT 12
#define F_ASSETS 13
#define F_UI 14
#define F_PSTORE 15

static int stage_now = ST_MOUNTS;
static int fb_ok, font_ok, touch_ok, ui_live, safe_mode;
static time_t boot_epoch;        /* instante do início, para o uptime do diagnóstico */
static time_t hw_last_report;    /* última vez que a tabela de hardware foi ao log */

static void stage(int s)
{
    stage_now = s;
    if (!ui_live) bd_stage(s);          // desenha na tela (barras + número) e persiste
    else { bd_log("STAGE %d", s); bd_flush(); }
}

// ------------------------------------------------------------------ nós /dev
// O kernel não cria os nós sozinho; lemos /sys/dev e criamos em /dev.
static void make_nodes(void)
{
    const char *kinds[] = { "char", "block" };
    for (int k = 0; k < 2; k++) {
        char dir[40]; snprintf(dir, sizeof dir, "/sys/dev/%s", kinds[k]);
        DIR *d = opendir(dir); if (!d) continue;
        struct dirent *e;
        while ((e = readdir(d))) {
            unsigned ma, mi; if (sscanf(e->d_name, "%u:%u", &ma, &mi) != 2) continue;
            char up[96], l[160], name[96] = ""; snprintf(up, sizeof up, "%s/%s/uevent", dir, e->d_name);
            FILE *f = fopen(up, "r"); if (!f) continue;
            while (fgets(l, sizeof l, f)) if (!strncmp(l, "DEVNAME=", 8)) { snprintf(name, sizeof name, "%s", l + 8); name[strcspn(name, "\n")] = 0; }
            fclose(f);
            if (!name[0]) continue;
            char path[130]; snprintf(path, sizeof path, "/dev/%s", name);
            for (char *c = path + 5; *c; c++) if (*c == '/') { *c = 0; mkdir(path, 0755); *c = '/'; }
            if (access(path, F_OK) == 0) continue;               // devtmpfs já criou: não mexer
            mknod(path, (k ? S_IFBLK : S_IFCHR) | 0660, makedev(ma, mi));
        }
        closedir(d);
    }
}

// ------------------------------------------------------------------- USB/ACM
// Mantido do código original: configfs + ACM + bind do UDC. A abertura de
// /dev/ttyGS0 agora vem do bootdiag (raw + CLOCAL + não bloqueante + DTR).
static void usb_serial(void)
{
    mkdir("/config", 0755);
    if (mount("none", "/config", "configfs", 0, NULL) && errno != EBUSY) { bd_log("configfs: errno %d", errno); return; }
    const char *G = "/config/usb_gadget/bp";
    mkdir("/config/usb_gadget", 0755); mkdir(G, 0755);
    char p[160];
    #define W_(rel, v) do { snprintf(p, sizeof p, "%s/%s", G, rel); { int _f = open(p, O_WRONLY); if (_f >= 0) { if (write(_f, v, strlen(v)) < 0) {} close(_f); } else bd_log("usb: %s não abriu (errno %d)", p, errno); } } while (0)
    W_("idVendor", "0x1d6b"); W_("idProduct", "0x0104"); W_("bcdUSB", "0x0200");
    snprintf(p, sizeof p, "%s/strings/0x409", G); mkdir(p, 0755);
    W_("strings/0x409/serialnumber", "BANKPHONE01"); W_("strings/0x409/manufacturer", "BANKPHONE"); W_("strings/0x409/product", "BANKPHONE OS");
    snprintf(p, sizeof p, "%s/configs/c.1", G); mkdir(p, 0755);
    snprintf(p, sizeof p, "%s/configs/c.1/strings/0x409", G); mkdir(p, 0755);
    W_("configs/c.1/strings/0x409/configuration", "serial"); W_("configs/c.1/MaxPower", "250");
    snprintf(p, sizeof p, "%s/functions/acm.usb0", G); mkdir(p, 0755);
    char a[160]; snprintf(a, sizeof a, "%s/functions/acm.usb0", G); snprintf(p, sizeof p, "%s/configs/c.1/acm.usb0", G);
    if (symlink(a, p) && errno != EEXIST) { bd_log("symlink acm: errno %d", errno); return; }
    DIR *d = opendir("/sys/class/udc"); if (!d) { bd_log("sem /sys/class/udc"); return; }
    struct dirent *e; char udc[80] = ""; while ((e = readdir(d))) if (e->d_name[0] != '.') snprintf(udc, sizeof udc, "%s", e->d_name); closedir(d);
    if (!udc[0]) { bd_log("sem UDC"); return; }
    W_("UDC", udc);
    bd_log("usb: gadget vinculado ao UDC '%s'", udc);
    #undef W_
}

// ------------------------------------------------------------------ display
static uint32_t *back;                        // buffer de desenho da UI (FW*FH)
static int FW, FH;

static uint32_t pack565(uint32_t c, const bd_fb *o)
{
    uint32_t r = (c >> 16) & 255u, g = (c >> 8) & 255u, b = c & 255u;
    uint32_t rl = o->red_len ? o->red_len : 5, gl = o->green_len ? o->green_len : 6, bl = o->blue_len ? o->blue_len : 5;
    return ((r >> (8 - rl)) << o->red_off) | ((g >> (8 - gl)) << o->green_off) | ((b >> (8 - bl)) << o->blue_off);
}

// copia o buffer da UI para o painel e mostra (pan). Suporta 32/24/16 bpp.
static void blit(void)
{
    bd_fb *o = &g_bd_fb;
    if (!o->ready || !back) return;
    int rows = o->h < FH ? o->h : FH, cols = o->w < FW ? o->w : FW;
    for (int y = 0; y < rows; y++) {
        uint8_t *dst = o->mem + (size_t)(y + o->yoff) * (size_t)o->stride + (size_t)o->xoff * (size_t)(o->bpp / 8);
        uint32_t *src = back + (size_t)y * FW;
        if (o->bpp == 32) {
            if (o->red_len == 8 && o->green_len == 8 && o->blue_len == 8 && o->red_off == 16 && o->green_off == 8 && o->blue_off == 0) {
                memcpy(dst, src, (size_t)cols * 4);
            } else if (o->red_len == 8 && o->green_len == 8 && o->blue_len == 8 && o->red_off == 0 && o->green_off == 8 && o->blue_off == 16) {
                /* BGR em memória (é o caso deste aparelho): troca R<->B; sem isso o teal vira amarelo */
                uint32_t *d32 = (uint32_t *)(void *)dst;
                for (int x = 0; x < cols; x++) { uint32_t c = src[x]; d32[x] = ((c & 0xFFu) << 16) | (c & 0xFF00u) | ((c >> 16) & 0xFFu) | 0xFF000000u; }
            } else {
                uint32_t *d32 = (uint32_t *)(void *)dst;
                uint32_t rs = (uint32_t)o->red_off, gs = (uint32_t)o->green_off, bs = (uint32_t)o->blue_off;
                for (int x = 0; x < cols; x++) { uint32_t c = src[x]; d32[x] = (((c >> 16) & 0xFFu) << rs) | (((c >> 8) & 0xFFu) << gs) | ((c & 0xFFu) << bs) | 0xFF000000u; }
            }
        } else if (o->bpp == 24) {
            for (int x = 0; x < cols; x++) { uint32_t c = src[x]; dst[3 * x] = (uint8_t)c; dst[3 * x + 1] = (uint8_t)(c >> 8); dst[3 * x + 2] = (uint8_t)(c >> 16); }
        } else if (o->bpp == 16) {
            uint16_t *d16 = (uint16_t *)(void *)dst;
            for (int x = 0; x < cols; x++) d16[x] = (uint16_t)pack565(src[x], o);
        }
    }
    bd_fb_flush(o);
}

// Barras de cor: a prova de que o painel está vivo e de que os canais estão certos.
static void bars_gfx(Surf s)
{
    static const uint32_t C[8] = { 0xFFFFFF, 0xFF0000, 0x00FF00, 0x0000FF, 0x000000, 0xFFFF00, 0x00FFFF, 0xFF00FF };
    int bw = s.w / 8, y0 = s.h * 35 / 100, hh = s.h * 30 / 100;
    for (int i = 0; i < 8; i++) g_fill(i * bw, y0, bw, hh, C[i]);
    g_fill(0, 0, s.w, 4, 0xFFFFFF);
    g_fill(0, s.h - 4, s.w, 4, 0xFFFFFF);
}

// Faixa de estado permanente (canto inferior esquerdo), desenhada DEPOIS do blit:
// verde = fb+fonte+toque ok · azul = sem toque · âmbar = sem fonte · vermelho = sem tela.
static void status_strip(void)
{
    bd_fb *o = &g_bd_fb;
    if (!o->ready) return;
    uint32_t c = !fb_ok ? 0xFF3030 : !font_ok ? 0xFFB020 : !touch_ok ? 0x3B82F6 : 0x30D060;
    bd_fb_rect(o, 0, o->h - 6, o->w / 5, 6, c);
}

// Tela de diagnóstico: sem UI, sem store. Existe para a FOTO responder tudo.
static void diag_screen(const char *title, const char *info1, const char *info2)
{
    bd_fb *o = &g_bd_fb;
    if (!o->ready) return;

    if (!font_ok) {                       // sem fonte: barras + número do estágio
        bd_fb_bars(o);
        bd_fb_stage(o, stage_now, stage_now >= 90 ? 0xFF3030 : 0x00C0FF);
        bd_fb_flush(o);
        status_strip();
        return;
    }
    Surf s = { back, FW, FH, FW };
    gfx_set(s);
    g_fill(0, 0, FW, FH, 0x07080C);
    bars_gfx(s);
    char l[128];
    g_text_c(FW / 2, FH / 10, 96, "BANKPHONE OS", 0xE8EAF0, 0);
    snprintf(l, sizeof l, "STAGE %d  %s", stage_now, title ? title : "");
    g_text_c(FW / 2, FH / 4, 52, l, stage_now >= 90 ? 0xF87171 : 0x5EEAD4, 0);
    if (info1) g_text(24, FH * 70 / 100 + 12, 30, info1, 0xE8EAF0, 0);
    if (info2) g_text(24, FH * 70 / 100 + 56, 30, info2, 0x7B8194, 0);
    snprintf(l, sizeof l, "%dx%d bpp=%d stride=%d red@%u/%u green@%u/%u blue@%u/%u",
             o->w, o->h, o->bpp, o->stride, o->red_off, o->red_len, o->green_off, o->green_len, o->blue_off, o->blue_len);
    g_text(24, FH - 110, 24, l, 0x7B8194, 0);
    snprintf(l, sizeof l, "font=%s touch=%s serial=%s persist=%s",
             font_ok ? "ok" : "NO", touch_ok ? "ok" : "NO", bd_serial_fd() >= 0 ? "ok" : "NO", plat_persist_ok() ? "ok" : "no (RO)");
    g_text(24, FH - 74, 24, l, 0x7B8194, 0);
    blit();
    status_strip();
}

// Tela de FALHA: continua viva, mostra o código e tenta de novo. Nunca reinicia.
static void fatal_screen(int code, const char *why)
{
    bd_fb *o = &g_bd_fb;
    stage_now = 90;
    bd_log("FAIL code=%d reason=%s", code, why ? why : "");
    bd_flush();
    if (!o->ready) return;
    if (!font_ok) {
        bd_fb_fill(o, 0x300000);
        bd_fb_stage(o, 90, 0xFF3030);
        bd_fb_stage(o, code, 0xFF3030);
        bd_fb_flush(o);
        return;
    }
    Surf s = { back, FW, FH, FW };
    gfx_set(s);
    g_fill(0, 0, FW, FH, 0x220608);
    char l[128];
    snprintf(l, sizeof l, "FAIL %d", code);
    g_text_c(FW / 2, FH / 6, 120, l, 0xF87171, 0);
    g_text_c(FW / 2, FH / 3, 40, why ? why : "", 0xE8EAF0, 0);
    g_text_c(FW / 2, FH / 2, 30, "O sistema continua vivo. Tentando de novo.", 0x7B8194, 0);
    g_text_c(FW / 2, FH * 62 / 100, 26, "Vol- reinicia · Vol+ (3 s) bootloader", 0x7B8194, 0);
    blit();
}

// ------------------------------------------------------------------ entrada
#define BIT(a, b) ((a)[(b) / 8] & (1 << ((b) % 8)))
static int tfd = -1, kfd[4], nk;
static int ax0, ax1, ay0, ay1;                 /* faixa bruta do sensor de toque */
static char tnode[40] = "", tname[64] = "?";   /* o que FOI encontrado (Regra Zero) */
static int t_has_mt, t_has_btn, t_has_mtid;    /* capacidades reais do node         */
static int t_read_err, t_scan_fails, t_kicks, scr_on = 1;
static int set_brightness(int pct);   /* definida mais abaixo; um caminho só de brilho */
static int touchdbg = -1;                      /* bankphone.touchdbg=0 desliga      */
static long long touch_first_ms;
static char fw_ver_now[128] = "(não lido)";
static int t_fwfile_present;

// ------------------------------------------------------------------ toque
// O núcleo (touchcore.h) é testado no Mac com eventos sintéticos: a conversão de
// coordenadas, o protocolo A, o B, o SYN_DROPPED e o recorte de borda não
// dependem de hardware para serem provados. Aqui só ficam as ligações com o
// aparelho: ler o node, mandar para a UI, e CONTAR para o relatório.
static struct { int evt, x, y; } tq[64];   /* fila do touchcore -> UI (um quadro = 1 evento) */
static int tq_n, tq_drop;
static touch_state TS;

static void keys_open(void);              /* definidos mais abaixo */
static long long now_ms(void);

static int touchdbg_on(void)
{
    if (touchdbg < 0) {
        const char *v = plat_boot_prop("bankphone.touchdbg");   /* lido uma única vez */
        touchdbg = (v[0] != '0');                               /* padrão: LIGADO */
    }
    return touchdbg;
}

// O touchcore não chama a UI direto: ele enfileira. Assim a lógica de toque
// continua testável no host e a ordem com as teclas fica previsível.
static void touch_out(void *ctx, int evt, int x, int y)
{
    (void)ctx;
    if (tq_n < 64) { tq[tq_n].evt = evt; tq[tq_n].x = x; tq[tq_n].y = y; tq_n++; }
    else tq_drop++;                            /* visível no heartbeat, nunca silencioso */
}

static void touch_arm(void)
{
    int w = FW > 0 ? FW : 720, h = FH > 0 ? FH : 1612;   /* tela ainda desconhecida? usa o pior caso */
    tc_reset(&TS, ax0, ax1, ay0, ay1, w, h);
    TS.emit = touch_out;
    TS.have_btn = t_has_btn;
    tq_n = 0;
}

// ------------------------------------------------------------------ firmware
// POR QUE ISTO EXISTE (medido no código do driver, não suposto):
//   O NT36528 é da família NT36xxx *flashless*: o firmware NÃO mora no chip.
//   O host baixa ILM/DLM por SPI dentro de nvt_update_firmware() ->
//   request_firmware("novatek_ts_fw.bin") -> nvt_bin_header_parser()
//   (é exatamente essa a linha que aparece no dmesg do Android, 3,1 s depois do probe).
//   Quem DISPARA isso é o notifier de framebuffer do driver:
//     FBIOBLANK/UNBLANK -> FB_EVENT_BLANK -> nvt_fb_notifier_callback ->
//     queue_work(resume_work) -> nvt_ts_resume() -> nvt_update_firmware()
//   E um UNBLANK sozinho NÃO basta: nvt_ts_resume() sai na primeira linha se
//   bTouchIsAwake == 1 (o probe deixa 1). Tem de ser POWERDOWN e depois UNBLANK,
//   senão o download nunca roda e o chip fica mudo — que é o sintoma observado.
//   O driver também cria /proc/nvt_fw_version (0444) e o open faz SPI de verdade:
//   por isso é lido com TIMEOUT, num filho, para o PID 1 nunca pendurar.
static int probe_proc_line(const char *path, char *out, size_t outsz, int ms)
{
    out[0] = 0;
    struct stat st;
    if (stat(path, &st) != 0) return 1;                 /* nem existe no kernel   */
    int pp[2];
    if (pipe(pp) != 0) return -1;
    pid_t pid = fork();
    if (pid == 0) {                                   /* filho: morre sozinho */
        close(pp[0]);
        int f = open(path, O_RDONLY | O_NONBLOCK);
        char b[256]; ssize_t n = -1;
        if (f >= 0) { n = read(f, b, sizeof b - 1); close(f); }
        if (n > 0) {
            ssize_t k = 0;
            while (k < n && b[k] != '\n') k++;
            if (k > 0 && write(pp[1], b, (size_t)k) < 0) {}
        }
        close(pp[1]);
        _exit(n > 0 ? 0 : 2);
    }
    close(pp[1]);
    if (pid < 0) { close(pp[0]); return -1; }
    struct pollfd p = { pp[0], POLLIN, 0 };
    int pr = poll(&p, 1, ms);
    ssize_t n = pr > 0 ? read(pp[0], out, outsz - 1) : -1;
    if (n < 0) n = 0;
    out[n] = 0;
    close(pp[0]);
    int status = 0;
    if (pr <= 0) {
        kill(pid, SIGKILL); waitpid(pid, NULL, 0);
        bd_log("toque: %s NÃO respondeu em %d ms (SPI pendurado?) — leitura abortada sem travar o sistema", path, ms);
        return -2;
    }
    waitpid(pid, &status, 0);
    return n > 0 ? 0 : 2;                               /* 2 = existe, mas vazio  */
}

// O que a imagem precisa ter dentro: o driver pede estes nomes EXATOS.
static void touch_fw_report(const char *tag)
{
    static const char *cand[] = {
        "/vendor/firmware/novatek_ts_fw.bin",      /* nome principal (is_ft_lcm == 0) */
        "/vendor/firmware/novatek_ts_mp.bin",      /* firmware de teste de produção  */
        "/vendor/firmware/novatek_ts_g6_fw.bin",   /* se o painel for "ft"           */
        "/vendor/firmware/novatek_ts_72d_fw.bin",  /* se o painel for 72d            */
        "/lib/firmware/novatek_ts_fw.bin",         /* caminho embutido do firmware_class */
        "/lib/firmware/novatek_ts_mp.bin",
        "/etc/firmware/novatek_ts_fw.bin",
    };
    t_fwfile_present = 0;
    for (size_t i = 0; i < sizeof cand / sizeof cand[0]; i++) {
        struct stat st;
        if (stat(cand[i], &st) != 0) { bd_log("%s: %s AUSENTE", tag, cand[i]); continue; }
        uint8_t h[8] = {0};
        int f = open(cand[i], O_RDONLY);
        if (f >= 0) { if (read(f, h, sizeof h) < 0) {} close(f); }
        bd_log("%s: %s = %lld bytes, começa com %02x %02x %02x %02x", tag, cand[i],
               (long long)st.st_size, h[0], h[1], h[2], h[3]);
        if (strstr(cand[i], "novatek_ts_fw.bin")) t_fwfile_present = 1;   /* o principal do driver */
    }
    /* nós /proc do próprio driver (nomes exatos da família NT36xxx) */
    char l[128];
    int r = probe_proc_line("/proc/nvt_fw_version", l, sizeof l, 1500);
    if (r == 0)      bd_log("%s: /proc/nvt_fw_version = '%s'", tag, l);
    else if (r == 1) bd_log("%s: /proc/nvt_fw_version NÃO EXISTE (o driver de toque não está carregado?)", tag);
    else if (r == 2) bd_log("%s: /proc/nvt_fw_version existe mas não devolveu nada (chip mudo / SPI sem resposta?)", tag);
    else             bd_log("%s: /proc/nvt_fw_version travou a leitura (SPI pendurado)", tag);
    probe_proc_line("/proc/tp_lockdown_info", l, sizeof l, 800);
    if (l[0]) bd_log("%s: /proc/tp_lockdown_info = '%s'", tag, l);

    /* lista (só os nomes) os outros nós do driver que existirem */
    DIR *d = opendir("/proc");
    if (d) {
        struct dirent *e; int n = 0;
        while ((e = readdir(d)) && n < 12) {
            if (strncmp(e->d_name, "nvt_", 4) && strncmp(e->d_name, "tp_", 3)) continue;
            bd_log("%s: /proc/%s presente", tag, e->d_name);
            n++;
        }
        closedir(d);
    }
    bd_flush();
}

// A prova direta do download: o fw_ver lido do chip via /proc.
static void touch_fw_check(const char *tag)
{
    char l[128] = "";
    int r = probe_proc_line("/proc/nvt_fw_version", l, sizeof l, 2000);
    if (r == 0)      snprintf(fw_ver_now, sizeof fw_ver_now, "%s", l);
    else if (r == 1) snprintf(fw_ver_now, sizeof fw_ver_now, "(sem no /proc = driver ausente)");
    else if (r == 2) snprintf(fw_ver_now, sizeof fw_ver_now, "(sem resposta do chip)");
    else             snprintf(fw_ver_now, sizeof fw_ver_now, "(leitura travou)");
    bd_log("%s: fw_ver=%s | eventos=%d syn=%d down=%d move=%d up=%d", tag, fw_ver_now,
           TS.ev, TS.syn, TS.downs, TS.moves, TS.ups);
    bd_flush();
}

// O GATILHO (medido no código do driver, não suposto):
//   FB_EARLY_EVENT_BLANK/POWERDOWN -> nvt_ts_suspend()  -> bTouchIsAwake = 0
//   FB_EVENT_BLANK/UNBLANK         -> resume_work -> nvt_ts_resume()
//   nvt_ts_resume(): if (bTouchIsAwake) return 0;   <-- SAI CEDO
//                    nvt_update_firmware("novatek_ts_fw.bin")  <-- baixa AQUI
// Como o probe termina com bTouchIsAwake = 1, um UNBLANK sozinho NÃO baixa o
// firmware: é preciso POWERDOWN (suspend zera a flag) e depois UNBLANK.
// Sem esse ciclo, o chip fica mudo pelo resto da vida do boot — que é
// exatamente o sintoma "node de toque existe, ZERO eventos".
static int fw_kick(const char *why)
{
    if (!g_bd_fb.ready) {
        bd_log("toque: sem tela aberta — não dá para disparar o kick (%s)", why);
        return -1;
    }
    t_kicks++;
    bd_log("toque: FW-KICK nº %d (%s) — POWERDOWN->UNBLANK para o driver baixar o firmware", t_kicks, why);
    bd_flush();
    bd_fb_blank(&g_bd_fb, FB_BLANK_POWERDOWN);
    usleep(350 * 1000);
    int rc = bd_fb_blank(&g_bd_fb, FB_BLANK_UNBLANK);
    if (rc != 0) bd_log("toque: UNBLANK devolveu %d — insistindo pelo caminho completo de tela", rc);
    bd_fb_power(&g_bd_fb);                   /* rede de segurança: UNBLANK + sysfs + PUT(FORCE) */
    if (scr_on) set_brightness(80);          /* o powerdown mexe no backlight */
    if (back) { Surf s = { back, FW, FH, FW }; gfx_set(s); }
    usleep(900 * 1000);                      /* o download roda numa workqueue do driver */
    touch_fw_check("toque: fw_ver depois do kick");
    return 0;
}

// O printk do próprio driver é a prova do download: [NVT-ts] nvt_bin_header_parser,
// nvt_ts_resume, "download firmware failed", etc. Lido em 3 momentos do boot,
// só as linhas NOVAS — no aparelho não há como rodar dmesg do lado de fora.
static void kmsg_touch_scan_at(void)
{
    static const int marks[] = { 5, 15, 45 };
    static const char *const K[] = { "[NVT", "nvt_", "novatek", "Novatek", "nt36", "NT36",
                                     "NVT-ts", "tpd", "TPD", "firmware", "touch", "Touch" };
    static int i;
    if (i >= 3 || !touch_first_ms) return;
    if ((now_ms() - touch_first_ms) / 1000 < marks[i]) return;
    bd_log("--- dmesg do kernel: linhas de TOQUE/FIRMWARE (t+%d s) ---", marks[i]);
    int n = bd_kmsg_grep("kmsg", K, (int)(sizeof K / sizeof K[0]), 30);
    bd_log("--- fim do dmesg de toque (linhas novas: %d) ---", n);
    bd_flush();
    i++;
}

// Prova viva: uma linha por segundo (só quando algo muda) com o contador de eventos.
static void touch_beat(void)
{
    static int last_ev = -9999; static int sec;
    if (!touchdbg_on()) return;
    if (!touch_ok) {
        if (last_ev != -1) { last_ev = -1; bd_log("toque: SEM node de toque aberto (ver linhas 'input:' acima)"); bd_flush(); }
        return;
    }
    if (TS.ev == last_ev && ++sec < 10) return;
    int mudo = (TS.ev == 0) && (now_ms() - touch_first_ms > 6000);
    bd_log("toque: eventos=%d syn=%d down=%d move=%d up=%d fantasma=%d drop=%d fila=%d node=%s%s",
           TS.ev, TS.syn, TS.downs, TS.moves, TS.ups, TS.ghosts, TS.dropped, tq_drop, tnode,
           mudo ? "  <== ZERO eventos: chip mudo (firmware não baixado — o download roda no resume do driver)" : "");
    bd_flush();
    last_ev = TS.ev; sec = 0;
}

// Caixa TOUCH TEST: o que a FOTO da tela responde sem PC nenhum.
// A caixa TOUCH TEST é ferramenta de desenvolvedor: DESLIGADA por padrão (bankphone.touchbox=1 na cmdline liga).
static int touch_box_on(void)
{
    static int box = -1;
    if (box < 0) box = plat_boot_prop("bankphone.touchbox")[0] == '1';
    return box;
}

static void touch_test_overlay(void)
{
    if (!touch_box_on() || !fb_ok || !font_ok || !back || FW < 400) return;
    Surf s = { back, FW, FH, FW };
    gfx_set(s);
    int w = (FW * 9) / 10, h = 150, x = (FW - w) / 2, y = 14;
    g_rrect(x, y, w, h, 14, 0x0B1220, 215);
    g_rring(x, y, w, h, 14, 2, TS.ev ? 0x30D060 : 0xFFB020);
    char l[220];
    g_text(x + 18, y + 10, 30, "TOUCH TEST", 0xE8EAF0, 0);
    snprintf(l, sizeof l, "%s '%s'  MT=%d btn=%d", touch_ok ? tnode : "SEM NODE", tname, t_has_mt, t_has_btn);
    g_text(x + 18, y + 44, 22, l, 0x7B8194, 0);
    snprintf(l, sizeof l, "ev=%d syn=%d  down=%d move=%d up=%d  fant=%d drop=%d",
             TS.ev, TS.syn, TS.downs, TS.moves, TS.ups, TS.ghosts, TS.dropped);
    g_text(x + 18, y + 72, 22, l, 0x7B8194, 0);
    snprintf(l, sizeof l, "x=%d y=%d %s  kicks=%d  fw=%s", TS.x, TS.y, TS.tracking ? "TOCANDO" : "solto", t_kicks, fw_ver_now);
    g_text(x + 18, y + 100, 22, l, TS.ev ? 0x5EEAD4 : 0xFFB020, 0);
    if (TS.downs || TS.no_down) {                      /* alvo: o dedo é a prova visual */
        g_rring(TS.x - 70, TS.y - 70, 140, 140, 70, 3, TS.tracking ? 0xFF6B6B : 0xFFFFFF);
        g_disc(TS.x, TS.y, TS.tracking ? 26 : 14, TS.tracking ? 0xFF6B6B : 0x3B82F6);
    }
    /* NÃO dá blit: quem apresenta é o ui_present(). Dois caminhos chamando blit()
     * faziam a tela de bloqueio sumir (o segundo desenhava por cima). Um quadro, um blit. */
}

// Redesenho completo: UI -> caixa de toque (opcional) -> UM blit.
// A faixa de vitalidade na borda inferior saiu daqui: ela existia para dar algum
// sinal quando não havia UI; agora a barra de status mostra tudo isso, com números
// reais. A faixa continua na tela de DIAGNÓSTICO (sem UI).
static void ui_present(void)
{
    static long long perf_log_ms; static int n_fr; static long long sum_draw, sum_blit, max_total;
    long long t0 = now_ms();
    ui_draw();
    touch_test_overlay();
    long long t1 = now_ms();
    blit();
    long long t2 = now_ms();
    sum_draw += t1 - t0; sum_blit += t2 - t1; n_fr++;
    if (t2 - t0 > max_total) max_total = t2 - t0;
    if (perf_log_ms == 0) perf_log_ms = t2;
    if (t2 - perf_log_ms >= 5000) {                    /* uma linha a cada 5 s: é a prova de desempenho no aparelho */
        bd_log("perf: %d quadros em %lld ms | media desenho=%lld ms blit=%lld ms | pior quadro=%lld ms",
               n_fr, t2 - perf_log_ms, n_fr ? sum_draw / n_fr : 0, n_fr ? sum_blit / n_fr : 0, max_total);
        perf_log_ms = t2; n_fr = 0; sum_draw = sum_blit = max_total = 0;
    }
}

// Mapa de capacidades de TODOS os nodes. É o que permite dizer "quem é quem" no
// relatório sem supor nada (Regra Zero: event0/1/2/3 não se adivinha).
static void input_nodes_report(void)
{
    for (int i = 0; i < 32; i++) {
        char p[40]; snprintf(p, sizeof p, "/dev/input/event%d", i);
        int fd = open(p, O_RDONLY | O_NONBLOCK); if (fd < 0) continue;
        uint8_t ev[8] = {0};
        if (ioctl(fd, EVIOCGBIT(0, sizeof ev), ev) < 0) { close(fd); continue; }
        char name[64] = "?";
        ioctl(fd, EVIOCGNAME(sizeof name - 1), name);
        unsigned long prop = 0;
        ioctl(fd, EVIOCGPROP(sizeof prop), &prop);
        bd_log("input: %s '%s' direct=%d abs=%d key=%d rel=%d", p, name,
               (int)((prop & (1UL << INPUT_PROP_DIRECT)) != 0),
               BIT(ev, EV_ABS) ? 1 : 0, BIT(ev, EV_KEY) ? 1 : 0, BIT(ev, EV_REL) ? 1 : 0);
        if (BIT(ev, EV_ABS)) {
            uint8_t ab[ABS_MAX / 8 + 1] = {0}, ky[KEY_MAX / 8 + 1] = {0};
            if (BIT(ev, EV_KEY)) ioctl(fd, EVIOCGBIT(EV_KEY, sizeof ky), ky);
            if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof ab), ab) == 0)
                bd_log("       abs: MT_X=%d MT_Y=%d MT_ID=%d X=%d Y=%d BTN_TOUCH=%d",
                       BIT(ab, ABS_MT_POSITION_X) ? 1 : 0, BIT(ab, ABS_MT_POSITION_Y) ? 1 : 0,
                       BIT(ab, ABS_MT_TRACKING_ID) ? 1 : 0, BIT(ab, ABS_X) ? 1 : 0,
                       BIT(ab, ABS_Y) ? 1 : 0, BIT(ky, BTN_TOUCH) ? 1 : 0);
        }
        close(fd);
    }
    bd_flush();
}

// Escolha do node de toque por PONTUAÇÃO de capacidade: painel direto + multi-toque
// ganha; BTN_TOUCH com ABS_X/Y serve de reserva; acelerômetro (ABS_X/Y sem BTN_TOUCH)
// é descartado de propósito.
static int touch_open_once(void)
{
    int best_fd = -1, best_score = 0, best_i = -1, best_mt = 0, best_btn = 0, best_mtid = 0;
    int bx0 = 0, bx1 = 0, by0 = 0, by1 = 0;
    char bname[64] = "?";

    for (int i = 0; i < 32; i++) {
        char p[40]; snprintf(p, sizeof p, "/dev/input/event%d", i);
        int fd = open(p, O_RDONLY | O_NONBLOCK); if (fd < 0) continue;

        uint8_t ev[8] = {0}, ab[ABS_MAX / 8 + 1] = {0}, ky[KEY_MAX / 8 + 1] = {0};
        char name[64] = "?";
        ioctl(fd, EVIOCGNAME(sizeof name - 1), name);
        if (ioctl(fd, EVIOCGBIT(0, sizeof ev), ev) < 0 || !BIT(ev, EV_ABS)) { close(fd); continue; }
        ioctl(fd, EVIOCGBIT(EV_ABS, sizeof ab), ab);
        if (BIT(ev, EV_KEY)) ioctl(fd, EVIOCGBIT(EV_KEY, sizeof ky), ky);

        int mtid = BIT(ab, ABS_MT_TRACKING_ID) ? 1 : 0;
        int mt = (BIT(ab, ABS_MT_POSITION_X) && BIT(ab, ABS_MT_POSITION_Y)) ? 1 : 0;
        int xy = (BIT(ab, ABS_X) && BIT(ab, ABS_Y)) ? 1 : 0;
        int btn = BIT(ky, BTN_TOUCH) ? 1 : 0;
        if (!mt && !(xy && btn)) { close(fd); continue; }         /* não é toque de tela */

        unsigned long prop = 0;
        ioctl(fd, EVIOCGPROP(sizeof prop), &prop);
        int score = (mt ? 2 : 0) + (xy ? 1 : 0) + (btn ? 1 : 0) + ((prop & (1UL << INPUT_PROP_DIRECT)) ? 4 : 0);
        if (score <= best_score) { close(fd); continue; }

        struct input_absinfo a;
        int nx0 = 0, nx1 = 0, ny0 = 0, ny1 = 0;
        if (mt) {
            ioctl(fd, EVIOCGABS(ABS_MT_POSITION_X), &a); nx0 = a.minimum; nx1 = a.maximum;
            ioctl(fd, EVIOCGABS(ABS_MT_POSITION_Y), &a); ny0 = a.minimum; ny1 = a.maximum;
        } else {
            ioctl(fd, EVIOCGABS(ABS_X), &a); nx0 = a.minimum; nx1 = a.maximum;
            ioctl(fd, EVIOCGABS(ABS_Y), &a); ny0 = a.minimum; ny1 = a.maximum;
        }
        bd_log("toque: candidato %s '%s' direct=%d MT=%d MT_ID=%d ABS_XY=%d BTN_TOUCH=%d x[%d,%d] y[%d,%d] pontos=%d",
               p, name, (int)((prop & (1UL << INPUT_PROP_DIRECT)) != 0), mt, mtid, xy, btn,
               nx0, nx1, ny0, ny1, score);
        if (best_fd >= 0) close(best_fd);
        best_fd = fd; best_score = score; best_i = i; best_mt = mt; best_btn = btn;
        best_mtid = mtid;
        bx0 = nx0; bx1 = nx1; by0 = ny0; by1 = ny1;
        snprintf(bname, sizeof bname, "%s", name);
    }

    if (best_fd < 0) {
        t_scan_fails++;
        if (t_scan_fails <= 3 || (t_scan_fails % 40) == 0)   /* sem metralhadora de log */
            bd_log("toque: NENHUM node com eixos de toque (varredura %d) — o driver não registrou input", t_scan_fails);
        bd_flush();
        return 0;
    }
    t_scan_fails = 0;
    tfd = best_fd; ax0 = bx0; ax1 = bx1; ay0 = by0; ay1 = by1;
    t_has_mt = best_mt; t_has_btn = best_btn; t_has_mtid = best_mtid;
    snprintf(tnode, sizeof tnode, "/dev/input/event%d", best_i);
    snprintf(tname, sizeof tname, "%s", bname);
    touch_ok = 1;
    bd_log("toque: USANDO %s '%s' x[%d,%d] y[%d,%d] MT=%d BTN_TOUCH=%d", tnode, tname, ax0, ax1, ay0, ay1, t_has_mt, t_has_btn);
    bd_flush();
    return 1;
}

static void touch_rescan(const char *why)
{
    if (tfd >= 0) { close(tfd); tfd = -1; }
    touch_ok = 0;
    bd_log("toque: reabrindo o node (%s)", why);
    if (touch_open_once()) touch_arm();
}

// Entrada em uma tacada: mapa dos nodes -> teclas -> toque -> diagnóstico de firmware.
static void input_start(void)
{
    input_nodes_report();
    keys_open();
    if (touch_open_once()) touch_arm();
    /* O init da entrada é quem sabe o que existe: publica isso no HardwareService,
     * que passa a ser a fonte única para a tela (Regra Zero: nada presumido). */
    hw_note_touch(tnode, tname, touch_ok);
    {
        char kd[128] = "";
        for (int i = 0; i < nk; i++) snprintf(kd + strlen(kd), sizeof kd - strlen(kd), "%skfd%d", i ? "," : "", i);
        hw_note_keys(nk, kd);
    }
    touch_first_ms = now_ms();
    touch_fw_report("fw");

    // Ciclo fechar/reabrir o node: o driver NÃO tem gancho em input_dev->open/close
    // (verificado no código do driver), então isto NÃO dispara nada. Fica aqui só
    // como prova registrada e para o caso de uma variante do driver ter o gancho.
    if (touch_ok) {
        bd_log("toque: ciclo open/close/reopen do node (inócuo neste driver: sem input_dev->open)");
        close(tfd); tfd = -1; touch_ok = 0;
        usleep(150 * 1000);
        if (touch_open_once()) touch_arm();
    }

    // O kick: é ELE que faz o driver pedir o firmware. Sem isso o chip fica mudo.
    if (touch_ok) {
        if (plat_boot_prop("bankphone.fwkick")[0] != '0') fw_kick("primeiro boot");
        else bd_log("toque: bankphone.fwkick=0 — kick desligado; o firmware só virá se o driver agendar sozinho");
        touch_fw_check("toque: fw_ver após o boot");
    } else {
        bd_log("toque: sem node — o kick exige o driver probeado");
    }
    if (touch_ok && !t_fwfile_present) {
        bd_log("toque: novatek_ts_fw.bin NÃO está no rootfs. O NT36528 é flashless:");
        bd_log("toque: sem esse arquivo o chip NUNCA gera interrupção (toque morto).");
    }
}

static void keys_open(void)
{
    nk = 0;
    for (int i = 0; i < 32 && nk < 4; i++) {
        char p[40]; snprintf(p, sizeof p, "/dev/input/event%d", i);
        int fd = open(p, O_RDONLY | O_NONBLOCK); if (fd < 0) continue;
        uint8_t ev[8] = {0}, key[KEY_MAX / 8 + 1] = {0}; char name[64] = "?";
        ioctl(fd, EVIOCGNAME(sizeof name - 1), name);
        if (ioctl(fd, EVIOCGBIT(0, sizeof ev), ev) < 0 || !BIT(ev, EV_KEY)) { close(fd); continue; }
        ioctl(fd, EVIOCGBIT(EV_KEY, sizeof key), key);
        if (BIT(key, KEY_VOLUMEUP) || BIT(key, KEY_VOLUMEDOWN) || BIT(key, KEY_POWER)) {
            kfd[nk++] = fd;
            bd_log("teclas: %s (%s)", p, name);
        } else close(fd);
    }
    if (!nk) bd_log("teclas: nenhum node com KEY_VOLUMEUP/DOWN/POWER");
    bd_flush();
}


// ------------------------------------------------------------------ desempenho do processador
// No boot o kernel deixa a CPU em frequência baixa/poucos núcleos; a UI por software sentia isso como "1 fps".
// Sobe tudo para o máximo e LÊ DE VOLTA o que o kernel aceitou (nada presumido).
static void cpu_write(const char *path, const char *v)
{
    int f = open(path, O_WRONLY); if (f < 0) return;
    if (write(f, v, strlen(v)) < 0) {}
    close(f);
}
static void cpu_read(const char *path, char *out, size_t n)
{
    out[0] = 0; FILE *f = fopen(path, "r"); if (!f) { snprintf(out, n, "?"); return; }
    if (!fgets(out, (int)n, f)) out[0] = 0; fclose(f);
    out[strcspn(out, "\n")] = 0;
}
static void cpu_boost(void)
{
    if (plat_boot_prop("bankphone.cpuboost")[0] == '0') { bd_log("cpu: boost desligado (bankphone.cpuboost=0)"); return; }
    char p[160], mx[48], g[48], mn[48], cur[48], on[48];
    for (int i = 1; i < 8; i++) { snprintf(p, sizeof p, "/sys/devices/system/cpu/cpu%d/online", i); cpu_write(p, "1"); }
    DIR *d = opendir("/sys/devices/system/cpu/cpufreq");
    if (!d) { bd_log("cpu: /sys/devices/system/cpu/cpufreq AUSENTE — sem controle de frequência"); return; }
    struct dirent *e;
    while ((e = readdir(d))) {
        if (strncmp(e->d_name, "policy", 6)) continue;
        snprintf(p, sizeof p, "/sys/devices/system/cpu/cpufreq/%s/scaling_governor", e->d_name); cpu_write(p, "performance");
        snprintf(p, sizeof p, "/sys/devices/system/cpu/cpufreq/%s/cpuinfo_max_freq", e->d_name); cpu_read(p, mx, sizeof mx);
        if (mx[0] && mx[0] != '?') { snprintf(p, sizeof p, "/sys/devices/system/cpu/cpufreq/%s/scaling_min_freq", e->d_name); cpu_write(p, mx); }
        snprintf(p, sizeof p, "/sys/devices/system/cpu/cpufreq/%s/scaling_governor", e->d_name); cpu_read(p, g, sizeof g);
        snprintf(p, sizeof p, "/sys/devices/system/cpu/cpufreq/%s/scaling_min_freq", e->d_name); cpu_read(p, mn, sizeof mn);
        snprintf(p, sizeof p, "/sys/devices/system/cpu/cpufreq/%s/scaling_cur_freq", e->d_name); cpu_read(p, cur, sizeof cur);
        bd_log("cpu: %s governor=%s min=%s max=%s cur=%s kHz", e->d_name, g, mn, mx, cur);
    }
    closedir(d);
    cpu_read("/sys/devices/system/cpu/online", on, sizeof on);
    bd_log("cpu: núcleos online = %s", on);
}

// ------------------------------------------------------------------ pstore
// O console do kernel sobrevive a panic/reboot (ramoops). É a única janela
// para "por que o boot anterior morreu" — lida ANTES de qualquer outra coisa.
static char pstore_head[3][96];

static void pstore_dump(void)
{
    DIR *d = opendir("/sys/fs/pstore");
    if (!d) { bd_log("pstore: /sys/fs/pstore AUSENTE (ramoops não montado?)"); return; }
    struct dirent *e; int n = 0;
    while ((e = readdir(d)) && n < 4) {
        if (e->d_name[0] == '.') continue;
        char p[300]; snprintf(p, sizeof p, "/sys/fs/pstore/%.200s", e->d_name);
        FILE *f = fopen(p, "r");
        if (!f) { bd_log("pstore: %s não abriu", e->d_name); continue; }
        bd_log("pstore: ===== %s =====", e->d_name);
        char l[512]; int lines = 0;
        while (lines < 400 && fgets(l, sizeof l, f)) {
            l[strcspn(l, "\n")] = 0;
            if (l[0]) bd_log("pstore| %s", l);
            if (n == 0 && lines < 3) snprintf(pstore_head[lines], sizeof pstore_head[0], "%s", l);
            lines++;
        }
        if (lines >= 400) bd_log("pstore: (truncado em 400 linhas)");
        fclose(f);
        n++;
    }
    closedir(d);
    if (!n) bd_log("pstore: vazio (nenhum panic do boot anterior registrado)");
}

// ------------------------------------------------------------------ teclas
#define HOLD_MS 3000
static long long now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (long long)t.tv_sec * 1000 + t.tv_nsec / 1000000; }

static int use_bcb(void)
{
    /* Nesta tabela de partições NÃO existe 'misc' (B3): o BCB clássico do AOSP
     * não tem onde morar — os candidatos MTK seriam 'para'/'boot_para', que não
     * são tocados sem evidência. Então o padrão é RESTART2 puro. */
    if (plat_boot_prop("bankphone.ro")[0] == '1') return 0;
#ifdef BANKPHONE_BCB_REBOOT
    return 1;
#else
    return access("/tmp/bcb-reboot", F_OK) == 0;   /* opt-in explícito */
#endif
}

static void key_action(int code)
{
    switch (code) {
    case KEY_VOLUMEUP: {
        const char *arg = plat_boot_prop("bankphone.fastboot");
        if (!arg[0]) arg = "bootloader";                   /* troque para "fastboot" pela cmdline se preciso */
        bd_log("Vol+ (3 s): reiniciando no bootloader (argumento '%s')", arg);
        set_brightness(0);
        if (use_bcb()) bd_reboot_target(arg);              /* RESTART2 (+ BCB, se habilitado) */
        else bd_reboot_restart2(arg);                      /* padrão: NÃO grava partição */
        break; }
    case KEY_VOLUMEDOWN:
        bd_log("Vol-: reiniciando no sistema (escape imediato, sem gravar partição)");
        bd_reboot_restart2(NULL);
        break;
    default: break;
    }
}

// Comando pela USB (só imagens de teste): o instalador manda uma linha pela serial
// e o aparelho reinicia sozinho no bootloader. Exige bankphone.devcmd=1 na cmdline.
static void devcmd_poll(void)
{
    static int on = -1;
    static DevCmd dc;
    if (on < 0) {
        on = plat_boot_prop("bankphone.devcmd")[0] == '1';
        bd_log("devcmd: %s", on ? "ativo (imagem de teste): aceito REBOOT-BOOTLOADER pela serial" : "desligado");
    }
    if (!on) return;
    int fd = bd_serial_fd();
    if (fd < 0) return;
    char b[64];
    ssize_t n = read(fd, b, sizeof b);
    if (n <= 0) return;
    if (devcmd_feed(&dc, b, (int)n) == DEVCMD_REBOOT_BOOTLOADER) {
        const char *arg = plat_boot_prop("bankphone.fastboot");
        if (!arg[0]) arg = "bootloader";
        bd_log("devcmd: reiniciando no bootloader a pedido da serial (argumento '%s')", arg);
        bd_reboot_restart2(arg);                           /* NÃO grava partição */
    }
}

// Brilho: UM caminho só. Depois da descoberta (F1), quem manda é o HardwareService,
// que escreve SÓ no nó de backlight encontrado e LÊ de volta. O bd_backlight fica
// como rede de segurança para o instante anterior à descoberta.
static int set_brightness(int pct)
{
    if (hw_brightness_set(pct) == 0) return 0;
    return bd_backlight(pct);
}

// ------------------------------------------------------------------ plataforma (ui.h)
static int64_t boot_mono(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec; }
int plat_clock_valid(void) { return time(NULL) > 1700000000; }
int64_t plat_now(void) { return plat_clock_valid() ? (int64_t)time(NULL) : boot_mono(); }
void plat_random(uint8_t *b, int n) { int f = open("/dev/urandom", O_RDONLY); if (f >= 0) { if (read(f, b, n) < 0) {} close(f); } else for (int i = 0; i < n; i++) b[i] = (uint8_t)(boot_mono() * 31 + i * 17); }
static int persist = 0;
int plat_persist_ok(void) { return persist; }
void plat_save(void)
{
    if (!persist) return;
    static char buf[200000]; size_t o = pin_serialize(buf, sizeof buf); o += m_serialize(buf + o, sizeof buf - o);
    int r = store_save(buf, o);
    if (r) { bd_log("store: gravação falhou (%d) — %s", r, ST.last_msg); bd_flush(); }
}
const char *plat_boot_prop(const char *key)
{
    static char v[64]; v[0] = 0; static char cl[4096]; static int got;
    if (!got) { int f = open("/proc/cmdline", O_RDONLY); if (f >= 0) { ssize_t n = read(f, cl, sizeof cl - 1); cl[n > 0 ? n : 0] = 0; close(f); } got = 1; }
    char *p = strstr(cl, key); size_t kl = strlen(key);
    if (p && p[kl] == '=') { p += kl + 1; size_t i = 0; while (*p && *p != ' ' && *p != '\n' && i < sizeof v - 1) v[i++] = *p++; v[i] = 0; }
    return v;
}

/* O store não conhece o bd_log: quem usa é que aponta o destino dos avisos. */
static void store_log_bridge(const char *msg) { bd_log("store: %s", msg); }

/* Persistência (F2): a AREA é escolhida na cmdline, nunca no código.
 *   bankphone.state=<partição>        ex.: bankphone.state=expdb
 *   bankphone.statehash=<16 hex>      hash da região TAL COMO ESTAVA NO BACKUP
 * Sem o hash, a TOMADA da área é recusada e o sistema roda só em memória —
 * e a tela de diagnóstico diz exatamente isso. */
static void load_state(void)
{
    static char buf[200000]; size_t n = 0;
    make_nodes();
    store_set_log(store_log_bridge);

    const char *ro    = plat_boot_prop("bankphone.ro");
    const char *spec  = plat_boot_prop("bankphone.state");
    const char *hash  = plat_boot_prop("bankphone.statehash");

    if (ro[0] == '1') {
        bd_log("store: SOMENTE LEITURA (cmdline bankphone.ro=1) — nenhuma partição será gravada%s",
               spec[0] ? ", mesmo com bankphone.state na cmdline" : "");
        bd_flush();
        return;
    }
    if (!spec[0]) {
        bd_log("store: sem persistência — falta 'bankphone.state=<partição>' na cmdline "
               "(o estado vive só na memória; é o modo seguro de teste)");
        bd_flush();
        return;
    }
    store_config(spec, hash);
    if (store_open() != 0) {
        bd_log("store: NÃO armado — %s", ST.why);
        bd_flush();
        return;
    }
    persist = 1;
    int r = store_load(buf, sizeof buf - 1, &n);
    if (r == 0) {
        buf[n] = 0; char *nl = strchr(buf, '\n');
        if (buf[0] == 'P' && nl) { *nl = 0; pin_deserialize(buf); m_deserialize(nl + 1); }
        bd_log("store: carregado (pin=%d tx=%d, %u bytes, slot %s%s)",
               PIN.set, M.n, (unsigned)n,
               ST.seq[0] >= ST.seq[1] ? "A" : "B", ST.recovered ? ", RECUPERADO do outro slot" : "");
    } else if (r == 1) {
        bd_log("store: área sem estado ainda (primeiro boot com esta região)");
    } else {
        bd_log("store: estado ilegível (r=%d) — seguindo com estado novo; a área segue armada", r);
    }
    if (store_heal() == 0) bd_log("store: cura aplicada (slot estragado/PENDENTE virou vazio)");
    bd_flush();
}

// ------------------------------------------------------------------ crash
static void crash(int sig, siginfo_t *si, void *ctx)
{
    (void)ctx;
    char b[160];
    snprintf(b, sizeof b, "\nBANKPHONE FATAL reason=crash signal=%d addr=%p\n", sig, si ? si->si_addr : (void *)0);
    bd_log_raw(b);
    bd_flush();                       // relatório persistente antes de qualquer coisa
    if (g_bd_fb.ready) {              // aviso na tela, sem depender de nada
        bd_fb_fill(&g_bd_fb, 0x300000);
        bd_fb_stage(&g_bd_fb, 91, 0xFF3030);
        bd_fb_flush(&g_bd_fb);
    }
    for (;;) pause();                 // PID 1 não morre; a serial continua legível
}

// ------------------------------------------------------------------ main
int main(void)
{
    // 0. sobreviver a qualquer coisa antes de qualquer log
    struct sigaction sa; memset(&sa, 0, sizeof sa); sa.sa_sigaction = crash; sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, NULL); sigaction(SIGBUS, &sa, NULL); sigaction(SIGILL, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL); sigaction(SIGABRT, &sa, NULL);

    // 1. /dev, /proc, /sys ANTES de qualquer open
    mkdir("/dev", 0755); mkdir("/proc", 0755); mkdir("/sys", 0755); mkdir("/tmp", 0755);
    mount("devtmpfs", "/dev", "devtmpfs", 0, "mode=0755");
    mount("proc", "/proc", "proc", 0, NULL);
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    mount("pstore", "/sys/fs/pstore", "pstore", 0, NULL);   /* ramoops: console do boot anterior */
    make_nodes();

    // 2. observabilidade mínima
    bd_init();
    stage(ST_KMSG);
    cpu_boost();

    // 3. dmesg do kernel + pstore do boot ANTERIOR + relatório durável
    //    Com bankphone.ro=1 NADA é gravado: sem relatório, sem store, sem BCB.
    boot_epoch = time(NULL);
    hw_last_report = boot_epoch;      /* o 1º despejo da tabela de hardware é 60 s depois */
    int ro = plat_boot_prop("bankphone.ro")[0] == '1';
    if (ro) bd_log("MODO SOMENTE LEITURA (bankphone.ro=1): nenhuma partição será gravada");
    bd_capture_kernel_log();
    stage(ST_DMESG);
    if (ro) bd_log("report: desativado pelo modo somente-leitura (o log fica na RAM e vai para a serial)");
    else if (bd_report_open(NULL) != 0) bd_log("diag: seguindo sem partição de relatório");
    else bd_flush();
    stage(ST_REPORT);
    pstore_dump();
    stage(ST_PSTORE);

    // 4. inventário: é ele que decide o resto (fb? drm? backlight? udc? input?)
    bd_dump_inventory();
    bd_watchdog_probe();
    stage(ST_INVENTORY);

    // 4.5 HARDWARE: a descoberta não depende da tela. Se a tela falhar, o serviço
    //     de hardware continua valendo e aparece no diagnóstico (serial e tela).
    //     (Sem estágio novo: a numeração de estágios é do bootdiag e não muda aqui.)
    hw_init();

    // 5. USB: gadget primeiro, serial depois (o UDC precisa estar vinculado antes do ttyGS0)
    usb_serial();
    stage(ST_USB);
    if (bd_serial_bind() == 0) stage(ST_SERIAL);
    else bd_log("serial: indisponível — o relatório persistente continua sendo gravado");

    // 6. TELA: abrir, LIGAR, e provar com barras — antes de fonte, UI e store
    int rc = bd_fb_open(&g_bd_fb);
    if (rc != 0) {
        bd_fail(-rc, "framebuffer indisponível");
        fb_ok = 0;
    } else {
        fb_ok = 1; FW = g_bd_fb.w; FH = g_bd_fb.h;
        back = calloc((size_t)FW * FH, 4);
        if (!back) { fb_ok = 0; fatal_screen(9, "sem memoria para o buffer de tela"); }
        stage(ST_FB_OPEN);
        if (bd_fb_power(&g_bd_fb) != 0) bd_fail(BD_F_FB_POWER, "unblank/put falhou (seguindo viva)");
        stage(ST_FB_POWER);

        // PROVA DE DISPLAY: barras de cor, sem fonte e sem UI
        if (back) { Surf s = { back, FW, FH, FW }; gfx_set(s); g_fill(0, 0, FW, FH, 0x000000); bars_gfx(s); blit(); }
        /* o framebuffer tem tamanho/bpp/stride REAIS: publica no serviço de hardware */
        hw_set_display(FW, FH, g_bd_fb.bpp, g_bd_fb.stride, g_bd_fb.node);
        stage(ST_BARS);
        bd_log("display: barras desenhadas — se a FOTO não mostrar cores, o problema é painel/backlight, não a UI");
    }

    // 7. backlight DEPOIS do painel ligado; respeita max_brightness e lê de volta
    if (fb_ok) {
        if (set_brightness(80) == 0) bd_fail(BD_F_BACKLIGHT, "nenhum node de backlight");
        stage(ST_BACKLIGHT);
    }

    // 8. fonte (assets) — o desenho da UI NUNCA pode depender disto para existir
    unsigned char *ttf = NULL; long ttf_len = 0;
    {
        FILE *f = fopen("/assets/Roboto-Regular.ttf", "rb");
        if (f) { fseek(f, 0, SEEK_END); ttf_len = ftell(f); rewind(f); ttf = malloc((size_t)ttf_len); if (ttf && fread(ttf, 1, (size_t)ttf_len, f) != (size_t)ttf_len) { free(ttf); ttf = NULL; } fclose(f); }
        if (!ttf) bd_log("FONTE: /assets/Roboto-Regular.ttf ausente ou ilegível (%ld bytes)", ttf_len);
    }
    font_ok = ttf && gfx_init(ttf) == 0;
    if (!font_ok) {
        bd_log("FONTE falhou: UI em modo diagnóstico (barras + estágio), sem reiniciar");
        bd_fail(ttf ? F_FONT : F_ASSETS, ttf ? "gfx_init falhou (TTF inválido?)" : "assets/Roboto-Regular.ttf ausente no ramdisk");
    }
    stage(ST_FONT);

    // 9. UI + estado + entrada
    if (fb_ok && font_ok) {
        ui_init(FW, FH);
        hw_set_display(FW, FH, g_bd_fb.bpp, g_bd_fb.stride, g_bd_fb.node);
        load_state();
        input_start();
        ui_live = 1;
        stage(ST_UI);
        bd_log("UI ativa. Vol+ (3 s) = bootloader · Vol- = reiniciar · Power = tela");
    } else {
        safe_mode = 1;
        load_state();
        input_start();
        bd_log("MODO DIAGNOSTICO: sem UI (%s). O sistema fica vivo e tenta de novo.",
               !fb_ok ? "sem tela" : "sem fonte");
        diag_screen("DIAGNOSTICO", !fb_ok ? "SEM TELA (fb_open falhou)" : "SEM FONTE (assets)",
                    "O sistema continua vivo e tentando de novo.");
    }
    stage(ST_INPUT);

    // 10. loop principal
    unsigned long frames = 0;
    stage(ST_LOOP);
    int on = 1, dirty = 1;
    time_t last = 0, retry_t = time(NULL);
    int kb_hold = -1; long long kb_down = 0;

    for (;;) {
        struct pollfd pf[5]; int np = 0;
        if (tfd >= 0) pf[np++] = (struct pollfd){ tfd, POLLIN, 0 };
        for (int i = 0; i < nk; i++) pf[np++] = (struct pollfd){ kfd[i], POLLIN, 0 };
        poll(pf, np, 100);            /* 100 ms: o protocolo A precisa de um relógio para o UP */
        devcmd_poll();                /* comando do instalador pela USB (só se bankphone.devcmd=1) */

        tc_now(&TS, now_ms());
        if (tc_tick(&TS, 250))        /* silêncio no protocolo A = dedo solto */
            if (touchdbg_on()) bd_log("TOUCH UP (por silêncio de 250 ms — protocolo A)");

        for (int i = 0; i < np; i++) {
            if (!(pf[i].revents & POLLIN)) continue;
            struct input_event e;
            ssize_t r;
            while ((r = read(pf[i].fd, &e, sizeof e)) == (ssize_t)sizeof e) {
                if (pf[i].fd == tfd) {                        // ---------------- toque
                    tc_event(&TS, &e);                        /* núcleo testado no host */
                } else if (e.type == EV_KEY) {                // ---------------- teclas
                    if (e.value == 1) {
                        if (e.code == KEY_VOLUMEDOWN) { key_action(KEY_VOLUMEDOWN); continue; }  // escape imediato
                        kb_hold = e.code; kb_down = now_ms();
                    } else if (e.value == 0 && kb_hold == e.code) {
                        long long held = now_ms() - kb_down; kb_hold = -1;
                        if (held < HOLD_MS) {
                            bd_log("tecla %d: %.1f s (<3 s) — ignorada", e.code, held / 1000.0);
                            if (e.code == KEY_POWER) {                 // toque curto: só a tela
                                on = !on; bd_backlight(on ? 80 : 0);
                                if (!on) ui_lock();
                                dirty = 1;
                            }
                            continue;
                        }
                        if (e.code == KEY_POWER) { on = !on; bd_backlight(on ? 80 : 0); if (!on) ui_lock(); dirty = 1; continue; }
                        key_action(e.code);                        // Vol+ (3 s) => bootloader
                    }
                }
            }
            if (pf[i].fd == tfd && r < 0 && errno != EAGAIN && errno != EINTR) {
                t_read_err++;
                bd_log("toque: read() falhou (errno=%d, %dª vez) — o driver pode ter re-registrado o node", errno, t_read_err);
                if (t_read_err == 1 || (t_read_err % 8) == 0) { touch_rescan("read falhou"); dirty = 1; }
            }
        }

        // Drena o que o touchcore produziu: prova na serial + entrega para a UI.
        // MOVE é limitado a 5 linhas/s para o relatório não virar metralhadora.
        scr_on = on;
        for (int q = 0; q < tq_n; q++) {
            static const char *TN[3] = { "DOWN", "MOVE", "UP" };
            static long long last_move_log;
            int evt = tq[q].evt;
            if (touchdbg_on() && (evt != 1 || now_ms() - last_move_log > 200)) {
                bd_log("TOUCH %s x=%d y=%d (evento %d no node %s)", TN[evt], tq[q].x, tq[q].y, TS.ev, tnode);
                bd_flush();
                if (evt == 1) last_move_log = now_ms();
            }
            if (on && ui_live) dirty |= ui_touch(evt, tq[q].x, tq[q].y);
            if (touch_box_on()) dirty = 1;             /* a caixa TOUCH TEST segue o dedo */
        }
        tq_n = 0;

        time_t t = time(NULL);
        if (t != last) {
            last = t;
            touch_beat();                       /* prova viva: contador de eventos na serial */
            kmsg_touch_scan_at();               /* printk do driver: a prova do firmware */
            dirty |= hw_tick(now_ms());         /* bateria/carregador/brilho/rede: 1x/s */
            {   /* contadores vivos para a tela de diagnóstico (F0) */
                UiDev d;
                memset(&d, 0, sizeof d);
                d.touch_ok = touch_ok; d.touch_ev = TS.ev;
                d.touch_down = TS.downs; d.touch_move = TS.moves; d.touch_up = TS.ups;
                d.touch_dropped = tq_drop; d.touch_kicks = t_kicks;
                snprintf(d.touch_node, sizeof d.touch_node, "%s", tnode);
                snprintf(d.touch_name, sizeof d.touch_name, "%s", tname);
                snprintf(d.fw_ver, sizeof d.fw_ver, "%s", fw_ver_now[0] ? fw_ver_now : "(não lido)");
                d.uptime_s = (long long)t - (long long)boot_epoch;
                d.frames = frames; d.persist_ok = persist;
                d.ro = plat_boot_prop("bankphone.ro")[0] == '1';
                d.safe_mode = safe_mode;
                d.serial_ok = bd_serial_fd() >= 0;
                ui_set_dev(&d);
            }
            /* a cada 60 s, despeja a tabela de hardware no log: é a prova do F1 */
            if (t - hw_last_report >= 60) { hw_last_report = t; hw_report(); }
            if (ui_live && on) dirty |= ui_tick();
            if (ui_live && on && dirty) { ui_present(); dirty = 0; }
            else if (!ui_live) diag_screen(safe_mode ? "DIAGNOSTICO" : "INICIANDO", "aguarde / veja o relatório", pstore_head[0][0] ? pstore_head[0] : NULL);
            bd_serial_pump();
            bd_note_written_frames(++frames);
        }

        // retentativas: nada de desistir, nada de reiniciar
        if (t - retry_t >= 3) {
            retry_t = t;
            if (!fb_ok) {
                if (bd_fb_open(&g_bd_fb) == 0) {
                    fb_ok = 1; FW = g_bd_fb.w; FH = g_bd_fb.h;
                    if (!back) back = calloc((size_t)FW * FH, 4);
                    if (back) { bd_fb_power(&g_bd_fb); Surf s = { back, FW, FH, FW }; gfx_set(s); g_fill(0, 0, FW, FH, 0); bars_gfx(s); blit(); }
                    bd_log("tela apareceu (retry)");
                }
            } else if (!font_ok && ttf && (font_ok = gfx_init(ttf) == 0)) {
                bd_log("fonte carregou (retry)");
            }
            if (!ui_live && fb_ok && font_ok) {
                ui_init(FW, FH); load_state(); if (!nk) keys_open();
                hw_note_touch(tnode, tname, touch_ok);
                ui_live = 1; safe_mode = 0; dirty = 1;
                bd_log("UI entrou em operação");
            }
            if (!touch_ok && tfd < 0) { if (touch_open_once()) { touch_arm(); dirty = 1; } }
            // AUTOCURA DO TOQUE: node aberto, arquivo de firmware presente e ZERO eventos
            // depois de 8 s => o chip continua sem firmware. Refaz o único gatilho que
            // existe (POWERDOWN->UNBLANK) e reconfere o fw_ver. No máximo 3 ciclos.
            if (touch_ok && t_fwfile_present && TS.ev == 0 && t_kicks < 3 && (now_ms() - touch_first_ms) > 20000 && on) {
                bd_log("toque: ZERO eventos 20 s depois do node aberto — o chip continua sem firmware");
                fw_kick("autocura: silêncio total com o node aberto");
                dirty = 1;
            }
        }

        if (ui_live && on && dirty) { ui_present(); dirty = 0; }
    }
    // PID 1 nunca retorna: o kernel entra em panic se /init sair.
}
