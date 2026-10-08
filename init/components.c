/* BANKPHONE OS — implementação dos componentes. Só tokens do theme.h. */
#include "components.h"
#include "icons.h"
#include "nav.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>

/* ---- variáveis globais do tema (o resto do sistema só lê) ---- */
int th_W, th_H, th_SC;

/* ================================================================== texto == */

int comp_caps_w(const char *s) { return th_text_w(TH_DP(TH_F_CAPS), s, TH_DP(TH_TR_CAPS), TH_W_REG); }

void comp_caps(int x, int y, const char *s, uint32_t color)
{
    th_text(x, y, TH_DP(TH_F_CAPS), s, color, TH_DP(TH_TR_CAPS), TH_W_REG);
}

/* Rótulo curto que não pode invadir um selo ao lado (ex.: DEMO no cabeçalho). */
void comp_caps_clip(int x, int y, int maxw, const char *s, uint32_t color)
{
    int px = TH_DP(TH_F_CAPS);
    char b[96];
    snprintf(b, sizeof b, "%.80s", s);
    if (th_text_w(px, b, TH_DP(TH_TR_CAPS), TH_W_REG) <= maxw) { comp_caps(x, y, b, color); return; }
    size_t n = strlen(b);
    while (n > 1) {
        b[--n] = 0;
        char t[112];
        snprintf(t, sizeof t, "%s\xE2\x80\xA6", b);
        if (th_text_w(px, t, TH_DP(TH_TR_CAPS), TH_W_REG) <= maxw) { comp_caps(x, y, t, color); return; }
    }
}

void comp_label(int x, int y, const char *s, uint32_t color, int px, ThWeight w)
{
    th_text(x, y, px, s, color, 0, w);
}
void comp_label_r(int xr, int y, const char *s, uint32_t color, int px, ThWeight w)
{
    th_text_r(xr, y, px, s, color, 0, w);
}
void comp_label_c(int cx, int y, const char *s, uint32_t color, int px, ThWeight w)
{
    th_text_c(cx, y, px, s, color, 0, w);
}

/* Recorte com reticências: mede e corta antes de desenhar, para que um valor
 * longo nunca passe por cima do rótulo ao lado. */
void comp_label_clip(int x, int y, int maxw, const char *s, uint32_t color, int px, ThWeight w)
{
    char b[192];
    snprintf(b, sizeof b, "%.160s", s);
    if (th_text_w(px, b, 0, w) <= maxw) { th_text(x, y, px, b, color, 0, w); return; }
    size_t n = strlen(b);
    while (n > 1) {
        b[--n] = 0;
        char t[200];
        snprintf(t, sizeof t, "%s\xE2\x80\xA6", b);      /* … em UTF-8 */
        if (th_text_w(px, t, 0, w) <= maxw) { th_text(x, y, px, t, color, 0, w); return; }
    }
    th_text(x, y, px, "", color, 0, w);
}
void comp_label_r_clip(int xr, int y, int maxw, const char *s, uint32_t color, int px, ThWeight w)
{
    char b[192];
    snprintf(b, sizeof b, "%.160s", s);
    if (th_text_w(px, b, 0, w) <= maxw) { th_text_r(xr, y, px, b, color, 0, w); return; }
    size_t n = strlen(b);
    while (n > 1) {
        b[--n] = 0;
        char t[200];
        snprintf(t, sizeof t, "\xE2\x80\xA6%s", b);
        if (th_text_w(px, t, 0, w) <= maxw) { th_text_r(xr, y, px, t, color, 0, w); return; }
    }
    th_text_r(xr, y, px, "", color, 0, w);
}

/* A largura de referência dos dígitos é o avanço do '0' nesta fonte/tamanho. */
static int digit_adv(int px, int *campo)
{
    if (!*campo) *campo = g_text_w(px, "0", 0);
    return *campo;
}

int comp_num_w(int px, const char *s, ThWeight w)
{
    int campo = 0, total = 0;
    for (const char *p = s; *p; p++) {
        if (*p >= '0' && *p <= '9') total += digit_adv(px, &campo) + TH_DP(TH_TR_NUM);
        else total += g_text_w(px, (char[2]){ *p, 0 }, 0);
    }
    return w >= TH_W_STRONG ? total + 1 : total;
}

void comp_num(int x, int y, int px, const char *s, uint32_t color, ThWeight w)
{
    int campo = 0;
    for (const char *p = s; *p; p++) {
        char g[2] = { *p, 0 };
        if (*p >= '0' && *p <= '9') {
            int adv = digit_adv(px, &campo) + TH_DP(TH_TR_NUM);
            th_text(x, y, px, g, color, 0, w);
            x += adv;
        } else {
            th_text(x, y, px, g, color, 0, w);
            x += g_text_w(px, g, 0);
        }
    }
}
void comp_num_r(int xr, int y, int px, const char *s, uint32_t color, ThWeight w)
{
    comp_num(xr - comp_num_w(px, s, w), y, px, s, color, w);
}
void comp_num_c(int cx, int y, int px, const char *s, uint32_t color, ThWeight w)
{
    comp_num(cx - comp_num_w(px, s, w) / 2, y, px, s, color, w);
}

int comp_wrap(int x, int y, int w, int px, const char *s, uint32_t color)
{
    char linha[256] = "", buf[512];
    int ll = 0, cy = y, lh = px * 14 / 10;
    snprintf(buf, sizeof buf, "%s", s);
    for (char *t = strtok(buf, " "); t; t = strtok(NULL, " ")) {
        char tent[320];
        snprintf(tent, sizeof tent, "%s%s%s", linha, ll ? " " : "", t);
        if (ll && th_text_w(px, tent, 0, TH_W_REG) > w) {
            th_text(x, cy, px, linha, color, 0, TH_W_REG);
            cy += lh;
            snprintf(linha, sizeof linha, "%s", t);
        } else {
            snprintf(linha, sizeof linha, "%.*s", (int)sizeof linha - 1, tent);
        }
        ll = 1;
    }
    if (ll) { th_text(x, cy, px, linha, color, 0, TH_W_REG); cy += lh; }
    return cy - y;
}

/* =================================================================== selo == */

int comp_badge(int x, int y, const char *text, uint32_t color)
{
    int px = TH_DP(TH_F_CAPS);
    int w = th_text_w(px, text, TH_DP(2), TH_W_STRONG) + TH_DP(TH_SP3) * 2;
    int h = TH_DP(TH_SP4) + TH_DP(TH_SP1);   /* altura da linha de título: o selo não invade o conteúdo de baixo */
    g_rring(x, y, w, h, TH_DP(TH_RAIO_S), TH_DP(TH_BORDA), color);
    th_text(x + TH_DP(TH_SP3), y + (h - px) / 2 - TH_DP(1), px, text, color, TH_DP(2), TH_W_STRONG);
    return w;
}

/* ================================================================= botões == */

int comp_button(int x, int y, int w, int h, const char *label, int icon_id,
                BtnKind kind, int act, int arg, int on, int dim)
{
    uint32_t bg = TH_SURF2, fg = TH_TXT, borda = TH_LINE;
    switch (kind) {
    case BK_PRIMARY: bg = dim ? TH_SURF2 : TH_ACC; fg = dim ? TH_TXT3 : TH_BG; borda = 0; break;
    case BK_NEUTRAL: bg = on ? TH_SURF3 : TH_SURF2; fg = dim ? TH_TXT3 : TH_TXT; break;
    case BK_DANGER:  bg = TH_SURF2; fg = dim ? TH_TXT3 : TH_BAD; borda = TH_BAD; break;
    case BK_GHOST:   bg = 0; fg = dim ? TH_TXT3 : (on ? TH_ACC : TH_TXT2); borda = 0; break;
    case BK_ICON:    bg = on ? TH_SURF3 : TH_SURF2; fg = dim ? TH_TXT3 : TH_TXT; break;
    }

    int px = TH_DP(TH_F_S);
    int tw = label ? th_text_w(px, label, TH_DP(1), TH_W_STRONG) : 0;
    int iw = icon_id >= 0 ? TH_DP(20) : 0;
    int gap = (label && icon_id >= 0) ? TH_DP(TH_SP3) : 0;
    int cx = x + w / 2, cy = y + h / 2;

    if (bg) g_rrect(x, y, w, h, TH_DP(TH_RAIO), bg, 255);
    if (borda) g_rring(x, y, w - 1, h - 1, TH_DP(TH_RAIO), TH_DP(TH_BORDA), borda);

    int start = cx - (tw + gap + iw) / 2;
    if (icon_id >= 0) icon(icon_id, start + iw / 2, cy, iw, fg);
    if (label) th_text(start + iw + gap, cy - px / 2 - TH_DP(1), px, label, fg, TH_DP(1), TH_W_STRONG);

    if (act != ACT_NONE) ui_hit_add(x, y, w, h, act, arg);
    return h;
}

/* ================================================================= cartão == */

int comp_card_begin(int x, int y, int w, int h, const char *caps_label, int act, int arg)
{
    g_rrect(x, y, w, h, TH_DP(TH_RAIO), TH_SURF, 255);
    g_rring(x, y, w - 1, h - 1, TH_DP(TH_RAIO), TH_DP(TH_BORDA), TH_LINE);
    int cy = y + TH_DP(TH_SP4);
    if (caps_label) {
        comp_caps(x + TH_DP(TH_SP4), cy, caps_label, TH_TXT3);
        cy += TH_DP(TH_F_CAPS) + TH_DP(TH_SP4);
    }
    if (act != ACT_NONE) ui_hit_add(x, y, w, h, act, arg);
    return cy;
}

void comp_divider(int x, int y, int w) { g_fill(x, y, w, 1, TH_LINE); }

int comp_row_kv(int x, int y, int w, const char *label, const char *value,
                uint32_t vcolor, int px, ThWeight wgt)
{
    int lw = th_text_w(px, label, 0, TH_W_REG) + TH_DP(TH_SP3);
    th_text(x, y, px, label, TH_TXT2, 0, TH_W_REG);
    comp_label_r_clip(x + w, y, w - lw, value, vcolor, px, wgt);
    return px;
}

int comp_row_list(int x, int y, int w, int icon_id, const char *label,
                  const char *value, uint32_t vcolor, int act, int arg, int disabled)
{
    int h = TH_DP(TH_H_LISTA);
    int px = TH_DP(TH_F_S);
    int tx = x + TH_DP(TH_SP4);
    if (icon_id >= 0) { icon(icon_id, tx + TH_DP(9), y + h / 2, TH_DP(20), disabled ? TH_TXT3 : TH_TXT2); tx += TH_DP(TH_SP5) + TH_DP(TH_SP1); }
    th_text(tx, y + (h - px) / 2 - TH_DP(1), px, label, disabled ? TH_TXT3 : TH_TXT, 0, TH_W_REG);
    if (value) {
        int sx = x + w - TH_DP(TH_SP4) - (act != ACT_NONE ? TH_DP(TH_SP4) : 0);
        int lw = th_text_w(px, label, 0, TH_W_REG) + TH_DP(TH_SP4);
        comp_label_r_clip(sx, y + (h - px) / 2 - TH_DP(1), w - lw, value, vcolor, px, TH_W_REG);
    }
    if (act != ACT_NONE) {
        icon(IC_CHEVRON, x + w - TH_DP(TH_SP3) - TH_DP(4), y + h / 2, TH_DP(14), disabled ? TH_TXT3 : TH_TXT3);
        if (!disabled) ui_hit_add(x, y, w, h, act, arg);
    }
    g_fill(x, y + h - 1, w, 1, TH_LINE);
    return h;
}

/* =============================================================== destaque == */

int comp_balance(int x, int y, const char *asset, const char *value, uint32_t color, int px)
{
    int cy = y;
    if (asset) { comp_caps(x, cy, asset, TH_TXT3); cy += TH_DP(TH_F_CAPS) + TH_DP(TH_SP2); }
    comp_num(x, cy, px, value, color, TH_W_STRONG);
    return cy + px - y;
}

int comp_tx_row(int x, int y, int w, const char *title, const char *sub, const char *when,
                const char *amount, uint32_t acolor, int act, int arg)
{
    int h = TH_DP(TH_H_LISTA);
    int px = TH_DP(TH_F_M);
    int amt_w = amount ? (comp_num_w(px, amount, TH_W_STRONG) + TH_DP(TH_SP3)) : 0;
    comp_label_clip(x, y + TH_DP(TH_SP1), w - amt_w, title, TH_TXT, px, TH_W_REG);
    /* linha de apoio: valor em cinza e selo de ambiente em âmbar, sempre juntos */
    {
        int y2 = y + TH_DP(TH_SP1) + px + TH_DP(TH_SP1);
        int sx = x;
        sx += th_text_w(TH_DP(TH_F_XS), sub, 0, TH_W_REG) + TH_DP(TH_SP2);
        th_text(x, y2, TH_DP(TH_F_XS), sub, TH_TXT3, 0, TH_W_REG);
        g_disc(x + th_text_w(TH_DP(TH_F_XS), sub, 0, TH_W_REG) + TH_DP(TH_SP1), y2 + TH_DP(TH_F_XS) / 2 - TH_DP(1),
               TH_DP(1), TH_TXT3);
        th_text(sx, y2, TH_DP(TH_F_XS), when && when[0] ? when : "", TH_TXT3, 0, TH_W_REG);
    }
    if (amount) comp_num_r(x + w, y + TH_DP(TH_SP1) + TH_DP(3), px, amount, acolor, TH_W_STRONG);
    if (act != ACT_NONE) ui_hit_add(x, y, w, h, act, arg);
    g_fill(x, y + h - 1, w, 1, TH_LINE);
    return h;
}

/* ============================================================ barra/nav ==== */

#include "hw.h"   /* a barra de status mostra dado REAL: sem leitura, aparece "--" */

int comp_statusbar(const char *screen_title, int show_back, int show_gear)
{
    int h = th_top();
    g_fill(0, 0, th_W, h, TH_BG);
    int px = TH_DP(TH_F_S);
    int cy = (h - px) / 2;
    int x = TH_DP(TH_MARGIN);

    if (show_back) {
        /* Alvo de 44 dp mesmo com desenho de 24: o toque tem folga, o desenho não
         * cresce. É o padrão de sistema e está registrado no relatório. */
        int bh = TH_DP(TH_TOUCH_MIN_DP);
        icon(IC_BACK, x + TH_DP(14), h / 2, TH_DP(20), TH_TXT);
        ui_hit_add(x - TH_DP(6), (h - bh) / 2, TH_DP(TH_TOUCH_MIN_DP), bh, ACT_BACK, 0);
        x += TH_DP(TH_TOUCH_MIN_DP);
    }

    char hora[16];
    ui_clock_str(hora, sizeof hora);
    th_text(x, cy - TH_DP(1), px, hora, TH_TXT, 0, TH_W_STRONG);
    int x2 = x + th_text_w(px, hora, 0, TH_W_STRONG) + TH_DP(TH_SP2);
    if (screen_title && screen_title[0]) {
        g_fill(x2, cy + TH_DP(2), 1, px - TH_DP(4), TH_LINE_S);
        th_text(x2 + TH_DP(TH_SP2), cy - TH_DP(1), px, screen_title, TH_TXT3, 0, TH_W_REG);
    }

    /* direita: engrenagem, bateria, rede, selo de ambiente */
    int rx = th_W - TH_DP(TH_MARGIN);
    if (show_gear) {
        int bh = TH_DP(TH_TOUCH_MIN_DP);
        icon(IC_SETTINGS, rx - TH_DP(14), h / 2, TH_DP(20), TH_TXT2);
        ui_hit_add(rx - TH_DP(TH_TOUCH_MIN_DP) + TH_DP(8), (h - bh) / 2, TH_DP(TH_TOUCH_MIN_DP), bh,
                   ACT_PUSH, SC_SETTINGS);
        rx -= TH_DP(TH_TOUCH_MIN_DP) + TH_DP(TH_SP2);
    }
    {
        const char *b = hw_batt_pct_str();
        int bw2 = th_text_w(px, b, 0, TH_W_REG);
        icon(IC_BATTERY, rx - bw2 - TH_DP(TH_SP3) - TH_DP(10), h / 2, TH_DP(20), TH_TXT2);
        th_text_r(rx, cy - TH_DP(1), px, b, TH_TXT2, 0, TH_W_REG);
        rx -= bw2 + TH_DP(TH_SP3) + TH_DP(TH_SP5) + TH_DP(TH_SP2);
    }
    {
        char net[24];   /* 0 = sem rede real */
        if (ui_net_state(net, sizeof net)) {
            icon(IC_WIFI, rx - TH_DP(TH_SP5) - TH_DP(10), h / 2, TH_DP(18), TH_OK);
            th_text_r(rx, cy - TH_DP(1), px, net, TH_OK, 0, TH_W_REG);
        } else {
            th_text_r(rx, cy - TH_DP(1), px, "SEM REDE", TH_TXT3, 0, TH_W_REG);
        }
    }
    g_fill(0, h - 1, th_W, 1, TH_LINE);
    return h;
}

int comp_env_badge(int x, int y) { return comp_badge(x, y, "DEMO", TH_DEMO); }

int comp_navbar(int active_tab)
{
    int h = TH_DP(TH_H_NAV), y = th_H - h;
    static const struct { const char *label; int ic; } T[4] = {
        { "MONEY",    IC_WALLET },
        { "ACTIVITY", IC_CLOCK },
        { "WALLET",   IC_SEND },
        { "SECURITY", IC_SECURITY },
    };
    g_fill(0, y, th_W, h, TH_SURF);
    g_fill(0, y, th_W, 1, TH_LINE);
    for (int i = 0; i < 4; i++) {
        int w = th_W / 4, x = i * w;
        int on = (i == active_tab);
        uint32_t c = on ? TH_ACC : TH_TXT3;
        icon(T[i].ic, x + w / 2, y + TH_DP(24), TH_DP(22), c);
        th_text_c(x + w / 2, y + TH_DP(44), TH_DP(TH_F_CAPS), T[i].label, c, TH_DP(1), on ? TH_W_STRONG : TH_W_REG);
        if (on) g_fill(x + w / 2 - TH_DP(18), y, TH_DP(36), TH_DP(3), TH_ACC);
        ui_hit_add(x, y, w, h, ACT_TAB, i);
    }
    return h;
}

/* ================================================================ teclado == */

int comp_keypad(int x, int y, int w)
{
    int kw = (w - 2 * TH_DP(TH_SP3)) / 3, kh = TH_DP(TH_H_TECLA), g = TH_DP(TH_SP3);
    static const char *L[12] = { "1","2","3","4","5","6","7","8","9","",  "0","<" };
    for (int i = 0; i < 12; i++) {
        int cx = x + (i % 3) * (kw + g), cy = y + (i / 3) * (kh + g);
        if (!L[i][0]) continue;
        g_rrect(cx, cy, kw, kh, TH_DP(TH_RAIO), TH_SURF2, 255);
        if (L[i][0] == '<') icon(IC_BACK, cx + kw / 2, cy + kh / 2, TH_DP(24), TH_TXT);
        else comp_label_c(cx + kw / 2, cy + (kh - TH_DP(34)) / 2, L[i], TH_TXT, TH_DP(34), TH_W_REG);
        ui_hit_add(cx, cy, kw, kh, ACT_KEY, L[i][0]);
    }
    return 4 * kh + 3 * g;
}

void comp_dots(int cx, int y, int n, int total)
{
    int g = TH_DP(TH_SP6), r = TH_DP(7);
    int x0 = cx - (total - 1) * g / 2;
    for (int i = 0; i < total; i++) {
        if (i < n) g_disc(x0 + i * g, y, r, TH_ACC);
        else g_rring(x0 + i * g - r, y - r, r * 2, r * 2, r, TH_DP(2), TH_TXT3);
    }
}

/* ================================================================= folha === */

int comp_sheet_begin(int sheet_h, int *out_x, int *out_w)
{
    g_rrect(0, 0, th_W, th_H, 0, 0x000000, 200);
    ui_hit_add(0, 0, th_W, th_H - sheet_h - TH_DP(TH_SP4), ACT_CLOSE, 0);   /* tocar fora fecha */
    int x = TH_DP(TH_SP2), w = th_W - 2 * TH_DP(TH_SP2);
    int y = th_H - sheet_h - TH_DP(TH_SP2);
    g_rrect(x, y, w, sheet_h, TH_DP(TH_RAIO) + TH_DP(2), TH_SURF, 255);
    g_rring(x, y, w - 1, sheet_h - 1, TH_DP(TH_RAIO) + TH_DP(2), TH_DP(TH_BORDA), TH_LINE_S);
    /* alça: pista visual de que é uma folha */
    g_rrect(th_W / 2 - TH_DP(18), y + TH_DP(TH_SP2), TH_DP(36), TH_DP(3), TH_DP(2), TH_LINE_S, 255);
    if (out_x) *out_x = x + TH_DP(TH_SP5);
    if (out_w) *out_w = w - 2 * TH_DP(TH_SP5);
    return y + TH_DP(TH_SP6);
}

void comp_sheet_title(int x, int y, const char *title, const char *badge_text)
{
    comp_caps(x, y, title, TH_TXT3);
    if (badge_text) {
        int bw = th_text_w(TH_DP(TH_F_CAPS), badge_text, TH_DP(2), TH_W_STRONG) + TH_DP(TH_SP3) * 2;
        comp_badge(th_W - TH_DP(TH_SP5) - bw, y - TH_DP(TH_SP1), badge_text, TH_DEMO);
    }
}
