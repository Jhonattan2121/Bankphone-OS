// BANKPHONE OS — gráficos 2D por software. Escrito do zero. Superfície 0x00RRGGBB.
#pragma once
#include <stdint.h>

typedef struct { uint32_t *p; int w, h, stride; } Surf;   // stride em pixels

int  gfx_init(const unsigned char *ttf);                   // 0 = ok
void gfx_set(Surf s);
void g_fill(int x, int y, int w, int h, uint32_t c);
void g_rrect(int x, int y, int w, int h, int r, uint32_t c, int alpha);          // alpha 0..255
void g_rring(int x, int y, int w, int h, int r, int bw, uint32_t c);             // só borda
void g_disc(int cx, int cy, int r, uint32_t c);
void g_line(float x0, float y0, float x1, float y1, float bw, uint32_t c);
int  g_text_w(int px, const char *s, int track);
void g_text(int x, int y, int px, const char *s, uint32_t c, int track);        // y = topo; px = altura da fonte
void g_text_c(int cx, int y, int px, const char *s, uint32_t c, int track);     // centralizado em cx
void g_text_r(int xr, int y, int px, const char *s, uint32_t c, int track);     // alinhado à direita em xr
