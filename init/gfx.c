#include "gfx.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static Surf S;
static stbtt_fontinfo font;

void gfx_set(Surf s) { S = s; }
int gfx_init(const unsigned char *ttf) { return stbtt_InitFont(&font, ttf, stbtt_GetFontOffsetForIndex(ttf, 0)) ? 0 : -1; }

static inline uint32_t mix(uint32_t bg, uint32_t fg, int a) {
    if (a <= 0) return bg; if (a >= 255) return fg;
    uint32_t k = (uint32_t)a + ((uint32_t)a >> 7);          /* 0..256 */
    uint32_t rb = ((fg & 0x00FF00FFu) * k + (bg & 0x00FF00FFu) * (256 - k)) >> 8;
    uint32_t g  = (((fg >> 8) & 0xFFu) * k + ((bg >> 8) & 0xFFu) * (256 - k)) >> 8;
    return (rb & 0x00FF00FFu) | (g << 8);
}
static inline void put(int x, int y, uint32_t c, int a) {
    if (x < 0 || y < 0 || x >= S.w || y >= S.h) return;
    uint32_t *d = &S.p[(size_t)y * S.stride + x]; *d = mix(*d, c, a);
}


/* faixa horizontal [x0,x1) na linha y: alpha constante (0..255). Sólido vira atribuição direta. */
static inline void span(int y, int x0, int x1, uint32_t c, int a) {
    if (y < 0 || y >= S.h || a <= 0) return;
    if (x0 < 0) x0 = 0; if (x1 > S.w) x1 = S.w; if (x0 >= x1) return;
    uint32_t *d = &S.p[(size_t)y * S.stride];
    if (a >= 255) { for (int i = x0; i < x1; i++) d[i] = c; return; }
    uint32_t k = (uint32_t)a + ((uint32_t)a >> 7);
    uint32_t fr = (c & 0x00FF00FFu) * k, fgx = ((c >> 8) & 0xFFu) * k, ik = 256 - k;
    for (int i = x0; i < x1; i++) {
        uint32_t bg = d[i];
        uint32_t rb = (fr + (bg & 0x00FF00FFu) * ik) >> 8, g = (fgx + ((bg >> 8) & 0xFFu) * ik) >> 8;
        d[i] = (rb & 0x00FF00FFu) | (g << 8);
    }
}

void g_fill(int x, int y, int w, int h, uint32_t c) {
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y, x1 = x + w > S.w ? S.w : x + w, y1 = y + h > S.h ? S.h : y + h;
    for (int j = y0; j < y1; j++) { uint32_t *d = &S.p[(size_t)j * S.stride]; for (int i = x0; i < x1; i++) d[i] = c; }
}

// distância assinada a um retângulo arredondado centrado em (cx,cy) com meia-largura hw, meia-altura hh, raio r
static inline float sdbox(float px, float py, float hw, float hh, float r) {
    float qx = fabsf(px) - (hw - r), qy = fabsf(py) - (hh - r);
    float ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0, m = qx > qy ? qx : qy;
    return sqrtf(ox * ox + oy * oy) + (m < 0 ? m : 0) - r;
}

void g_rrect(int x, int y, int w, int h, int r, uint32_t c, int alpha) {
    if (r > w / 2) r = w / 2; if (r > h / 2) r = h / 2;
    float hw = w / 2.0f, hh = h / 2.0f;
    int z = r + 2; if (z > w / 2) z = w / 2;                       /* largura da zona de borda (cada lado) */
    for (int j = y < 0 ? 0 : y; j < y + h && j < S.h; j++) {
        float py = j + 0.5f - y - hh, qy = fabsf(py) - (hh - r);
        /* miolo (|qx| bem dentro): d só depende da linha */
        float dm = (qy > 0 ? qy : (qy < -2.0f ? -2.0f : qy)) - r;
        int am = dm <= -1 ? 255 : dm >= 0 ? 0 : (int)(-dm * 255);
        span(j, x + z, x + w - z, c, am * alpha / 255);
        for (int side = 0; side < 2; side++) {                       /* zonas de canto/borda: distância exata */
            int i0 = side ? x + w - z : x, i1 = side ? x + w : x + z;
            for (int i = i0 < 0 ? 0 : i0; i < i1 && i < S.w; i++) {
                float d = sdbox(i + 0.5f - x - hw, py, hw, hh, (float)r);
                int a = d <= -1 ? 255 : d >= 0 ? 0 : (int)(-d * 255);
                if (a > 0) put(i, j, c, a * alpha / 255);
            }
        }
    }
}

void g_rring(int x, int y, int w, int h, int r, int bw, uint32_t c) {
    if (r > w / 2) r = w / 2; if (r > h / 2) r = h / 2;
    float hw = w / 2.0f, hh = h / 2.0f;
    int z = r + bw + 2; if (z > w / 2) z = w / 2;
    int band = r + bw + 2;                                           /* linhas perto do topo/base: percorre tudo */
    for (int j = y < 0 ? 0 : y; j < y + h && j < S.h; j++) {
        float py = j + 0.5f - y - hh;
        int full = (j - y) < band || (y + h - 1 - j) < band;
        for (int side = 0; side < 2; side++) {
            int i0, i1;
            if (full) { if (side) break; i0 = x; i1 = x + w; }
            else { i0 = side ? x + w - z : x; i1 = side ? x + w : x + z; }
            for (int i = i0 < 0 ? 0 : i0; i < i1 && i < S.w; i++) {
                float d = sdbox(i + 0.5f - x - hw, py, hw, hh, (float)r);
                float o = d >= 0 ? (d >= 1 ? 0 : 1 - d) : 1, in = d < -bw ? (d < -bw - 1 ? 1 : (-bw - d)) : 0;
                float a = o - in; if (a > 0) put(i, j, c, (int)(a * 255));
            }
        }
    }
}

void g_disc(int cx, int cy, int r, uint32_t c) {
    for (int j = cy - r - 1; j <= cy + r + 1; j++) for (int i = cx - r - 1; i <= cx + r + 1; i++) {
        float d = sqrtf((i + .5f - cx) * (i + .5f - cx) + (j + .5f - cy) * (j + .5f - cy)) - r;
        int a = d <= -1 ? 255 : d >= 0 ? 0 : (int)(-d * 255); if (a > 0) put(i, j, c, a);
    }
}

void g_line(float x0, float y0, float x1, float y1, float bw, uint32_t c) {
    float dx = x1 - x0, dy = y1 - y0, L2 = dx * dx + dy * dy, hw = bw / 2;
    int xa = (int)fminf(x0, x1) - (int)bw - 1, xb = (int)fmaxf(x0, x1) + (int)bw + 1, ya = (int)fminf(y0, y1) - (int)bw - 1, yb = (int)fmaxf(y0, y1) + (int)bw + 1;
    for (int j = ya; j <= yb; j++) for (int i = xa; i <= xb; i++) {
        float px = i + .5f - x0, py = j + .5f - y0, t = L2 > 0 ? (px * dx + py * dy) / L2 : 0; t = t < 0 ? 0 : t > 1 ? 1 : t;
        float ex = px - t * dx, ey = py - t * dy, d = sqrtf(ex * ex + ey * ey) - hw;
        int a = d <= -1 ? 255 : d >= 0 ? 0 : (int)(-d * 255); if (a > 0) put(i, j, c, a);
    }
}

// ---- texto (cache de glifos por tamanho) ----
typedef struct { int cp, px; unsigned char *bmp; int w, h, xo, yo, adv; } Glyph;
#define NG 768
static Glyph gc[NG];

static Glyph *glyph(int cp, int px) {
    unsigned h = ((unsigned)cp * 2654435761u ^ (unsigned)px * 40503u) % NG;
    for (int k = 0; k < NG; k++) {
        Glyph *g = &gc[(h + k) % NG];
        if (g->px == px && g->cp == cp) return g;
        if (g->px == 0) {
            float sc = stbtt_ScaleForPixelHeight(&font, (float)px); int adv, lsb;
            stbtt_GetCodepointHMetrics(&font, cp, &adv, &lsb);
            g->cp = cp; g->px = px; g->adv = (int)(adv * sc + 0.5f);
            g->bmp = stbtt_GetCodepointBitmap(&font, sc, sc, cp, &g->w, &g->h, &g->xo, &g->yo);
            return g;
        }
    }
    return &gc[h];
}

static int next_cp(const char **s) {
    const unsigned char *p = (const unsigned char *)*s; int c = *p++;
    if (c >= 0xF0) { c = ((c & 7) << 18) | ((p[0] & 63) << 12) | ((p[1] & 63) << 6) | (p[2] & 63); p += 3; }
    else if (c >= 0xE0) { c = ((c & 15) << 12) | ((p[0] & 63) << 6) | (p[1] & 63); p += 2; }
    else if (c >= 0xC0) { c = ((c & 31) << 6) | (p[0] & 63); p += 1; }
    *s = (const char *)p; return c;
}

int g_text_w(int px, const char *s, int track) { int w = 0; while (*s) { Glyph *g = glyph(next_cp(&s), px); w += g->adv + track; } return w > 0 ? w - track : 0; }

void g_text(int x, int y, int px, const char *s, uint32_t c, int track) {
    int asc, desc, gap; stbtt_GetFontVMetrics(&font, &asc, &desc, &gap);
    int base = y + (int)(asc * stbtt_ScaleForPixelHeight(&font, (float)px) + 0.5f);
    while (*s) {
        Glyph *g = glyph(next_cp(&s), px);
        if (g->bmp) for (int j = 0; j < g->h; j++) for (int i = 0; i < g->w; i++) { int a = g->bmp[j * g->w + i]; if (a) put(x + g->xo + i, base + g->yo + j, c, a); }
        x += g->adv + track;
    }
}
void g_text_c(int cx, int y, int px, const char *s, uint32_t c, int track) { g_text(cx - g_text_w(px, s, track) / 2, y, px, s, c, track); }
void g_text_r(int xr, int y, int px, const char *s, uint32_t c, int track) { g_text(xr - g_text_w(px, s, track), y, px, s, c, track); }
