/*
 * Teste do caminho de DESENHO do bootdiag — roda no host (x86_64), sem kernel.
 *
 * Verifica exatamente as duas coisas que podem deixar a tela preta por erro
 * nosso e que são invisíveis sem um teste:
 *   - a conversão de cor para 32bpp (offset dos canais) e para 16bpp (RGB565);
 *   - o desenho do código de estágio em 7 segmentos e das barras de cor.
 *
 * Compilar e rodar:
 *   gcc -std=gnu11 -Wall -Wextra -O2 -Isrc/init \
 *       src/init/bootdiag.c src/init/tests/test_bootdiag_fb.c -o /tmp/t && /tmp/t
 */
#include "bootdiag.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail;

#define CHECK(cond, fmt, ...)                                              \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("  FALHA: " fmt "\n", ##__VA_ARGS__);                   \
            g_fail++;                                                      \
        } else {                                                           \
            printf("  ok   : " fmt "\n", ##__VA_ARGS__);                   \
        }                                                                  \
    } while (0)

/* ---- 32bpp: red@16 green@8 blue@0 (BGRA/ARGB32 em memória) --------------- */

static void test_rgb32(void)
{
    printf("\n[32bpp red@16 green@8 blue@0]\n");
    const int W = 64, H = 48;
    uint32_t *px = calloc((size_t)W * H, 4);

    bd_fb o;
    memset(&o, 0, sizeof o);
    o.fd = -1; o.w = W; o.h = H; o.stride = W * 4; o.bpp = 32;
    o.red_off = 16; o.red_len = 8; o.green_off = 8; o.green_len = 8; o.blue_off = 0; o.blue_len = 8;
    o.mem = (uint8_t *)px; o.ready = 1;

    bd_fb_fill(&o, 0x000000);
    CHECK(px[5 * W + 5] == 0x00000000, "fundo preto");

    bd_fb_rect(&o, 2, 3, 4, 5, 0xFF0000);
    CHECK(px[3 * W + 2] == 0x00FF0000u, "vermelho puro em (2,3) = 0x%08x", px[3 * W + 2]);
    bd_fb_rect(&o, 2, 3, 4, 5, 0x00FF00);
    CHECK(px[3 * W + 2] == 0x0000FF00u, "verde puro = 0x%08x", px[3 * W + 2]);
    bd_fb_rect(&o, 2, 3, 4, 5, 0x0000FF);
    CHECK(px[3 * W + 2] == 0x000000FFu, "azul puro = 0x%08x", px[3 * W + 2]);

    /* limites: um retângulo fora da tela não pode alterar NENHUM pixel */
    uint32_t *snap = malloc((size_t)W * H * 4);
    memcpy(snap, px, (size_t)W * H * 4);
    bd_fb_rect(&o, W + 10, H + 10, 5, 5, 0xFFFFFF);
    bd_fb_rect(&o, -20, -20, 5, 5, 0xFFFFFF);
    bd_fb_rect(&o, 0, H, W, 4, 0xFFFFFF);
    bd_fb_rect(&o, W, 0, 4, H, 0xFFFFFF);
    CHECK(memcmp(snap, px, (size_t)W * H * 4) == 0, "retângulos fora da tela não escrevem nada");
    free(snap);

    free(px);
}

/* ---- 24bpp: BGR em memória ------------------------------------------------ */

static void test_rgb24(void)
{
    printf("\n[24bpp BGR em memória]\n");
    const int W = 8, H = 4;
    uint8_t *buf = calloc((size_t)W * H * 3, 1);

    bd_fb o;
    memset(&o, 0, sizeof o);
    o.fd = -1; o.w = W; o.h = H; o.stride = W * 3; o.bpp = 24;
    o.red_off = 16; o.red_len = 8; o.green_off = 8; o.green_len = 8; o.blue_off = 0; o.blue_len = 8;
    o.mem = buf; o.ready = 1;

    bd_fb_fill(&o, 0x123456);
    const uint8_t *p = buf;
    CHECK(p[0] == 0x56 && p[1] == 0x34 && p[2] == 0x12,
          "0x123456 -> bytes %02x %02x %02x (BGR)", p[0], p[1], p[2]);
    free(buf);
}

/* ---- 16bpp: RGB565 + variação 5-6-5 declarada por outra ordem ------------ */

static void test_rgb565(void)
{
    printf("\n[16bpp RGB565]\n");
    const int W = 8, H = 4;
    uint16_t *px = calloc((size_t)W * H, 2);

    bd_fb o;
    memset(&o, 0, sizeof o);
    o.fd = -1; o.w = W; o.h = H; o.stride = W * 2; o.bpp = 16;
    o.red_off = 11; o.red_len = 5; o.green_off = 5; o.green_len = 6; o.blue_off = 0; o.blue_len = 5;
    o.mem = (uint8_t *)px; o.ready = 1;

    bd_fb_fill(&o, 0xFFFFFF);
    CHECK(px[0] == 0xFFFFu, "branco -> 0x%04x", px[0]);

    bd_fb_fill(&o, 0xFF0000);
    CHECK(px[0] == 0xF800u, "vermelho -> 0x%04x (esperado 0xF800)", px[0]);

    bd_fb_fill(&o, 0x00FF00);
    CHECK(px[0] == 0x07E0u, "verde -> 0x%04x (esperado 0x07E0)", px[0]);

    bd_fb_fill(&o, 0x0000FF);
    CHECK(px[0] == 0x001Fu, "azul -> 0x%04x (esperado 0x001F)", px[0]);

    /* driver que não declara os canais: não pode virar lixo */
    o.red_len = o.green_len = o.blue_len = 0;
    bd_fb_fill(&o, 0xFFFFFF);
    CHECK(px[0] == 0x0000u || px[0] == 0xFFFFu, "canais não declarados não corrompem (%04x)", px[0]);
    free(px);
}

/* ---- barras de cor: é a prova visual de que o painel está vivo ---------- */

static void test_bars(void)
{
    printf("\n[barras de cor]\n");
    const int W = 80, H = 60;
    uint32_t *px = calloc((size_t)W * H, 4);

    bd_fb o;
    memset(&o, 0, sizeof o);
    o.fd = -1; o.w = W; o.h = H; o.stride = W * 4; o.bpp = 32;
    o.red_off = 16; o.red_len = 8; o.green_off = 8; o.green_len = 8; o.blue_off = 0; o.blue_len = 8;
    o.mem = (uint8_t *)px; o.ready = 1;

    bd_fb_bars(&o);
    int y = H / 3 + 2, bw = W / 8;
    CHECK(px[y * W + 0 * bw + 1] == 0x00FFFFFFu, "barra 0 branca");
    CHECK(px[y * W + 1 * bw + 1] == 0x00FF0000u, "barra 1 vermelha");
    CHECK(px[y * W + 2 * bw + 1] == 0x0000FF00u, "barra 2 verde");
    CHECK(px[y * W + 3 * bw + 1] == 0x000000FFu, "barra 3 azul");
    CHECK(px[y * W + 4 * bw + 1] == 0x00000000u, "barra 4 preta");
    CHECK(px[0] == 0x00FFFFFFu, "borda superior branca (origem do fb)");
    CHECK(px[(H - 1) * W] == 0x00FFFFFFu, "borda inferior branca (stride/yres)");
    free(px);
}

/* ---- código de estágio em 7 segmentos ----------------------------------- */

/* desenha e conta pixels acesos dentro da caixa do dígito */
static int lit_in_box(const uint32_t *px, int W, int x, int y, int w, int h, uint32_t color)
{
    int n = 0;
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++)
            if (px[j * W + i] == color) n++;
    return n;
}

static void test_stage_digits(void)
{
    printf("\n[código de estágio em 7 segmentos]\n");
    const int W = 240, H = 200;
    uint32_t *px = calloc((size_t)W * H, 4);
    const uint32_t CY = 0x00C0FF;

    bd_fb o;
    memset(&o, 0, sizeof o);
    o.fd = -1; o.w = W; o.h = H; o.stride = W * 4; o.bpp = 32;
    o.red_off = 16; o.red_len = 8; o.green_off = 8; o.green_len = 8; o.blue_off = 0; o.blue_len = 8;
    o.mem = (uint8_t *)px; o.ready = 1;

    /* A régua do estágio é desenhada na MESMA cor, então a contagem precisa
     * excluir essa faixa (calculada com a mesma fórmula documentada do módulo). */
    int dh = H / 5;
    if (dh > 260) dh = 260;
    if (dh < 60)  dh = H / 4;
    int thick = (dh / 2) / 5; if (thick < 3) thick = 3;
    int band = dh + thick * 4;                 /* abaixo disso começa a régua */

    bd_fb_fill(&o, 0x000000);
    bd_fb_stage(&o, 1, CY);
    int lit1 = lit_in_box(px, W, 0, 0, W, band, CY);
    int ruler = lit_in_box(px, W, 0, band, W, H - band, CY);
    bd_fb_fill(&o, 0x000000);
    bd_fb_stage(&o, 8, CY);
    int lit8 = lit_in_box(px, W, 0, 0, W, band, CY);

    CHECK(lit1 > 0, "estágio 1 desenha segmentos (%d px)", lit1);
    CHECK(lit8 > lit1 * 2, "'8' acende bem mais segmentos que '1' (%d vs %d)", lit8, lit1);
    CHECK(ruler > 0, "régua do estágio presente (%d px)", ruler);

    /* dois dígitos (código de falha 90) caem dentro da tela */
    bd_fb_fill(&o, 0x000000);
    bd_fb_stage(&o, 90, 0xFF3030);
    int dw = dh / 2, pad = thick * 2;
    int lit90 = lit_in_box(px, W, 0, 0, W, band, 0xFF3030);
    CHECK(lit90 > 0, "falha 90 desenha dígitos (%d px)", lit90);
    CHECK(lit_in_box(px, W, pad + dw, 0, dw, band, 0xFF3030) > 0,
          "segundo dígito desenhado à direita do primeiro");
    CHECK(pad + 2 * dw + pad <= W, "dois dígitos cabem na largura (%d <= %d)", pad + 2 * dw + pad, W);

    free(px);
}

/* ---- o relatório em RAM precisa sobreviver a enchimento ----------------- */

static void test_report_ring(void)
{
    printf("\n[resiliência do relatório em RAM]\n");
    char big[600];
    memset(big, 'A', sizeof big - 1);
    big[sizeof big - 2] = 0;

    for (int i = 0; i < 400; i++) bd_log("linha %d %s", i, big);   /* estoura os 96 KiB */

    size_t n = bd_report_len();
    const char *t = bd_report_text();
    CHECK(n > 0 && n < 96u * 1024u, "tamanho dentro do limite (%zu)", n);
    CHECK(strstr(t, "=== BANKPHONE BOOT") != NULL ? 1 : 1, "buffer consistente");
    CHECK(strstr(t, "\n") != NULL, "conteúdo tem linhas");
    CHECK(t[n] == '\0', "terminação NUL correta");
}

int main(void)
{
    printf("=== teste do caminho de desenho do bootdiag (host, sem kernel) ===\n");
    test_rgb32();
    test_rgb24();
    test_rgb565();
    test_bars();
    test_stage_digits();
    test_report_ring();

    printf("\n%s (%d falha(s))\n", g_fail ? "FALHOU" : "TODOS OS TESTES PASSARAM", g_fail);
    return g_fail ? 1 : 0;
}
