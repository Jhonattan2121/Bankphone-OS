/* BANKPHONE OS — HardwareService: descoberta e leitura REAL de /sys.
 *
 * Nada aqui adivinha caminho. A varredura olha o que existe e classifica pelo
 * CONTEÚDO (type/uevent), não pelo nome. Todo resultado (inclusive o que não
 * existe) vai para o log com a mesma frase que aparece na tela.
 */
#define _GNU_SOURCE
#include "hw.h"
#include "bootdiag.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

HwInfo HW;

/* --------------------------------------------------------------- leitura --- */
static int rd_str(const char *path, char *out, size_t n)
{
    out[0] = 0;
    int f = open(path, O_RDONLY);
    if (f < 0) return -1;
    ssize_t r = read(f, out, n - 1);
    close(f);
    if (r <= 0) { out[0] = 0; return -1; }
    out[r] = 0;
    out[strcspn(out, "\r\n")] = 0;
    return 0;
}

static int rd_int(const char *path, long *out)
{
    char b[64];
    if (rd_str(path, b, sizeof b)) return -1;
    if (!b[0]) return -1;
    char *end = NULL;
    long v = strtol(b, &end, 10);
    if (end == b) return -1;
    *out = v;
    return 0;
}

static int rd_exists(const char *path) { return access(path, F_OK) == 0; }

/* junta diretório + arquivo, com corte seguro */
static void path_join(char *out, size_t n, const char *dir, const char *file)
{
    snprintf(out, n, "%.*s/%s", (int)(n > 16 ? n - 16 : 0), dir, file);
}

/* ---------------------------------------------------------- descoberta ----- */

/* Percorre /sys/class/<cls> e devolve o primeiro diretório com nome que contenha
 * `contem` e no qual exista o arquivo `exige`. Se `contem` for NULL, aceita qualquer. */
static int scan_class(const char *cls, const char *contem, const char *exige,
                      char *out_dir, size_t n, const char **nomes, int max_nomes)
{
    char base[64];
    snprintf(base, sizeof base, "/sys/class/%s", cls);
    DIR *d = opendir(base);
    if (!d) return 0;
    struct dirent *e;
    int achou = 0, listados = 0;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        if (contem && !strstr(e->d_name, contem)) continue;
        char dir[128], teste[240];
        snprintf(dir, sizeof dir, "%.*s/%.32s", (int)sizeof dir - 34, base, e->d_name);
        if (exige) { path_join(teste, sizeof teste, dir, exige); if (!rd_exists(teste)) continue; }
        if (nomes && listados < max_nomes) nomes[listados++] = strdup(e->d_name);
        if (!achou) { snprintf(out_dir, n, "%s", dir); achou = 1; }
    }
    if (nomes) { while (listados < max_nomes) nomes[listados++] = NULL; }
    closedir(d);
    return achou;
}

static void discover_power(void)
{
    const char *nomes[24];
    char base[64] = "/sys/class/power_supply";
    DIR *d = opendir(base);
    if (!d) {
        HW.batt.st = HW_UNAVAILABLE; snprintf(HW.batt.why, sizeof HW.batt.why, "/sys/class/power_supply ausente");
        HW.chg.st = HW_UNAVAILABLE; snprintf(HW.chg.why, sizeof HW.chg.why, "/sys/class/power_supply ausente");
        bd_log("hw: bateria/carregador UNAVAILABLE — /sys/class/power_supply não existe");
        return;
    }
    struct dirent *e;
    int n = 0;
    while ((e = readdir(d)) && n < 24) if (e->d_name[0] != '.') nomes[n++] = strdup(e->d_name);
    while (n < 24) nomes[n++] = NULL;
    closedir(d);

    char lista[256] = "";
    for (int i = 0; nomes[i]; i++) snprintf(lista + strlen(lista), sizeof lista - strlen(lista), "%s%s", i ? "," : "", nomes[i]);
    bd_log("hw: power_supply = [%s]", lista);

    char bateria[200] = "", carreg[200] = "", tipo[40] = "";
    for (int i = 0; nomes[i]; i++) {
        char p[240], t[40] = "";
        snprintf(p, sizeof p, "%s/%s/type", base, nomes[i]);
        rd_str(p, t, sizeof t);
        if (!t[0]) { snprintf(p, sizeof p, "%s/%s/uevent", base, nomes[i]); rd_str(p, t, sizeof t); }  /* sem /type: procura em uevent */
        if (strstr(t, "Battery") && !bateria[0]) { snprintf(bateria, sizeof bateria, "%s/%s", base, nomes[i]); snprintf(tipo, sizeof tipo, "%.*s", (int)sizeof tipo - 1, t); }
        else if ((strstr(t, "Mains") || strstr(t, "USB") || strstr(t, "Wireless")) && !carreg[0]) snprintf(carreg, sizeof carreg, "%s/%s", base, nomes[i]);
        bd_log("hw: power_supply %s type='%s'", nomes[i], t);
    }
    for (int i = 0; i < 24 && nomes[i]; i++) free((void *)nomes[i]);

    if (bateria[0]) {
        snprintf(HW.batt_node, sizeof HW.batt_node, "%.*s", (int)sizeof HW.batt_node - 1, bateria);
        HW.batt.st = HW_AVAILABLE;
        snprintf(HW.batt.why, sizeof HW.batt.why, "%.40s (type=%.20s)", bateria, tipo);
    } else {
        HW.batt.st = HW_UNAVAILABLE;
        snprintf(HW.batt.why, sizeof HW.batt.why, "nenhum nó com type=Battery");
    }
    if (carreg[0]) {
        snprintf(HW.chg_node, sizeof HW.chg_node, "%.*s", (int)sizeof HW.chg_node - 1, carreg);
        HW.chg.st = HW_AVAILABLE;
        snprintf(HW.chg.why, sizeof HW.chg.why, "%.*s", (int)sizeof HW.chg.why - 1, carreg);
    } else {
        HW.chg.st = HW_UNAVAILABLE;
        snprintf(HW.chg.why, sizeof HW.chg.why, "nenhum nó type=Mains/USB");
    }
}

static void discover_backlight(void)
{
    char dir[200] = "";
    /* 1) /sys/class/backlight (padrão do kernel) */
    if (scan_class("backlight", NULL, "brightness", dir, sizeof dir, NULL, 0)) {
        snprintf(HW.bl_node, sizeof HW.bl_node, "%.*s/brightness", (int)sizeof HW.bl_node - 12, dir);
        HW.bl.st = HW_AVAILABLE;
        snprintf(HW.bl.why, sizeof HW.bl.why, "%.*s", (int)sizeof HW.bl.why - 1, dir);
        bd_log("hw: backlight (classe backlight) = %s", dir);
    } else {
        /* 2) /sys/class/leds (no MTK o LCD costuma ser um led chamado lcd-backlight) */
        const char *nomes[32];
        char leds[64] = "/sys/class/leds";
        DIR *d = opendir(leds);
        if (d) {
            struct dirent *e; int n = 0;
            while ((e = readdir(d)) && n < 32) if (e->d_name[0] != '.') nomes[n++] = strdup(e->d_name);
            while (n < 32) nomes[n++] = NULL;
            closedir(d);
            char lista[300] = "";
            for (int i = 0; nomes[i]; i++) snprintf(lista + strlen(lista), sizeof lista - strlen(lista), "%s%s", i ? "," : "", nomes[i]);
            bd_log("hw: /sys/class/leds = [%s]", lista);
            for (int i = 0; nomes[i]; i++) {
                char b[128];
                path_join(b, sizeof b, leds, nomes[i]);   /* reusa b como dir */
                char teste[300];
                path_join(teste, sizeof teste, b, "brightness");
                if (!rd_exists(teste)) continue;
                if (strstr(nomes[i], "backlight") || strstr(nomes[i], "lcd")) {
                    snprintf(HW.bl_node, sizeof HW.bl_node, "%.*s/brightness", (int)sizeof HW.bl_node - 12, b);
                    HW.bl.st = HW_AVAILABLE;
                    snprintf(HW.bl.why, sizeof HW.bl.why, "%.*s", (int)sizeof HW.bl.why - 1, b);
                    bd_log("hw: backlight (led) = %s", b);
                    break;
                }
            }
            for (int i = 0; i < 32 && nomes[i]; i++) free((void *)nomes[i]);
        }
        if (HW.bl.st != HW_AVAILABLE) {
            HW.bl.st = HW_UNAVAILABLE;
            snprintf(HW.bl.why, sizeof HW.bl.why, "sem /sys/class/backlight/*/brightness e sem led de backlight");
            bd_log("hw: brilho UNAVAILABLE — %s", HW.bl.why);
        }
    }
    if (HW.bl.st == HW_AVAILABLE) {
        char p[240];
        snprintf(p, sizeof p, "%.*s", (int)sizeof p - 1, HW.bl_node);
        char *slash = strrchr(p, '/'); if (slash) *slash = 0;
        char mx[240]; path_join(mx, sizeof mx, p, "max_brightness");
        long v = 0;
        if (rd_int(mx, &v) == 0 && v > 0) HW.bl_max = (int)v;
        else { HW.bl_max = 0; bd_log("hw: brilho: max_brightness ilegível — a escala vai ser tratada como UNKNOWN"); }
    }
}

static void discover_vibrator(void)
{
    /* 1) leds/vibrator* (MTK: brightness + duration) */
    const char *nomes[32];
    char leds[64] = "/sys/class/leds";
    DIR *d = opendir(leds);
    if (d) {
        struct dirent *e; int n = 0;
        while ((e = readdir(d)) && n < 32) if (e->d_name[0] != '.') nomes[n++] = strdup(e->d_name);
        while (n < 32) nomes[n++] = NULL;
        closedir(d);
        for (int i = 0; nomes[i]; i++) {
            if (!strstr(nomes[i], "vibrat")) continue;
            char b[240], t[300];
            path_join(b, sizeof b, leds, nomes[i]);
            path_join(t, sizeof t, b, "brightness");
            if (!rd_exists(t)) continue;
            snprintf(HW.vib_node, sizeof HW.vib_node, "%.*s", (int)sizeof HW.vib_node - 1, b);
            char dur[300]; path_join(dur, sizeof dur, b, "duration");
            HW.vib_style = 1;
            snprintf(HW.vib.why, sizeof HW.vib.why, "%.*s/brightness%s", (int)sizeof HW.vib.why - 30, b, rd_exists(dur) ? " +duration" : " (sem duration)");
            HW.vib.st = HW_AVAILABLE;
            bd_log("hw: vibração = %s", HW.vib.why);
            break;
        }
        for (int i = 0; i < 32 && nomes[i]; i++) free((void *)nomes[i]);
    }
    /* 2) timed_output/vibrator/enable (ms) — interface antiga */
    if (HW.vib.st != HW_AVAILABLE) {
        char t[200] = "/sys/class/timed_output/vibrator/enable";
        if (rd_exists(t)) {
            snprintf(HW.vib_node, sizeof HW.vib_node, "%.*s", (int)sizeof HW.vib_node - 1, t);
            HW.vib_style = 2;
            HW.vib.st = HW_AVAILABLE;
            snprintf(HW.vib.why, sizeof HW.vib.why, "%.*s", (int)sizeof HW.vib.why - 1, t);
            bd_log("hw: vibração = %s (timed_output, em ms)", t);
        } else {
            HW.vib.st = HW_UNAVAILABLE;
            snprintf(HW.vib.why, sizeof HW.vib.why, "sem led 'vibrator*' e sem /sys/class/timed_output/vibrator");
            bd_log("hw: vibração UNAVAILABLE — %s", HW.vib.why);
        }
    }
}

static void discover_net(void)
{
    char base[64] = "/sys/class/net";
    DIR *d = opendir(base);
    if (!d) { HW.net.st = HW_UNAVAILABLE; snprintf(HW.net.why, sizeof HW.net.why, "/sys/class/net ausente"); return; }
    struct dirent *e;
    char lista[200] = "";
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        snprintf(lista + strlen(lista), sizeof lista - strlen(lista), "%s%.16s", lista[0] ? "," : "", e->d_name);
        char p[240], st[24] = "", car[8] = "";
        snprintf(p, sizeof p, "%.*s/%.16s/operstate", (int)sizeof p - 32, base, e->d_name); rd_str(p, st, sizeof st);
        snprintf(p, sizeof p, "%.*s/%.16s/carrier", (int)sizeof p - 30, base, e->d_name); rd_str(p, car, sizeof car);
        int up = (st[0] == 'u' && st[1] == 'p') || (car[0] == '1');
        if (!HW.net_if[0]) {                                   /* primeira interface: registra como candidata */
            snprintf(HW.net_if, sizeof HW.net_if, "%.*s", (int)sizeof HW.net_if - 1, e->d_name);
            snprintf(HW.net_state, sizeof HW.net_state, "%.*s", (int)sizeof HW.net_state - 1, st[0] ? st : "?");
            HW.net_up = up;
            bd_log("hw: rede: %s operstate=%s carrier=%s", e->d_name, st[0] ? st : "?", car[0] ? car : "?");
        }
    }
    closedir(d);
    bd_log("hw: /sys/class/net = [%s]", lista);
    if (lista[0]) {
        HW.net.st = HW_AVAILABLE;                      /* existe interface; se está UP é outra pergunta */
        snprintf(HW.net.why, sizeof HW.net.why, "interfaces: %.48s", lista);
    } else {
        HW.net.st = HW_UNAVAILABLE; snprintf(HW.net.why, sizeof HW.net.why, "nenhuma interface");
    }
}

void hw_init(void)
{
    memset(&HW, 0, sizeof HW);
    HW.chg_online = -1;
    HW.vib_last_ms = -1;
    HW.batt.st = HW.chg.st = HW.bl.st = HW.vib.st = HW.scr.st = HW.touch.st = HW.keys.st = HW.net.st = HW_UNKNOWN;
    snprintf(HW.batt.why, sizeof HW.batt.why, "não descoberto");
    snprintf(HW.net.why, sizeof HW.net.why, "não descoberto");

    discover_power();
    discover_backlight();
    discover_vibrator();
    discover_net();
    HW.touch.st = HW_UNKNOWN; snprintf(HW.touch.why, sizeof HW.touch.why, "aguardando o init de entrada");
    HW.keys.st = HW_UNKNOWN;  snprintf(HW.keys.why, sizeof HW.keys.why, "aguardando o init de entrada");
    HW.scr.st = HW_UNKNOWN;   snprintf(HW.scr.why, sizeof HW.scr.why, "aguardando o framebuffer");
    (void)hw_tick(0);
    bd_flush();
}

/* ------------------------------------------------------------- leitura viva */

int hw_tick(long long now_ms)
{
    /* fotografia do que a tela mostra, para detectar mudança de verdade */
    int p_cap = HW.capacity, p_on = HW.chg_online, p_bl = HW.bl_pct, p_up = HW.net_up;
    long p_v = HW.voltage_uv, p_t = HW.temp_dc, p_i = HW.current_ua;
    char p_st[24]; snprintf(p_st, sizeof p_st, "%s", HW.status);
    char p_nst[16]; snprintf(p_nst, sizeof p_nst, "%s", HW.net_state);
    long v;
    if (HW.batt.st == HW_AVAILABLE) {
        char p[240];
        snprintf(p, sizeof p, "%s/capacity", HW.batt_node);
        if (rd_int(p, &v) == 0 && v >= 0 && v <= 100) { HW.capacity = (int)v; }
        else { HW.capacity = -1; }
        snprintf(p, sizeof p, "%s/status", HW.batt_node);
        if (rd_str(p, HW.status, sizeof HW.status)) HW.status[0] = 0;
        snprintf(p, sizeof p, "%s/health", HW.batt_node);
        if (rd_str(p, HW.health, sizeof HW.health)) HW.health[0] = 0;
        snprintf(p, sizeof p, "%s/technology", HW.batt_node);
        if (rd_str(p, HW.tech, sizeof HW.tech)) HW.tech[0] = 0;
        snprintf(p, sizeof p, "%s/voltage_now", HW.batt_node);
        if (rd_int(p, &v)) HW.voltage_uv = -1; else HW.voltage_uv = v;
        snprintf(p, sizeof p, "%s/temp", HW.batt_node);
        if (rd_int(p, &v)) HW.temp_dc = -1000; else HW.temp_dc = v;
        snprintf(p, sizeof p, "%s/current_now", HW.batt_node);
        if (rd_int(p, &v)) HW.current_ua = 0; else HW.current_ua = v;
        snprintf(p, sizeof p, "%s/charge_counter", HW.batt_node);
        if (rd_int(p, &v)) HW.charge_counter_uah = -1; else HW.charge_counter_uah = v;
        snprintf(p, sizeof p, "%s/present", HW.batt_node);
        if (rd_int(p, &v)) HW.present = -1; else HW.present = (int)v;
    }
    if (HW.chg.st == HW_AVAILABLE) {
        char p[240];
        snprintf(p, sizeof p, "%s/online", HW.chg_node);
        if (rd_int(p, &v) == 0) HW.chg_online = (int)(v ? 1 : 0);
        snprintf(p, sizeof p, "%s/type", HW.chg_node);
        if (rd_str(p, HW.chg_type, sizeof HW.chg_type)) HW.chg_type[0] = 0;
    }
    if (HW.bl.st == HW_AVAILABLE) {
        long raw = -1;
        if (rd_int(HW.bl_node, &raw) == 0) {
            HW.bl_raw = (int)raw;
            HW.bl_pct = HW.bl_max > 0 ? (int)((long)raw * 100 / HW.bl_max) : -1;
        }
    }
    HW.last_read_ms = now_ms;
    return (p_cap != HW.capacity || p_on != HW.chg_online || p_bl != HW.bl_pct || p_up != HW.net_up
            || p_v != HW.voltage_uv || p_t != HW.temp_dc || p_i != HW.current_ua
            || strcmp(p_st, HW.status) || strcmp(p_nst, HW.net_state)) ? 1 : 0;
}

void hw_set_display(int w, int h, int bpp, int stride, const char *fbnode)
{
    HW.scr_w = w; HW.scr_h = h; HW.scr_bpp = bpp; HW.scr_stride = stride;
    snprintf(HW.fb_node, sizeof HW.fb_node, "%s", fbnode ? fbnode : "?");
    if (w > 0 && h > 0) {
        HW.scr.st = HW_AVAILABLE;
        snprintf(HW.scr.why, sizeof HW.scr.why, "%dx%d bpp=%d stride=%d", w, h, bpp, stride);
    } else {
        HW.scr.st = HW_UNAVAILABLE; snprintf(HW.scr.why, sizeof HW.scr.why, "framebuffer não abriu");
    }
}

void hw_note_touch(const char *node, const char *name, int ok)
{
    snprintf(HW.touch_node, sizeof HW.touch_node, "%s", node ? node : "");
    snprintf(HW.touch_name, sizeof HW.touch_name, "%s", name ? name : "");
    HW.touch.st = ok ? HW_AVAILABLE : HW_UNAVAILABLE;
    snprintf(HW.touch.why, sizeof HW.touch.why, "%s",
             ok ? "node aberto por capacidade" : "nenhum node com eixos de toque");
}

void hw_note_keys(int n, const char *desc)
{
    HW.keys_n = n;
    snprintf(HW.keys_desc, sizeof HW.keys_desc, "%s", desc ? desc : "");
    HW.keys.st = n > 0 ? HW_AVAILABLE : HW_UNAVAILABLE;
    snprintf(HW.keys.why, sizeof HW.keys.why, "%s", n > 0 ? desc ? desc : "?" : "nenhum node com KEY_VOLUMEUP/DOWN/POWER");
}

/* --------------------------------------------------------------- brilho ---- */

int hw_brightness_get(void)
{
    if (HW.bl.st != HW_AVAILABLE) return -1;
    long raw = -1;
    if (rd_int(HW.bl_node, &raw)) return -1;
    HW.bl_raw = (int)raw;
    HW.bl_pct = HW.bl_max > 0 ? (int)((long)raw * 100 / HW.bl_max) : -1;
    return HW.bl_pct;
}

int hw_brightness_set(int pct)
{
    if (HW.bl.st != HW_AVAILABLE) { bd_log("hw: brilho: sem nó (nada escrito)"); return -1; }
    if (HW.bl_max <= 0) { bd_log("hw: brilho: max_brightness desconhecido — recusando escrever às cegas"); return -2; }
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    if (pct > 0 && pct < 3) pct = 3;                    /* abaixo disso o painel some */
    int val = (int)((long)pct * HW.bl_max / 100);
    if (val < 1 && pct > 0) val = 1;
    char b[16];
    snprintf(b, sizeof b, "%d", val);
    int f = open(HW.bl_node, O_WRONLY);
    if (f < 0) { bd_log("hw: brilho: %s não abriu para escrita (errno=%d)", HW.bl_node, errno); return -3; }
    ssize_t w = write(f, b, strlen(b));
    close(f);
    int back = hw_brightness_get();                     /* LÊ DE VOLTA: é a prova */
    bd_log("hw: brilho %d%% -> %d/%d (write=%zd) | lido de volta = %d%%", pct, val, HW.bl_max, w, back);
    bd_flush();
    return (w > 0 && back >= 0) ? 0 : -4;
}

/* ------------------------------------------------------------- vibração ---- */

int hw_vibrate(int ms)
{
    if (HW.vib.st != HW_AVAILABLE) return -1;
    if (ms <= 0) ms = 1;
    if (ms > 2000) ms = 2000;
    char b[24];
    int ok = 0;
    if (HW.vib_style == 1) {
        char dur[240];
        snprintf(dur, sizeof dur, "%s/duration", HW.vib_node);
        if (rd_exists(dur)) {
            snprintf(b, sizeof b, "%d", ms);
            int f = open(dur, O_WRONLY);
            if (f >= 0) { if (write(f, b, strlen(b)) > 0) ok = 1; close(f); }
        }
        int f = open(HW.vib_node, O_WRONLY);            /* .../brightness */
        if (f >= 0) {
            snprintf(b, sizeof b, "255");
            if (write(f, b, strlen(b)) > 0) ok = 1;
            else { snprintf(b, sizeof b, "1"); if (write(f, b, strlen(b)) > 0) ok = 1; }
            close(f);
        }
    } else if (HW.vib_style == 2) {
        int f = open(HW.vib_node, O_WRONLY);
        if (f >= 0) { snprintf(b, sizeof b, "%d", ms); if (write(f, b, strlen(b)) > 0) ok = 1; close(f); }
    }
    HW.vib_last_ms = ms;
    if (ok) { HW.vib_writes++; HW.vib.st = HW_AVAILABLE; }
    else { HW.vib.st = HW_UNKNOWN; snprintf(HW.vib.why, sizeof HW.vib.why, "escrita em %.*s falhou", (int)sizeof HW.vib.why - 20, HW.vib_node); }
    return ok ? 0 : -2;
}

/* -------------------------------------------------------------- strings ---- */

const char *hw_batt_pct_str(void)
{
    static char b[16];
    if (HW.batt.st != HW_AVAILABLE || HW.capacity < 0) return "--";
    snprintf(b, sizeof b, "%d%%", HW.capacity);
    return b;
}

const char *hw_batt_status_str(void)
{
    if (HW.batt.st != HW_AVAILABLE) return "NOT AVAILABLE";
    if (!HW.status[0]) return "UNKNOWN";
    if (!strcmp(HW.status, "Charging")) return "CHARGING";
    if (!strcmp(HW.status, "Discharging")) return "DISCHARGING";
    if (!strcmp(HW.status, "Full")) return "FULL";
    if (!strcmp(HW.status, "Not charging")) return "NOT CHARGING";
    return HW.status;
}

const char *hw_net_str(void)
{
    if (HW.net.st != HW_AVAILABLE || !HW.net_if[0]) return "";
    if (!HW.net_up) return "";
    return HW.net_if;
}

const char *hw_state_str(HwState s)
{
    return s == HW_AVAILABLE ? "AVAILABLE" : s == HW_UNAVAILABLE ? "UNAVAILABLE" : "UNKNOWN";
}

/* ------------------------------------------------------------- relatório --- */

void hw_report(void)
{
    bd_log("=== HARDWARE SERVICE ===");
    bd_log("hw: bateria      %-11s %s", hw_state_str(HW.batt.st), HW.batt.why);
    if (HW.batt.st == HW_AVAILABLE)
        bd_log("hw:   capacity=%d%% status='%s' voltage=%ld.%03ldV temp=%ld.%ldC current=%ld.%03ldmA health='%s' tech='%s' present=%d",
               HW.capacity, HW.status[0] ? HW.status : "?", HW.voltage_uv / 1000000, (HW.voltage_uv % 1000000) / 1000,
               HW.temp_dc / 10, labs(HW.temp_dc % 10),
               HW.current_ua / 1000, labs(HW.current_ua % 1000), HW.health, HW.tech, HW.present);
    bd_log("hw: carregador   %-11s %s", hw_state_str(HW.chg.st), HW.chg.why);
    if (HW.chg.st == HW_AVAILABLE) bd_log("hw:   online=%d type='%s'", HW.chg_online, HW.chg_type);
    bd_log("hw: brilho       %-11s %s (%d/%d = %d%%)", hw_state_str(HW.bl.st), HW.bl.why, HW.bl_raw, HW.bl_max, HW.bl_pct);
    bd_log("hw: vibração     %-11s %s", hw_state_str(HW.vib.st), HW.vib.why);
    bd_log("hw: tela         %-11s %s", hw_state_str(HW.scr.st), HW.scr.why);
    bd_log("hw: toque        %-11s %s '%s'", hw_state_str(HW.touch.st), HW.touch.why, HW.touch_name);
    bd_log("hw: teclas       %-11s %s", hw_state_str(HW.keys.st), HW.keys.why);
    bd_log("hw: rede         %-11s %s (if=%s operstate=%s)", hw_state_str(HW.net.st), HW.net.why,
           HW.net_if[0] ? HW.net_if : "-", HW.net_state[0] ? HW.net_state : "-");
    bd_flush();
}
