/* BANKPHONE OS — implementação dos ícones (primitivas vetoriais). */
#include "icons.h"
#include "gfx.h"
#include <math.h>

static void arc(int cx, int cy, int r, float a0, float a1, float bw, uint32_t c)
{
    int n = 24;
    float px = 0, py = 0;
    for (int i = 0; i <= n; i++) {
        float a = a0 + (a1 - a0) * (float)i / (float)n;
        float x = cx + r * cosf(a), y = cy + r * sinf(a);
        if (i) g_line(px, py, x, y, bw, c);
        px = x; py = y;
    }
}

void icon(int id, int cx, int cy, int size, uint32_t color)
{
    const float u = size / 24.0f;          /* 1 unidade da grade de 24 dp */
    float bw = 1.75f * u;
    if (bw < 1.2f) bw = 1.2f;
    const uint32_t c = color;

    switch (id) {
    case IC_SEND: {                        /* seta saindo: ↗ com base */
        g_line(cx - 6 * u, cy + 6 * u, cx - 6 * u, cy - 1 * u, bw, c);
        g_line(cx - 6 * u, cy + 6 * u, cx + 1 * u, cy + 6 * u, bw, c);
        g_line(cx - 5 * u, cy + 5 * u, cx + 7 * u, cy - 7 * u, bw, c);
        g_line(cx + 1 * u, cy - 7 * u, cx + 7 * u, cy - 7 * u, bw, c);
        g_line(cx + 7 * u, cy - 7 * u, cx + 7 * u, cy - 1 * u, bw, c);
        break; }
    case IC_RECEIVE: {                     /* ↘ chegando com base */
        g_line(cx - 7 * u, cy - 6 * u, cx - 7 * u, cy + 1 * u, bw, c);
        g_line(cx - 7 * u, cy - 6 * u, cx - 1 * u, cy - 6 * u, bw, c);
        g_line(cx - 6 * u, cy - 5 * u, cx + 6 * u, cy + 7 * u, bw, c);
        g_line(cx, cy + 7 * u, cx + 6 * u, cy + 7 * u, bw, c);
        g_line(cx + 6 * u, cy + 7 * u, cx + 6 * u, cy + 1 * u, bw, c);
        break; }
    case IC_SWAP: {                        /* duas setas opostas */
        g_line(cx - 7 * u, cy - 3 * u, cx + 6 * u, cy - 3 * u, bw, c);
        g_line(cx + 2 * u, cy - 7 * u, cx + 6.5f * u, cy - 3 * u, bw, c);
        g_line(cx + 2 * u, cy + 1 * u, cx + 6.5f * u, cy - 3 * u, bw, c);
        g_line(cx + 7 * u, cy + 4 * u, cx - 6 * u, cy + 4 * u, bw, c);
        g_line(cx - 2 * u, cy + 0.5f * u, cx - 6.5f * u, cy + 4 * u, bw, c);
        g_line(cx - 2 * u, cy + 7.5f * u, cx - 6.5f * u, cy + 4 * u, bw, c);
        break; }
    case IC_WALLET: {                      /* carteira: corpo + aba */
        float w = 15 * u, h = 11 * u;
        g_rring((int)(cx - w / 2), (int)(cy - h / 2 + 1 * u), (int)w, (int)h, (int)(2 * u), (int)(bw + 0.5f), c);
        g_line(cx - w / 2, cy - h / 2 + 3.5f * u, cx + w / 2, cy - h / 2 + 3.5f * u, bw - 0.4f, c);
        g_disc((int)(cx + w / 2 - 3.5f * u), (int)(cy + 1.5f * u), (int)(1.5f * u), c);
        break; }
    case IC_SECURITY: {                    /* escudo */
        g_line(cx, cy - 9 * u, cx - 7 * u, cy - 5.5f * u, bw, c);
        g_line(cx, cy - 9 * u, cx + 7 * u, cy - 5.5f * u, bw, c);
        g_line(cx - 7 * u, cy - 5.5f * u, cx - 7 * u, cy + 1 * u, bw, c);
        g_line(cx + 7 * u, cy - 5.5f * u, cx + 7 * u, cy + 1 * u, bw, c);
        g_line(cx - 7 * u, cy + 1 * u, cx, cy + 9 * u, bw, c);
        g_line(cx + 7 * u, cy + 1 * u, cx, cy + 9 * u, bw, c);
        break; }
    case IC_SETTINGS: {                    /* engrenagem simplificada: 3 trilhos */
        arc(cx, cy, 7.5f * u, 0, 6.2832f, bw, c);
        g_disc(cx, cy, (int)(2.2f * u), c);
        for (int i = 0; i < 6; i++) {
            float a = (float)i * 1.0472f;
            g_line(cx + 7.5f * u * cosf(a), cy + 7.5f * u * sinf(a),
                   cx + 10.5f * u * cosf(a), cy + 10.5f * u * sinf(a), bw, c);
        }
        break; }
    case IC_BACK: {
        g_line(cx + 6 * u, cy - 8 * u, cx - 4 * u, cy, bw, c);
        g_line(cx - 4 * u, cy, cx + 6 * u, cy + 8 * u, bw, c);
        g_line(cx - 4 * u, cy, cx + 9 * u, cy, bw, c);
        break; }
    case IC_CHEVRON: {
        g_line(cx - 4 * u, cy - 7 * u, cx + 4 * u, cy, bw, c);
        g_line(cx + 4 * u, cy, cx - 4 * u, cy + 7 * u, bw, c);
        break; }
    case IC_CLOSE: {
        g_line(cx - 7 * u, cy - 7 * u, cx + 7 * u, cy + 7 * u, bw, c);
        g_line(cx + 7 * u, cy - 7 * u, cx - 7 * u, cy + 7 * u, bw, c);
        break; }
    case IC_CHECK: {
        g_line(cx - 7 * u, cy + 1 * u, cx - 2 * u, cy + 6 * u, bw, c);
        g_line(cx - 2 * u, cy + 6 * u, cx + 8 * u, cy - 6 * u, bw, c);
        break; }
    case IC_WARNING: {
        g_line(cx, cy - 9 * u, cx - 9 * u, cy + 7 * u, bw, c);
        g_line(cx, cy - 9 * u, cx + 9 * u, cy + 7 * u, bw, c);
        g_line(cx - 9 * u, cy + 7 * u, cx + 9 * u, cy + 7 * u, bw, c);
        g_line(cx, cy - 4 * u, cx, cy + 1.5f * u, bw, c);
        g_disc(cx, (int)(cy + 4.5f * u), (int)(1.2f * u), c);
        break; }
    case IC_LOCK: {
        g_rrect((int)(cx - 7 * u), (int)(cy - 1 * u), (int)(14 * u), (int)(10 * u), (int)(2 * u), c, 255);
        arc(cx, cy - 3 * u, 4.5f * u, 3.1416f, 6.2832f, bw, c);
        break; }
    case IC_WIFI: {
        arc(cx, cy + 6 * u, 8 * u, 3.9f, 5.5f, bw, c);
        arc(cx, cy + 6 * u, 4.6f * u, 3.9f, 5.5f, bw, c);
        g_disc(cx, (int)(cy + 6 * u), (int)(1.3f * u), c);
        break; }
    case IC_BATTERY: {
        int bwid = (int)(17 * u), bh = (int)(10 * u);
        g_rring((int)(cx - bwid / 2 - 1 * u), (int)(cy - bh / 2), bwid, bh, (int)(2 * u), (int)(bw + 0.4f), c);
        g_rrect((int)(cx + bwid / 2 - 0.5f * u), (int)(cy - 2 * u), (int)(2.5f * u), (int)(4 * u), (int)(1 * u), c, 255);
        break; }
    case IC_CLOCK: {
        arc(cx, cy, 8.5f * u, 0, 6.2832f, bw, c);
        g_line(cx, cy - 4.5f * u, cx, cy, bw, c);
        g_line(cx, cy, cx + 4 * u, cy + 2 * u, bw, c);
        break; }
    case IC_PLUS: {
        g_line(cx - 7 * u, cy, cx + 7 * u, cy, bw, c);
        g_line(cx, cy - 7 * u, cx, cy + 7 * u, bw, c);
        break; }
    case IC_INFO: {
        arc(cx, cy, 8.5f * u, 0, 6.2832f, bw, c);
        g_line(cx, cy - 4 * u, cx, cy + 1 * u, bw, c);
        g_disc(cx, (int)(cy + 4 * u), (int)(1.2f * u), c);
        break; }
    default: break;
    }
}

const char *icon_name(int id)
{
    static const char *N[] = { "send", "receive", "swap", "wallet", "security", "settings",
                               "back", "close", "check", "warning", "lock", "wifi", "battery",
                               "clock", "chevron", "plus", "info" };
    return (id >= 0 && id < IC_N) ? N[id] : "?";
}
