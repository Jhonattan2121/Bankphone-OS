/*
 * BANKPHONE OS — HOST (macOS/Linux): renderiza as telas em PNG para revisão
 * ANTES de qualquer teste no aparelho. Não faz parte do sistema do celular.
 *
 * O que este programa faz:
 *   1. roda o MESMO ui.c/screens.c/sheets.c/components.c do aparelho;
 *   2. dirige a UI pelo MESMO caminho de toque do aparelho (ui_touch), achando
 *      os alvos pela AÇÃO registrada (ui_hit_find) — não por pixel chumbado;
 *   3. descobre o hardware do próprio Mac (hw_init) e escreve o que encontrou.
 *
 * Dois modos:
 *   (padrão)          valores de hardware REAIS do Mac => quase tudo
 *                     UNAVAILABLE/NOT AVAILABLE. Prova que o estado honesto
 *                     renderiza e que nada é inventado.
 *   --amostra         injeta um conjunto de valores de AMOSTRA (bateria 87%,
 *                     brilho, vibração, toque event3...) para o usuário ver o
 *                     desenho pretendido. Os nomes dos arquivos levam
 *                     "-amostra" e a tela mostra o selo AMOSTRA no rodapé:
 *                     nenhum valor de amostra se disfarça de medição.
 *
 * Uso:  ./host out/            (gera out/pv-*.png)
 *       ./host out/ --amostra
 */
#define _GNU_SOURCE
#include "ui.h"
#include "nav.h"
#include "components.h"
#include "icons.h"
#include "money.h"
#include "sec.h"
#include "hw.h"
#include "store.h"
#include "bootdiag.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <zlib.h>

/* ------------------------------------------------------- ganchos do host --- */
static int64_t g_now = 1759468800;              /* 2025-10-03 00:00 UTC */
static int g_clock = 1;
int64_t plat_now(void) { return g_now; }
int plat_clock_valid(void) { return g_clock; }
void plat_random(uint8_t *b, int n) { for (int i = 0; i < n; i++) b[i] = (uint8_t)(rand() & 255); }
void plat_save(void) {}
int plat_persist_ok(void) { return 0; }
const char *plat_boot_prop(const char *k)
{
    static char v[64];
    v[0] = 0;
    /* no Mac não existe cmdline de kernel: o host responde o que tem */
    if (!strcmp(k, "bankphone.ro")) snprintf(v, sizeof v, "1");
    return v;
}

/* bootdiag: o host não tem kernel para ler — as chamadas viram eco no terminal */
void bd_log(const char *fmt, ...) { (void)fmt; }
void bd_flush(void) {}
int  bd_serial_fd(void) { return -1; }

/* Contadores do PID 1: no host quem publica é este arquivo (via ui_set_dev).
 * Sem --amostra ficam zerados — que é a verdade aqui: não há PID 1 no Mac. */
static UiDev g_dev;

/* -------------------------------------------------------------- PNG ------- */
static uint32_t *g_px;
static int g_w, g_h;

static void png_write(const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) { printf("host: não consegui escrever %s\n", path); return; }
    /* linhas com filtro 0 */
    size_t raw = (size_t)g_h * (1 + (size_t)g_w * 3);
    unsigned char *buf = malloc(raw);
    for (int y = 0; y < g_h; y++) {
        unsigned char *l = buf + (size_t)y * (1 + (size_t)g_w * 3);
        l[0] = 0;
        for (int x = 0; x < g_w; x++) {
            uint32_t c = g_px[(size_t)y * g_w + x];
            l[1 + x * 3 + 0] = (unsigned char)(c >> 16);
            l[1 + x * 3 + 1] = (unsigned char)(c >> 8);
            l[1 + x * 3 + 2] = (unsigned char)c;
        }
    }
    unsigned long clen = compressBound(raw);
    unsigned char *comp = malloc(clen);
    compress2(comp, &clen, buf, raw, 6);

    unsigned char hdr[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n' };
    fwrite(hdr, 1, 8, f);
    const char *ctype[3] = { "IHDR", "IDAT", "IEND" };
    unsigned char ihdr[13];
    ihdr[0] = g_w >> 24; ihdr[1] = g_w >> 16; ihdr[2] = g_w >> 8; ihdr[3] = g_w;
    ihdr[4] = g_h >> 24; ihdr[5] = g_h >> 16; ihdr[6] = g_h >> 8; ihdr[7] = g_h;
    ihdr[8] = 8; ihdr[9] = 2; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
    struct { const char *t; const unsigned char *d; unsigned len; } chunk[3] = {
        { ctype[0], ihdr, 13 }, { ctype[1], comp, (unsigned)clen }, { ctype[2], NULL, 0 }
    };
    for (int i = 0; i < 3; i++) {
        unsigned char len[4] = { chunk[i].len >> 24, chunk[i].len >> 16, chunk[i].len >> 8, chunk[i].len };
        fwrite(len, 1, 4, f);
        fwrite(chunk[i].t, 1, 4, f);
        if (chunk[i].len) fwrite(chunk[i].d, 1, chunk[i].len, f);
        unsigned char crcbuf[4];
        uLong c = crc32(0L, Z_NULL, 0);
        c = crc32(c, (const Bytef *)chunk[i].t, 4);
        if (chunk[i].len) c = crc32(c, chunk[i].d, chunk[i].len);
        crcbuf[0] = c >> 24; crcbuf[1] = c >> 16; crcbuf[2] = c >> 8; crcbuf[3] = c;
        fwrite(crcbuf, 1, 4, f);
    }
    fclose(f);
    free(buf); free(comp);
    printf("  %s  (%dx%d)\n", path, g_w, g_h);
}

/* ------------------------------------------------------------- fontes ----- */
static unsigned char *read_all(const char *p, long *n)
{
    FILE *f = fopen(p, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    *n = ftell(f);
    rewind(f);
    unsigned char *b = malloc((size_t)*n);
    if (fread(b, 1, (size_t)*n, f) != (size_t)*n) { fclose(f); free(b); return NULL; }
    fclose(f);
    return b;
}

/* -------------------------------------------------------------- toques ---- */
/* Um toque é sempre precedido por um quadro: é assim no aparelho (loop de 1 Hz
 * ou a cada evento) e é o que garante que os alvos sejam os do quadro atual. */
static void tap_act(int a, int arg)
{
    ui_draw();
    int x, y;
    if (!ui_hit_find(a, arg, &x, &y)) { printf("host: alvo (act=%d arg=%d) não existe nesta tela\n", a, arg); return; }
    ui_touch(0, x, y);
    ui_touch(2, x, y);
    ui_draw();
}

static void type_digits(const char *d)
{
    for (; *d; d++) {
        ui_draw();
        int x, y;
        if (!ui_hit_find(ACT_KEY, *d, &x, &y)) { printf("host: tecla '%c' não encontrada\n", *d); return; }
        ui_touch(0, x, y);
        ui_touch(2, x, y);
    }
}

/* ------------------------------------------------------------ amostra ----- */
static void aplicar_amostra(void)
{
    HW.batt.st = HW_AVAILABLE;
    snprintf(HW.batt.why, sizeof HW.batt.why, "/sys/class/power_supply/battery (AMOSTRA)");
    snprintf(HW.batt_node, sizeof HW.batt_node, "/sys/class/power_supply/battery");
    HW.capacity = 87; snprintf(HW.status, sizeof HW.status, "Charging");
    snprintf(HW.health, sizeof HW.health, "Good"); snprintf(HW.tech, sizeof HW.tech, "Li-ion");
    HW.voltage_uv = 3862000; HW.temp_dc = 312; HW.current_ua = 486000; HW.present = 1;
    HW.chg.st = HW_AVAILABLE; snprintf(HW.chg.why, sizeof HW.chg.why, "/sys/class/power_supply/usb (AMOSTRA)");
    HW.chg_online = 1; snprintf(HW.chg_type, sizeof HW.chg_type, "USB");
    HW.bl.st = HW_AVAILABLE; snprintf(HW.bl_node, sizeof HW.bl_node, "/sys/class/leds/lcd-backlight/brightness");
    snprintf(HW.bl.why, sizeof HW.bl.why, "/sys/class/leds/lcd-backlight (AMOSTRA)");
    HW.bl_max = 255; HW.bl_raw = 102; HW.bl_pct = 40;
    HW.vib.st = HW_AVAILABLE; snprintf(HW.vib_node, sizeof HW.vib_node, "/sys/class/leds/vibrator");
    snprintf(HW.vib.why, sizeof HW.vib.why, "/sys/class/leds/vibrator/brightness +duration (AMOSTRA)");
    HW.vib_style = 1; HW.vib_writes = 0;
    HW.scr.st = HW_AVAILABLE; HW.scr_w = 720; HW.scr_h = 1612; HW.scr_bpp = 32; HW.scr_stride = 2944;
    snprintf(HW.fb_node, sizeof HW.fb_node, "/dev/fb0");
    snprintf(HW.scr.why, sizeof HW.scr.why, "720x1612 bpp=32 stride=2944");
    HW.touch.st = HW_AVAILABLE; snprintf(HW.touch_node, sizeof HW.touch_node, "/dev/input/event3");
    snprintf(HW.touch_name, sizeof HW.touch_name, "mtk-tpd");
    snprintf(HW.touch.why, sizeof HW.touch.why, "node aberto por capacidade");
    HW.keys.st = HW_AVAILABLE; HW.keys_n = 3;
    snprintf(HW.keys_desc, sizeof HW.keys_desc, "event0,event1,event2");
    HW.net.st = HW_AVAILABLE; snprintf(HW.net_if, sizeof HW.net_if, "wlan0");
    snprintf(HW.net_state, sizeof HW.net_state, "down");
    snprintf(HW.net.why, sizeof HW.net.why, "interfaces: lo,wlan0");
    HW.net_up = 0;
}

static void dev_amostra(void)
{
    g_dev.touch_ok = 1; g_dev.touch_ev = 1284; g_dev.touch_down = 37; g_dev.touch_move = 1180;
    g_dev.touch_up = 36; g_dev.touch_kicks = 1; g_dev.touch_dropped = 0;
    snprintf(g_dev.touch_node, sizeof g_dev.touch_node, "/dev/input/event3");
    snprintf(g_dev.touch_name, sizeof g_dev.touch_name, "mtk-tpd");
    snprintf(g_dev.fw_ver, sizeof g_dev.fw_ver, "fw_ver=0x50, x_num=32, y_num=72 (AMOSTRA)");
    g_dev.uptime_s = 412; g_dev.frames = 412; g_dev.serial_ok = 1;
    ui_set_dev(&g_dev);
}

/* ---------------------------------------------------------------- main ---- */
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
/* BENCH=1: mede o custo médio de ui_draw() em cada captura (ms no Mac; só serve para comparar antes/depois). */
static void timed_draw(int line)
{
    ui_draw();
    if (!getenv("BENCH")) return;
    struct timespec a, b; clock_gettime(CLOCK_MONOTONIC, &a);
    for (int i = 0; i < 20; i++) ui_draw();
    clock_gettime(CLOCK_MONOTONIC, &b);
    printf("BENCH linha %-4d  %7.2f ms/quadro\n", line, ((b.tv_sec - a.tv_sec) * 1e3 + (b.tv_nsec - a.tv_nsec) / 1e6) / 20.0);
}

int main(int argc, char **argv)
{
    const char *outdir = argc > 1 ? argv[1] : "work";
    int amostra = 0;
    for (int i = 1; i < argc; i++) if (!strcmp(argv[i], "--amostra")) amostra = 1;
    char cmd[512];
    snprintf(cmd, sizeof cmd, "mkdir -p %s", outdir);
    if (system(cmd)) {}

    g_w = 720; g_h = 1612;
    g_px = calloc((size_t)g_w * g_h, 4);
    if (!g_px) return 1;
    Surf s = { g_px, g_w, g_h, g_w };

    /* A fonte é procurada em vários lugares: assim o script pode rodar da raiz do
     * repo ou de dentro de src/init sem quebrar. BANKPHONE_FONT manda em tudo. */
    long tn = 0;
    static const char *cand[] = {
        "assets/Roboto-Regular.ttf", "src/init/assets/Roboto-Regular.ttf",
        "assets/Roboto-Medium.ttf",  "src/init/assets/Roboto-Medium.ttf",
    };
    const char *fonte = getenv("BANKPHONE_FONT") ? getenv("BANKPHONE_FONT") : cand[0];
    unsigned char *ttf = read_all(fonte, &tn);
    for (int i = 0; !ttf && i < 4; i++) { fonte = cand[i]; ttf = read_all(fonte, &tn); }
    if (ttf) printf("host: fonte = %s (%ld bytes)\n", fonte, tn);
    if (!ttf) { printf("host: sem fonte em assets/ (coloque Roboto-Regular.ttf)\n"); return 1; }
    if (gfx_init(ttf)) { printf("host: TTF inválido\n"); return 1; }
    gfx_set(s);

    hw_init();                       /* descoberta REAL do Mac */
    if (amostra) { aplicar_amostra(); dev_amostra(); }
    ui_init(g_w, g_h);

    /* BANKPHONE_ESTADO=file:/caminho.img faz o host mostrar a tela com a área de
     * estado JÁ ARMADA — o mesmo caminho de código do aparelho, lendo um arquivo
     * de verdade (nada de valor inventado na mão dentro do desenho). Sem esta
     * variável, a tela mostra a verdade do Mac: "NÃO PEDIDA". */
    {
        const char *est = getenv("BANKPHONE_ESTADO");
        if (est && *est) {
            static char sbuf[200000];
            size_t sn = 0;
            store_config(est, "");
            if (store_open() == 0) {
                int r = store_load(sbuf, sizeof sbuf - 1, &sn);
                if (r == 0) {
                    sbuf[sn] = 0;
                    char *nl = strchr(sbuf, '\n');
                    if (sbuf[0] == 'P' && nl) { *nl = 0; pin_deserialize(sbuf); m_deserialize(nl + 1); }
                }
                g_dev.persist_ok = 1;
                ui_set_dev(&g_dev);
                printf("host: área de estado ARMADA a partir de %s (seqA=%u seqB=%u, %u gravação(ões))\n",
                       est, ST.seq[0], ST.seq[1], ST.saves);
            } else {
                printf("host: %s não armou: %s\n", est, ST.why);
            }
        }
    }
    snprintf(g_dev.fw_ver, sizeof g_dev.fw_ver, "(host: não lido)");

    char path[512];
    const char *modo = amostra ? "-amostra" : "";

    /* 1. bloqueio, criando o PIN */
    timed_draw(__LINE__);
    snprintf(path, sizeof path, "%s/pv-1-criar-pin%s.png", outdir, modo);
    png_write(path);

    /* 2. bloqueio com PIN já definido */
    {
        uint8_t salt[16];
        plat_random(salt, 16);
        pin_set("123456", salt);
        ui_lock();
        timed_draw(__LINE__);
        snprintf(path, sizeof path, "%s/pv-2-bloqueado%s.png", outdir, modo);
        png_write(path);
    }

    /* 3. destravado, ledger VAZIO (estado honesto de primeira execução) */
    type_digits("123456");
    timed_draw(__LINE__);
    snprintf(path, sizeof path, "%s/pv-3-dinheiro-vazio%s.png", outdir, modo);
    png_write(path);

    /* 4. agora com histórico: umas transações DEMO no ledger do host */
    m_load_demo(g_now);
    m_receive_pix(25000, "Contato DEMO", g_now - 3600 * 5);
    {
        const char *e = NULL;
        Tx *t = m_prepare_pix("Maria Souza", 15000, g_now - 3600 * 2, &e);
        if (t) m_authorize(t, L_PIN, g_now - 3600 * 2 + 5);
        t = m_prepare_pix("João Lima", 123450, g_now - 3600 * 26, &e);
        if (t) m_authorize(t, L_PIN, g_now - 3600 * 26 + 5);
    }
    timed_draw(__LINE__);
    snprintf(path, sizeof path, "%s/pv-4-dinheiro%s.png", outdir, modo);
    png_write(path);


    /* 5. atividade */
    tap_act(ACT_TAB, SC_ACTIVITY);
    timed_draw(__LINE__);
    snprintf(path, sizeof path, "%s/pv-5-atividade%s.png", outdir, modo);
    png_write(path);

    /* 6. carteira (estado honesto: nada gerado) */
    tap_act(ACT_TAB, SC_WALLET);
    timed_draw(__LINE__);
    snprintf(path, sizeof path, "%s/pv-6-carteira%s.png", outdir, modo);
    png_write(path);

    /* 7. segurança */
    tap_act(ACT_TAB, SC_SECURITY);
    timed_draw(__LINE__);
    snprintf(path, sizeof path, "%s/pv-7-seguranca%s.png", outdir, modo);
    png_write(path);

    /* 8. ajustes (hardware do Mac; com --amostra, valores de amostra) */
    tap_act(ACT_PUSH, SC_SETTINGS);
    timed_draw(__LINE__);
    snprintf(path, sizeof path, "%s/pv-8-ajustes%s.png", outdir, modo);
    png_write(path);

    /* 9. diagnóstico */
    tap_act(ACT_PUSH, SC_DEVELOPER);
    timed_draw(__LINE__);
    snprintf(path, sizeof path, "%s/pv-9-diagnostico%s.png", outdir, modo);
    png_write(path);

    /* 9b. diagnóstico rolado até o fim: prova que a seção de PERSISTÊNCIA (F2)
     *     desenha inteira, sem sobreposição, no quadro real de 720x1612. */
    ui_touch(0, 360, 1400); ui_touch(1, 360, 1200); ui_touch(1, 360, 400); ui_touch(2, 360, 400);
    ui_touch(0, 360, 1400); ui_touch(1, 360, 1200); ui_touch(1, 360, 400); ui_touch(2, 360, 400);
    timed_draw(__LINE__);
    snprintf(path, sizeof path, "%s/dev-diag-fim%s.png", outdir, modo);
    png_write(path);

    /* 10. folha de valor */
    tap_act(ACT_BACK, 0);            /* diagnóstico -> ajustes */
    tap_act(ACT_BACK, 0);            /* ajustes -> abas */
    tap_act(ACT_TAB, SC_MONEY);
    tap_act(ACT_SHEET, SH_AMOUNT);
    type_digits("12345");
    timed_draw(__LINE__);
    snprintf(path, sizeof path, "%s/pv-10-valor%s.png", outdir, modo);
    png_write(path);

    /* 11. revisão, com o aviso de demonstração */
    tap_act(ACT_CONT, 0);
    timed_draw(__LINE__);
    snprintf(path, sizeof path, "%s/pv-11-revisao%s.png", outdir, modo);
    png_write(path);

    /* 12. PIN de autorização */
    tap_act(ACT_AUTH, 0);
    timed_draw(__LINE__);
    snprintf(path, sizeof path, "%s/pv-12-pin%s.png", outdir, modo);
    png_write(path);

    /* 12b. a folha do PIN com dígitos JÁ DIGITADOS: prova que os pontinhos
     *      acompanham o teclado (era um defeito só visível no aparelho) */
    type_digits("123");
    timed_draw(__LINE__);
    snprintf(path, sizeof path, "%s/dev-pin-preenchido%s.png", outdir, modo);
    png_write(path);

    /* 13. comprovante, já com o resultado REAL da autorização */
    type_digits("456");
    timed_draw(__LINE__);
    snprintf(path, sizeof path, "%s/pv-13-comprovante%s.png", outdir, modo);
    png_write(path);

    /* ---- capturas de CONFERÊNCIA: provam rolagem, folhas e o aviso. Não são
     *      entrega principal, mas ficam no mesmo diretório e no relatório. ---- */
    {
        /* folha de aviso pelo caminho real (botão de teste do diagnóstico) */
        tap_act(ACT_CLOSE, 0);                 /* fecha o comprovante, se aberto */
        tap_act(ACT_TAB, SC_MONEY);
        tap_act(ACT_PUSH, SC_SETTINGS);
        tap_act(ACT_PUSH, SC_DEVELOPER);
        tap_act(ACT_NOTICE_TEST, 0);
        timed_draw(__LINE__);
        snprintf(path, sizeof path, "%s/dev-aviso%s.png", outdir, modo);
        png_write(path);
    }
    {
        /* rolagem até o fim das telas longas */
        tap_act(ACT_CLOSE, 0);
        tap_act(ACT_BACK, 0);                  /* diagnóstico -> ajustes */
        tap_act(ACT_BACK, 0);                  /* ajustes -> abas */
        tap_act(ACT_TAB, SC_ACTIVITY);
        ui_touch(0, 360, 1200); ui_touch(1, 360, 1100); ui_touch(1, 360, 600); ui_touch(2, 360, 600);
        timed_draw(__LINE__);
        snprintf(path, sizeof path, "%s/dev-rolagem-fim%s.png", outdir, modo);
        png_write(path);
        tap_act(ACT_TAB, SC_MONEY);
        ui_touch(0, 360, 1200); ui_touch(1, 360, 1100); ui_touch(1, 360, 600); ui_touch(2, 360, 600);
        timed_draw(__LINE__);
        snprintf(path, sizeof path, "%s/dev-dinheiro-fim%s.png", outdir, modo);
        png_write(path);
    }
    {
        /* folha de receber: QR NÃO IMPLEMENTADO + simulações DEMO */
        tap_act(ACT_TAB, SC_MONEY);
        tap_act(ACT_SHEET, SH_RECEIVE);
        timed_draw(__LINE__);
        snprintf(path, sizeof path, "%s/dev-receber%s.png", outdir, modo);
        png_write(path);
        tap_act(ACT_CLOSE, 0);
        /* folha de escolher contato */
        tap_act(ACT_SHEET, SH_WHO);
        timed_draw(__LINE__);
        snprintf(path, sizeof path, "%s/dev-quem%s.png", outdir, modo);
        png_write(path);
        tap_act(ACT_CLOSE, 0);
    }

    /* LEIA-ME do diretório: diz QUEM renderizou, com QUE fonte e QUE código.
     * Sem isso uma captura de host pode ser confundida com foto do aparelho. */
    {
        char lp[512];
        snprintf(lp, sizeof lp, "%s/LEIA-ME.txt", outdir);
        FILE *f = fopen(lp, "w");
        if (f) {
            time_t t = time(NULL);
            struct tm *tm = gmtime(&t);
            fprintf(f, "BANKPHONE OS — capturas do HOST (nao sao fotos do aparelho)\n");
            fprintf(f, "geradas em %04d-%02d-%02d %02d:%02d UTC por src/init/host.c\n",
                    tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min);
            fprintf(f, "fonte usada: %s (%ld bytes) — no aparelho a fonte vai em assets/ do ramdisk\n", fonte, tn);
            fprintf(f, "modo: %s\n", amostra ? "--amostra (valores de AMOSTRA injetados para revisao de desenho)"
                                            : "descoberta real do Mac (quase tudo NOT AVAILABLE — de propósito)");
            fprintf(f, "telas: pv-*.png (entrega) · dev-*.png (conferencia: rolagem, folhas, aviso)\n");
            fprintf(f, "o codigo de desenho e' o MESMO do aparelho: ui.c, screens.c, sheets.c, components.c, icons.c\n");
            fclose(f);
            printf("  %s\n", lp);
        }
    }

    /* resumo do que o host realmente descobriu */
    printf("\nHARDWARE DESCOBERTO NO HOST (sem amostra):\n");
    {   /* re-descobre limpo para o relatório final */
        hw_init();
        printf("  bateria   : %s — %s\n", hw_state_str(HW.batt.st), HW.batt.why);
        printf("  brilho    : %s — %s\n", hw_state_str(HW.bl.st), HW.bl.why);
        printf("  vibração  : %s — %s\n", hw_state_str(HW.vib.st), HW.vib.why);
        printf("  rede      : %s — %s\n", hw_state_str(HW.net.st), HW.net.why);
    }
    return 0;
}
