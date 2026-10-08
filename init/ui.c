/*
 * BANKPHONE OS — ORQUESTRADOR DA INTERFACE.
 *
 * Responsabilidades (e só elas):
 *   1. estado de UI: pilha de telas, folha aberta, rolagem, teclado, haptics;
 *   2. registro de alvos de toque e AUDITORIA do mínimo de 44 dp;
 *   3. despacho de ações (nunca desenho à mão: quem desenha são screens/sheets);
 *   4. modo bloqueio (PIN) — é o único lugar que mexe no buffer do PIN;
 *   5. relógio REAL quando existe (NTP é F3; até lá, honestamente "--:--").
 *
 * Toda ação que toca em hardware chama o HardwareService (hw.c) e devolve ao
 * usuário o que realmente aconteceu (brilho lido de volta, vibração executada).
 */
#include "ui.h"
#include "nav.h"
#include "components.h"
#include "icons.h"
#include "money.h"
#include "sec.h"
#include "hw.h"
#include "bootdiag.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

/* ============================================================== estado ===== */

static int W, H;
static int locked = 1;
static int tab = SC_MONEY;                 /* aba ativa                     */
static int stack[8], sp;                   /* pilha de telas empilhadas      */
static int sheet = SH_NONE;
static int scroll[SC_N];                   /* rolagem guardada por tela      */
static int scr_total[SC_N];                /* altura total do conteúdo, do último desenho */
static int haptics = 1;
static UiDev DEV;

/* folha (formato no nav.h; o desenho está em sheets.c) */
SheetState SH;
const char *SHEET_CONTACTS[3] = { "Maria Souza", "João Lima", "Ana Costa" };

/* alvos de toque */
typedef struct { int x, y, w, h, act, arg; } Hit;
#define MAX_HITS 220
static Hit hits[MAX_HITS];
static int nh;
static int prev_total, prev_below, prev_min_dp;

/* toque em andamento */
static int d_x, d_y, d_on, d_moved, d_scroll0, d_scr0;

/* ============================================================== helpers === */

/* relógio REAL: sem RTC válido, a hora é DESCONHECIDA (nunca inventada). */
void ui_clock_str(char *o, size_t n)
{
    if (!plat_clock_valid()) { snprintf(o, n, "--:--"); return; }
    time_t t = (time_t)plat_now() - 3 * 3600;      /* America/Sao_Paulo (UTC-3) */
    struct tm *tm = gmtime(&t);
    snprintf(o, n, "%02d:%02d", tm->tm_hour, tm->tm_min);
}

/* estado da rede para a barra: só diz o nome quando há interface ACIMA.
 * Wi-Fi real é F3 — hoje, com interface mas sem IP, mostra SEM REDE. */
int ui_net_state(char *nome, size_t n)
{
    if (HW.net.st != HW_AVAILABLE || !HW.net_if[0] || !HW.net_up) return 0;
    snprintf(nome, n, "%s", HW.net_if);
    return 1;
}

int ui_haptics_on(void) { return haptics; }

int ui_hit_find(int a, int arg, int *cx, int *cy)
{
    for (int i = nh - 1; i >= 0; i--) {
        if (hits[i].act != a || hits[i].arg != arg) continue;
        if (cx) *cx = hits[i].x + hits[i].w / 2;
        if (cy) *cy = hits[i].y + hits[i].h / 2;
        return 1;
    }
    return 0;
}

int ui_hitcount(int *abaixo44, int *min_h_dp)
{
    if (abaixo44) *abaixo44 = prev_below;
    if (min_h_dp) *min_h_dp = prev_min_dp;
    return prev_total;
}

/* ============================================================ alvos ====== */

void ui_hit_add(int x, int y, int w, int h, int act, int arg)
{
    if (nh >= MAX_HITS) return;
    hits[nh].x = x; hits[nh].y = y; hits[nh].w = w; hits[nh].h = h;
    hits[nh].act = act; hits[nh].arg = arg;
    nh++;
}

static void audit_hits(void)
{
    int abaixo = 0, min_dp = 1 << 20;
    for (int i = 0; i < nh; i++) {
        int h_dp = hits[i].h / TH_DP(1);
        if (h_dp < min_dp) min_dp = h_dp;
        if (h_dp < TH_TOUCH_MIN_DP) abaixo++;
    }
    prev_total = nh;
    prev_below = abaixo;
    prev_min_dp = (nh ? min_dp : 0);
    if (abaixo && nh) {
        static int avisou;
        if (!avisou) { avisou = 1; bd_log("ui: %d alvo(s) abaixo de %d dp (auditoria do próprio quadro)", abaixo, TH_TOUCH_MIN_DP); bd_flush(); }
#ifdef BANKPHONE_UI_DEBUG_HITS
        for (int i = 0; i < nh; i++)
            if (hits[i].h / TH_DP(1) < TH_TOUCH_MIN_DP)
                fprintf(stderr, "[altura] act=%d arg=%d %dx%d px = %d dp\n", hits[i].act, hits[i].arg,
                        hits[i].w, hits[i].h, hits[i].h / TH_DP(1));
#endif
    }
}

/* ============================================================== estado ==== */

static int cur_screen(void) { return sp ? stack[sp - 1] : tab; }

int sheet_pend_tx(void) { return SH.pend; }
int sheet_rcpt_tx(void) { return SH.rcpt; }

int sheet_amt_value(int asset)
{
    int64_t d = parse_cents(SH.amt);
    if (d < 0) return 0;
    return (int)(asset == A_BRL ? d : d * 10000);
}

void sheet_amt_str(char *o, size_t n, int asset)
{
    int64_t v = sheet_amt_value(asset);
    if (!SH.amt[0]) { snprintf(o, n, asset == A_BRL ? "R$ 0,00" : "$ 0.00"); return; }
    fmt_money(o, n, asset, v);
}

/* ============================================================== bloqueio == */

static char pinbuf[8];
static char pin_first[8];
static int  pin_len;

static const char *lock_hint(void)
{
    static char b[64];
    int64_t ls = pin_locked_s(plat_now());
    if (!PIN.set) return pin_first[0] ? "Repita o PIN para confirmar" : "Crie um PIN de 6 dígitos";
    if (ls > 0) { snprintf(b, sizeof b, "Bloqueado. Tente de novo em %lld s", (long long)ls); return b; }
    return "Digite o PIN";
}

static void lock_draw(void)
{
    int y = TH_DP(TH_SP6) + TH_DP(TH_SP2);

    comp_badge(th_W / 2 - (comp_caps_w("DEMO") + TH_DP(TH_SP3) * 2) / 2, y, "DEMO", TH_DEMO);
    y += TH_DP(TH_SP6) + TH_DP(TH_SP5);

    char c[16];
    ui_clock_str(c, sizeof c);
    comp_num_c(th_W / 2, y, TH_DP(TH_F_XXL), c, plat_clock_valid() ? TH_TXT : TH_TXT3, TH_W_STRONG);
    y += TH_DP(TH_F_XXL) + TH_DP(TH_SP3);
    if (!plat_clock_valid()) comp_label_c(th_W / 2, y, "SEM RELÓGIO REAL (uptime interno)", TH_TXT3, TH_DP(TH_F_XS), TH_W_REG);

    y = th_H * 30 / 100;
    comp_label_c(th_W / 2, y, "BANKPHONE OS", TH_TXT2, TH_DP(TH_F_XS), TH_W_STRONG);
    y += TH_DP(TH_F_L);
    comp_label_c(th_W / 2, y, lock_hint(), pin_locked_s(plat_now()) > 0 ? TH_BAD : TH_TXT, TH_DP(TH_F_S), TH_W_REG);
    y += TH_DP(TH_F_M) + TH_DP(TH_SP5);
    comp_dots(th_W / 2, y, pin_len, 6);
    y += TH_DP(TH_SP6);
    if (PIN.fails && PIN.set && pin_locked_s(plat_now()) <= 0) {
        char f[48];
        snprintf(f, sizeof f, "%d tentativa(s) errada(s)", PIN.fails);
        comp_label_c(th_W / 2, y, f, TH_BAD, TH_DP(TH_F_XS), TH_W_REG);
    }
    y = th_H - TH_DP(TH_SP4) - (4 * TH_DP(TH_H_TECLA) + 3 * TH_DP(TH_SP3));
    comp_keypad(th_cx(), y, th_cw());

    /* aviso honesto do rodapé: o sistema todo é demonstração */
    comp_label_c(th_W / 2, th_H - TH_DP(TH_SP5), "nada aqui é dinheiro real", TH_TXT3, TH_DP(TH_F_CAPS), TH_W_REG);
}

/* A folha do PIN (sheets.c) desenha SH.pin_len, não o pin_len daqui. Sem esta
 * cópia, o teclado muda o buffer da tela de bloqueio e os pontinhos da folha de
 * autorização ficam parados em zero — e isso é o tipo de defeito que só aparece
 * no aparelho, na mão do usuário. Então: um lugar só decide, e o outro espelha. */
static void pin_len_spelhar(int n) { pin_len = n; SH.pin_len = n; }

static void lock_key(int k)
{
    if (k == '<') { if (pin_len) { pinbuf[pin_len - 1] = 0; pin_len_spelhar(pin_len - 1); } return; }
    if (pin_len >= 6) return;
    pinbuf[pin_len++] = (char)k;
    pinbuf[pin_len] = 0;
    pin_len_spelhar(pin_len);
    if (haptics) hw_vibrate(12);
    if (pin_len < 6) return;

    char p[8];
    snprintf(p, sizeof p, "%s", pinbuf);
    pinbuf[0] = 0;
    pin_len_spelhar(0);

    if (!PIN.set) {                                     /* primeiro uso: cria */
        if (!pin_first[0]) { snprintf(pin_first, sizeof pin_first, "%s", p); return; }
        if (strcmp(pin_first, p)) { pin_first[0] = 0; return; }
        uint8_t salt[16];
        plat_random(salt, 16);
        pin_set(p, salt);
        pin_first[0] = 0;
        plat_save();
        locked = 0;
        bd_log("seg: PIN definido e gravado (sal do kernel, SHA-256 iterado)");
        bd_flush();
        return;
    }
    int r = pin_check(p, plat_now());
    plat_save();
    if (r != 0) {
        SH.msg[0] = 0;
        bd_log("seg: PIN errado (tentativa %d)", PIN.fails);
        return;
    }
    if (locked) { locked = 0; return; }
    if (SH.purpose == 1) { M.emergency = 0; plat_save(); sheet = SH_NONE; SH.purpose = 0; return; }
    {
        int idx = SH.pend;
        if (idx < 0) { sheet = SH_NONE; return; }
        Tx *t = &M.tx[idx];
        Level need = m_required(t);
        if (need == L_PIN) {
            const char *e = m_authorize(t, L_PIN, plat_now());
            plat_save();
            if (t->st == S_AWAITING_AUTH) { snprintf(SH.msg, sizeof SH.msg, "%s", e ? e : "Não autorizado."); sheet = SH_NOTICE; return; }
            SH.rcpt = idx; SH.pend = -1; sheet = SH_RECEIPT;
        } else {
            sheet = SH_CONFIRM2;
        }
    }
}

/* ================================================================ ações == */

static void notice(const char *s)
{
    snprintf(SH.msg, sizeof SH.msg, "%s", s);
    sheet = SH_NOTICE;
    bd_log("ui: aviso — %s", s);
    bd_flush();
}

static void act(int a, int arg)
{
    const char *err = NULL;
    switch (a) {
    case ACT_KEY:
        if (locked || sheet == SH_PIN) { lock_key(arg); return; }
        if (sheet == SH_AMOUNT) {
            size_t n = strlen(SH.amt);
            if (arg == '<') { if (n) SH.amt[n - 1] = 0; return; }
            if (n >= 9) return;
            if (n == 0 && arg == '0') return;
            SH.amt[n] = (char)arg;
            SH.amt[n + 1] = 0;
        }
        return;

    case ACT_TAB:    tab = arg; sp = 0; sheet = SH_NONE; return;
    case ACT_PUSH:   if (sp < 8) stack[sp++] = arg; sheet = SH_NONE; return;
    case ACT_BACK:
        if (sheet != SH_NONE && sheet != SH_AMOUNT && sheet != SH_PIN) { sheet = SH_NONE; return; }
        if (sheet == SH_AMOUNT) { sheet = SH_WHO; return; }
        if (sheet == SH_PIN) { sheet = SH_REVIEW; return; }
        if (sp) sp--;
        return;
    case ACT_SHEET:
        if (arg == SH_AMOUNT) { SH.amt_mode = 1; SH.amt[0] = 0; }
        if (arg == SH_WHO) { SH.amt_mode = 0; SH.amt[0] = 0; }
        sheet = arg;
        return;
    case ACT_CLOSE:  sheet = SH_NONE; return;

    case ACT_WHO:    SH.who = arg; SH.amt[0] = 0; sheet = SH_AMOUNT; return;
    case ACT_FLIP:   SH.swap_from = SH.swap_from == A_BRL ? A_USDC : A_BRL; SH.amt[0] = 0; return;

    case ACT_LOADDEMO:
        m_load_demo(plat_now());
        plat_save();
        return;
    case ACT_DEMOPIX:
        m_receive_pix(arg, "Contato DEMO", plat_now());
        plat_save();
        sheet = SH_NONE;
        return;

    case ACT_CONT: {
        Tx *t = NULL;
        if (SH.amt_mode == 0) {
            t = m_prepare_pix(SHEET_CONTACTS[SH.who], sheet_amt_value(A_BRL), plat_now(), &err);
        } else {
            Quote q;
            int to = SH.swap_from == A_BRL ? A_USDC : A_BRL;
            if (!m_quote(SH.swap_from, to, sheet_amt_value(SH.swap_from), plat_now(), &q)) { notice("Cotação indisponível. Nada foi movido."); return; }
            t = m_prepare_swap(&q, plat_now(), &err);
        }
        if (!t) { notice(err ? err : "Não foi possível iniciar a transação."); return; }
        SH.pend = (int)(t - M.tx);
        sheet = SH_REVIEW;
        return; }
    case ACT_CANCEL:
        if (SH.pend >= 0) { m_cancel(&M.tx[SH.pend], plat_now()); SH.pend = -1; plat_save(); }
        sheet = SH_NONE;
        return;
    case ACT_AUTH:      SH.purpose = 0; SH.msg[0] = 0; pinbuf[0] = 0; pin_len_spelhar(0); sheet = SH_PIN; return;
    case ACT_CONFIRM2: {
        int idx = SH.pend;
        if (idx < 0) { sheet = SH_NONE; return; }
        Tx *t = &M.tx[idx];
        const char *e = m_authorize(t, m_required(t), plat_now());
        plat_save();
        if (t->st == S_AWAITING_AUTH) { notice(e ? e : "Não autorizado."); return; }
        SH.rcpt = idx; SH.pend = -1; sheet = SH_RECEIPT;
        return; }
    case ACT_TX:        SH.rcpt = arg; sheet = SH_RECEIPT; return;
    case ACT_EMERG:
        if (M.emergency) { SH.purpose = 1; SH.msg[0] = 0; pinbuf[0] = 0; pin_len_spelhar(0); sheet = SH_PIN; }
        else { M.emergency = 1; plat_save(); bd_log("seg: MODO EMERGÊNCIA ligado (trava Pix e troca) — gravado"); bd_flush(); }
        return;
    case ACT_LOCK:      ui_lock(); return;

    /* ---- hardware (F1): cada ação devolve o RESULTADO real ---- */
    case ACT_BRIGHT: {
        if (arg >= 0) { hw_brightness_set(arg); return; }
        /* arg < 0: usa a posição X do último toque dentro da barra */
        int r = -1;
        for (int i = nh - 1; i >= 0; i--)
            if (hits[i].act == ACT_BRIGHT) { r = i; break; }
        if (r < 0) return;
        int pct = d_x <= hits[r].x ? 0
                : d_x >= hits[r].x + hits[r].w ? 100
                : (d_x - hits[r].x) * 100 / hits[r].w;
        hw_brightness_set(pct);
        return; }
    case ACT_BRIGHT_STEP: {
        int p = hw_brightness_get();
        if (p < 0) p = 50;
        hw_brightness_set(p + arg);
        return; }
    case ACT_HAPTIC:    haptics = !haptics; bd_log("ui: vibração ao toque = %s", haptics ? "LIGADA" : "DESLIGADA"); bd_flush(); return;
    case ACT_VIB_TEST:  if (hw_vibrate(arg) == 0) bd_log("hw: vibração de teste %d ms executada (escrita no nó ok)", arg); else notice("Vibração indisponível neste aparelho/kernel."); return;
    case ACT_HW_REFRESH: (void)hw_tick(0); hw_report(); return;
    case ACT_NOTICE_TEST:
        notice("Teste de aviso: este é o caminho usado quando uma operação é recusada. Nada foi executado.");
        return;
    case ACT_BL_DISCOVER: {
        HW.bl.st = HW_UNKNOWN;
        snprintf(HW.bl.why, sizeof HW.bl.why, "redescobrindo a pedido do usuário");
        hw_init();
        bd_log("hw: redescoberta a pedido do usuário — brilho: %s", HW.bl.why);
        return; }
    default: return;
    }
}

/* ================================================================ desenho = */

static void present(void)
{
    int scr = cur_screen();
    int total = screens_draw(scr, scroll[scr]);
    scr_total[scr] = total;             /* fica sabendo para limitar o dedo  */

    /* limita a rolagem ao conteúdo real (rede de segurança: o limite normal já
     * acontece no arrasto, com o total do quadro anterior) */
    int view = th_bottom() - th_top();
    if (scroll[scr] > total - view) scroll[scr] = total - view > 0 ? total - view : 0;

    int is_tab = (sp == 0);
    comp_statusbar(is_tab ? "" : screen_title(scr), !is_tab, is_tab);
    if (is_tab) comp_navbar(tab);

    /* faixa de emergência acima da navegação */
    if (M.emergency) {
        int y = th_bottom() - TH_DP(TH_SP6);
        g_fill(0, y, W, TH_DP(TH_SP6), TH_BAD);
        comp_label_c(W / 2, y + TH_DP(TH_SP2), "MODO EMERGÊNCIA · OPERAÇÕES TRAVADAS", TH_BG, TH_DP(TH_F_CAPS), TH_W_STRONG);
    }

    if (sheet != SH_NONE) sheets_draw(sheet);

    /* primeiro uso: aviso único e honesto na tela de dinheiro */
    audit_hits();
}

/* ================================================================= API ==== */

void ui_init(int w, int h)
{
    W = w; H = h;
    th_init(w, h);
    m_init();
    memset(&PIN, 0, sizeof PIN);
    memset(&SH, 0, sizeof SH);
    SH.pend = -1; SH.rcpt = -1; SH.swap_from = A_BRL; SH.who = 0;
    sp = 0; tab = SC_MONEY; sheet = SH_NONE; locked = 1;
    memset(scroll, 0, sizeof scroll);
}

void ui_draw(void)
{
    /* UM único caminho de desenho: limpa, e ou desenha o bloqueio, ou as telas.
     * (Dois caminhos de apresentação já fizeram a tela de bloqueio ser apagada
     *  pelo outro; por isso existe só este.) */
    nh = 0;
    g_fill(0, 0, W, H, TH_BG);
    if (locked) { lock_draw(); audit_hits(); return; }
    present();
}

int ui_touch(int ev, int x, int y)
{
    if (ev == 0) { d_x = x; d_y = y; d_on = 1; d_moved = 0; d_scroll0 = scroll[cur_screen()]; d_scr0 = cur_screen(); return 0; }
    if (!d_on) return 0;
    if (ev == 1) {
        int dy = y - d_y;
        if (dy > TH_DP(TH_SP2) || dy < -TH_DP(TH_SP2)) d_moved = 1;
        if (d_moved && sheet == SH_NONE) {
            int scr = d_scr0;
            scroll[scr] = d_scroll0 - (y - d_y);
            if (scroll[scr] < 0) scroll[scr] = 0;
            {   /* não deixa arrastar para o vazio: o limite vem do conteúdo medido
                 * no último desenho desta tela (antes isto só era corrigido no
                 * quadro seguinte — um piscar em branco no fim da rolagem) */
                int view = th_bottom() - th_top();
                int maxs = scr_total[scr] - view;
                if (maxs < 0) maxs = 0;
                if (scroll[scr] > maxs) scroll[scr] = maxs;
            }
            return 1;
        }
        return 0;
    }
    /* UP: só dispara se o dedo soltou perto de onde apertou e não arrastou */
    d_on = 0;
    if (d_moved) return 0;
    for (int i = nh - 1; i >= 0; i--) {
        Hit *h = &hits[i];
        if (x >= h->x && x < h->x + h->w && y >= h->y && y < h->y + h->h) {
            d_x = x; d_y = y;                       /* para a barra de brilho */
            act(h->act, h->arg);
            return 1;
        }
    }
    return 0;
}

int ui_tick(void)
{
    /* o relógio muda a cada segundo; contadores de toque idem */
    if (locked) return 1;
    return 1;
}

void ui_lock(void)
{
    locked = 1;
    sheet = SH_NONE;
    sp = 0;
    pinbuf[0] = 0; pin_len_spelhar(0); pin_first[0] = 0;
    if (SH.pend >= 0) { m_cancel(&M.tx[SH.pend], plat_now()); SH.pend = -1; plat_save(); }
    bd_log("ui: tela bloqueada");
}

void ui_set_dev(const UiDev *d)
{
    if (d) DEV = *d;
}

const UiDev *ui_dev(void) { return &DEV; }
