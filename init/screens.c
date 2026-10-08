/*
 * BANKPHONE OS — TELAS. Cada tela é composição de componentes: nenhum desenho à
 * mão, nenhuma constante de cor ou tamanho fora do theme.h.
 *
 * Estado que estas telas leem (todas globais já existentes no sistema):
 *   M / m_balance / m_total_brl / m_state_name / m_required / m_pix_spent_since  (money.c)
 *   PIN / pin_locked_s                                                          (sec.c)
 *   HW / hw_*                                                                   (hw.c)
 *   plat_now / plat_clock_valid / plat_boot_prop / plat_persist_ok               (ui.h)
 *   ui_dev()  — contadores publicados pelo PID 1                                 (ui.h)
 */
#include "ui.h"
#include "nav.h"
#include "components.h"
#include "icons.h"
#include "money.h"
#include "sec.h"
#include "hw.h"
#include "store.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* estado de UI que a tela Developer mostra (publicado pelo ui.c) */
extern int ui_haptics_on(void);
extern int ui_hitcount(int *abaixo44, int *min_h_dp);
extern int ui_screen_brightness(void);

/* ------------------------------------------------------------------ apoio -- */

static const char *env_name(void) { return "DEMO"; }   /* até existir parceiro/rede real */

/* valor em USDC equivalente, pela taxa DEMO (declarada na tela) */
static void approx_usdc(char *out, size_t n, int64_t brl_cents)
{
    int64_t usdc_micro = brl_cents * 1000000 / 560;     /* 1 USDC = R$ 5,60 (DEMO) */
    fmt_money(out, n, A_USDC, usdc_micro);
}

/* quando aconteceu: hora REAL se o relógio for válido; senão uptime, e o rótulo
 * diz exatamente qual dos dois está sendo mostrado. */
static void when_str(char *o, size_t n, int64_t created)
{
    if (plat_clock_valid()) {
        time_t tt = (time_t)created - 3 * 3600;
        struct tm *tm = gmtime(&tt);
        snprintf(o, n, "%02d/%02d %02d:%02d", tm->tm_mday, tm->tm_mon + 1, tm->tm_hour, tm->tm_min);
    } else {
        snprintf(o, n, "há %lld min", (long long)((plat_now() - created) / 60));
    }
}

/* cartão de seção: devolve o Y interno, já com o rótulo desenhado */
static int card(int *y, int h, const char *caps, int act, int arg)
{
    int y0 = comp_card_begin(th_cx(), *y, th_cw(), h, caps, act, arg);
    *y += h + TH_DP(TH_SP4);
    return y0;
}

/* ================================================================= MONEY === */

static int scr_money(int scroll)
{
    (void)scroll;
    int y = th_top() + TH_DP(TH_SP4);
    int x = th_cx(), w = th_cw();

    /* cabeçalho: marca + selo de ambiente (o selo é o único, não se repete) */
    {
        int bw = comp_caps_w(env_name()) + TH_DP(TH_SP3) * 2;
        comp_caps_clip(x, y, w - bw - TH_DP(TH_SP3), "BANKPHONE", TH_TXT3);
        comp_badge(x + w - bw, y - TH_DP(TH_SP1), env_name(), TH_DEMO);
    }
    y += TH_DP(TH_F_CAPS) + TH_DP(TH_SP4);

    /* cartão do saldo */
    {
        int h = TH_DP(TH_SP4) + TH_DP(TH_F_CAPS) + TH_DP(TH_SP2) + TH_DP(TH_F_XXL)
              + TH_DP(TH_SP3) + TH_DP(TH_F_XS) + TH_DP(TH_SP4) + TH_DP(TH_SP2)
              + 2 * TH_DP(TH_H_LISTA - TH_SP2) + TH_DP(TH_SP2);
        int cy = card(&y, h, NULL, ACT_NONE, 0);
        char b[96], a[64];
        comp_caps(x + TH_DP(TH_SP4), cy, "SALDO TOTAL", TH_TXT3);
        cy += TH_DP(TH_F_CAPS) + TH_DP(TH_SP2);
        fmt_money(b, sizeof b, A_BRL, m_total_brl());
        comp_num(x + TH_DP(TH_SP4), cy, TH_DP(TH_F_XXL), b, TH_TXT, TH_W_STRONG);
        cy += TH_DP(TH_F_XXL) + TH_DP(TH_SP3);
        approx_usdc(a, sizeof a, m_total_brl());
        snprintf(b, sizeof b, "~ %s  ·  DEMO 1 USDC = R$ 5,60", a);
        comp_label(x + TH_DP(TH_SP4), cy, b, TH_TXT3, TH_DP(TH_F_XS), TH_W_REG);
        cy += TH_DP(TH_F_XS) + TH_DP(TH_SP4);
        comp_divider(x + TH_DP(TH_SP4), cy, w - 2 * TH_DP(TH_SP4));
        cy += TH_DP(TH_SP2);
        fmt_money(b, sizeof b, A_BRL, m_balance(A_BRL));
        comp_row_kv(x + TH_DP(TH_SP4), cy + TH_DP(TH_SP2), w - 2 * TH_DP(TH_SP4), "BRL", b, TH_TXT, TH_DP(TH_F_S), TH_W_REG);
        cy += TH_DP(TH_H_LISTA);
        fmt_money(b, sizeof b, A_USDC, m_balance(A_USDC));
        comp_row_kv(x + TH_DP(TH_SP4), cy + TH_DP(TH_SP2), w - 2 * TH_DP(TH_SP4), "USDC", b, TH_TXT, TH_DP(TH_F_S), TH_W_REG);
    }

    /* ações */
    {
        int bwid = (w - 2 * TH_DP(TH_SP3)) / 3;
        int bh = TH_DP(TH_H_BOTAO_G);
        comp_button(x, y, bwid, bh, "ENVIAR", IC_SEND, BK_PRIMARY, ACT_SHEET, SH_WHO, 0, M.emergency);
        comp_button(x + bwid + TH_DP(TH_SP3), y, bwid, bh, "RECEBER", IC_RECEIVE, BK_NEUTRAL, ACT_SHEET, SH_RECEIVE, 0, 0);
        comp_button(x + 2 * (bwid + TH_DP(TH_SP3)), y, bwid, bh, "TROCAR", IC_SWAP, BK_NEUTRAL, ACT_SHEET, SH_AMOUNT, 0, M.emergency);
        y += bh + TH_DP(TH_SP4);
    }
    if (M.emergency) {
        int cy = card(&y, TH_DP(TH_SP4) + TH_DP(TH_F_S) + TH_DP(TH_SP4), NULL, ACT_NONE, 0);
        icon(IC_WARNING, x + TH_DP(TH_SP4) + TH_DP(10), cy + TH_DP(10), TH_DP(20), TH_BAD);
        comp_label(x + TH_DP(TH_SP4) + TH_DP(TH_SP5) + TH_DP(TH_SP2), cy + TH_DP(2),
                   "MODO EMERGÊNCIA ATIVO — operações travadas", TH_BAD, TH_DP(TH_F_S), TH_W_REG);
    }

    /* últimas transações */
    if (M.n == 0) {
        comp_caps(x, y, "SEM HISTÓRICO", TH_TXT3);
        y += TH_DP(TH_F_CAPS) + TH_DP(TH_SP2);
        y += comp_wrap(x, y, w, TH_DP(TH_F_XS),
                       "Nenhuma transação neste aparelho. Tudo neste sistema e' demonstracao: "
                       "nada aqui move dinheiro real.", TH_TXT3);
        y += TH_DP(TH_SP4);
        comp_button(x, y, w, TH_DP(TH_H_BOTAO_G), "CARREGAR SALDO DEMO", IC_PLUS, BK_NEUTRAL, ACT_LOADDEMO, 0, 0, 0);
        y += TH_DP(TH_H_BOTAO_G);
        return y;
    }
    {
        const char *rot = "ÚLTIMAS TRANSAÇÕES · LIVRO-RAZÃO DEMO";
        int bw = comp_caps_w(env_name()) + TH_DP(TH_SP3) * 2;
        comp_caps_clip(x, y, w - bw - TH_DP(TH_SP3), rot, TH_TXT3);
        comp_badge(x + w - bw, y - TH_DP(TH_SP1), env_name(), TH_DEMO);
    }
    y += TH_DP(TH_F_CAPS) + TH_DP(TH_SP2);
    int mostradas = 0;
    for (int i = M.n - 1; i >= 0 && mostradas < 3; i--, mostradas++) {
        Tx *t = &M.tx[i];
        char titulo[80], sub[48], quando[48], val[48];
        switch (t->type) {
        case T_PIX_IN:  snprintf(titulo, sizeof titulo, "Pix recebido"); break;
        case T_PIX_OUT: snprintf(titulo, sizeof titulo, "Pix para %.40s", t->cp); break;
        case T_SWAP:    snprintf(titulo, sizeof titulo, "Troca %.30s", t->cp); break;
        default:        snprintf(titulo, sizeof titulo, "Saldo de demonstração"); break;
        }
        snprintf(sub, sizeof sub, "%.20s · DEMO", m_state_name(t->st));
        when_str(quando, sizeof quando, t->created);
        if (t->type == T_PIX_OUT) { fmt_money(val, sizeof val, A_BRL, t->from_amt); }
        else { fmt_money(val, sizeof val, t->to == A_NONE ? A_BRL : t->to, t->to_amt); }
        y += comp_tx_row(x, y, w, titulo, sub, quando, val,
                         t->type == T_PIX_OUT ? TH_TXT : TH_OK, ACT_TX, i);
    }
    comp_button(x, y + TH_DP(TH_SP3), w, TH_DP(TH_H_BOTAO), "VER TODAS", IC_CHEVRON, BK_NEUTRAL, ACT_TAB, SC_ACTIVITY, 0, 0);
    y += TH_DP(TH_H_BOTAO) + TH_DP(TH_SP3);
    return y;
}

/* ============================================================== ACTIVITY === */

static int scr_activity(int scroll)
{
    int y = th_top() + TH_DP(TH_SP4);
    int x = th_cx(), w = th_cw();
    char l[64];
    snprintf(l, sizeof l, "ATIVIDADE · %d", M.n);
    comp_caps(x, y, l, TH_TXT3);
    y += TH_DP(TH_F_CAPS) + TH_DP(TH_SP3);
    if (!M.n) {
        comp_label(x, y, "Nada registrado ainda.", TH_TXT2, TH_DP(TH_F_M), TH_W_REG);
        return y + TH_DP(TH_F_M);
    }
    y -= scroll;
    for (int i = M.n - 1; i >= 0; i--) {
        Tx *t = &M.tx[i];
        char titulo[80], sub[48], quando[64], val[48];
        switch (t->type) {
        case T_PIX_IN:  snprintf(titulo, sizeof titulo, "Pix recebido"); break;
        case T_PIX_OUT: snprintf(titulo, sizeof titulo, "Pix para %.40s", t->cp); break;
        case T_SWAP:    snprintf(titulo, sizeof titulo, "Troca %.30s", t->cp); break;
        default:        snprintf(titulo, sizeof titulo, "Saldo de demonstração"); break;
        }
        snprintf(sub, sizeof sub, "%.20s · DEMO", m_state_name(t->st));
        when_str(quando, sizeof quando, t->created);
        if (!plat_clock_valid()) {
            /* sem relógio real, dizer UPTIME em vez de fingir uma hora */
            snprintf(quando, sizeof quando, "em %lldmin de uptime", (long long)(t->created / 60));
        }
        if (t->type == T_PIX_OUT) fmt_money(val, sizeof val, A_BRL, t->from_amt);
        else fmt_money(val, sizeof val, t->to == A_NONE ? A_BRL : t->to, t->to_amt);
        y += comp_tx_row(x, y, w, titulo, sub, quando, val, t->type == T_PIX_OUT ? TH_TXT : TH_OK, ACT_TX, i);
    }
    return y + scroll + TH_DP(TH_SP4);
}

/* ================================================================ WALLET === */

static int scr_wallet(int scroll)
{
    (void)scroll;
    int y = th_top() + TH_DP(TH_SP4);
    int x = th_cx(), w = th_cw();

    comp_caps_clip(x, y, w - (comp_caps_w("TESTNET NÃO INICIADO") + TH_DP(TH_SP3) * 2) - TH_DP(TH_SP3), "CARTEIRA", TH_TXT3);
    {
        int bw = comp_caps_w("TESTNET NÃO INICIADO") + TH_DP(TH_SP3) * 2;
        comp_badge(x + w - bw, y - TH_DP(TH_SP1), "TESTNET NÃO INICIADO", TH_WARN);
    }
    y += TH_DP(TH_F_CAPS) + TH_DP(TH_SP4);

    /* Estado honesto: nada foi gerado ainda. Nenhum valor falso é mostrado. */
    {
        int h = TH_DP(TH_SP4) + TH_DP(TH_F_XL) + TH_DP(TH_SP3) + 5 * TH_DP(TH_H_LISTA) + TH_DP(TH_SP4);
        int cy = card(&y, h, NULL, ACT_NONE, 0);
        comp_label(x + TH_DP(TH_SP4), cy, "NÃO IMPLEMENTADO", TH_WARN, TH_DP(TH_F_XL), TH_W_REG);
        cy += TH_DP(TH_F_XL) + TH_DP(TH_SP3);
        comp_row_kv(x + TH_DP(TH_SP4), cy + TH_DP(TH_SP2), w - 2 * TH_DP(TH_SP4), "Chave privada", "NÃO GERADA", TH_TXT3, TH_DP(TH_F_S), TH_W_REG); cy += TH_DP(TH_H_LISTA);
        comp_row_kv(x + TH_DP(TH_SP4), cy + TH_DP(TH_SP2), w - 2 * TH_DP(TH_SP4), "Endereço", "NOT AVAILABLE", TH_TXT3, TH_DP(TH_F_S), TH_W_REG); cy += TH_DP(TH_H_LISTA);
        comp_row_kv(x + TH_DP(TH_SP4), cy + TH_DP(TH_SP2), w - 2 * TH_DP(TH_SP4), "Rede", "NENHUMA", TH_TXT3, TH_DP(TH_F_S), TH_W_REG); cy += TH_DP(TH_H_LISTA);
        comp_row_kv(x + TH_DP(TH_SP4), cy + TH_DP(TH_SP2), w - 2 * TH_DP(TH_SP4), "Saldo on-chain", "NOT AVAILABLE", TH_TXT3, TH_DP(TH_F_S), TH_W_REG); cy += TH_DP(TH_H_LISTA);
        comp_row_kv(x + TH_DP(TH_SP4), cy + TH_DP(TH_SP2), w - 2 * TH_DP(TH_SP4), "Última transação", "NENHUMA", TH_TXT3, TH_DP(TH_F_S), TH_W_REG);
    }

    comp_caps_clip(x, y, w, "O QUE FALTA (F5)", TH_TXT3);
    y += TH_DP(TH_F_CAPS) + TH_DP(TH_SP2);
    y += comp_wrap(x, y, w, TH_DP(TH_F_XS),
                   "Para esta tela deixar de dizer NÃO IMPLEMENTADO faltam, nesta ordem: "
                   "(1) geração de chave no aparelho com entropia do kernel; (2) curva secp256k1 e "
                   "keccak256 próprias; (3) endereço EVM derivado da chave pública; "
                   "(4) assinatura de transação EIP-155/EIP-1559; (5) RPC HTTPS de uma testnet EVM; "
                   "(6) hash de transação consultável num explorer. A troca de moedas de hoje é "
                   "lançamento contábil DEMO: não tem hash, não tem taxa on-chain, não sai do aparelho.",
                   TH_TXT3);
    y += TH_DP(TH_SP4);
    return y;
}

/* ============================================================== SECURITY === */

static int scr_security(int scroll)
{
    (void)scroll;
    int y = th_top() + TH_DP(TH_SP4);
    int x = th_cx(), w = th_cw();
    char b[64], l[96];

    comp_caps(x, y, "SEGURANÇA", TH_TXT3);
    y += TH_DP(TH_F_CAPS) + TH_DP(TH_SP3);

    {
        int h = TH_DP(TH_SP4) + 6 * TH_DP(TH_H_LISTA) + TH_DP(TH_SP2);
        int cy = card(&y, h, NULL, ACT_NONE, 0);
        comp_row_list(x + TH_DP(TH_SP2), cy, w - 2 * TH_DP(TH_SP2), IC_LOCK, "PIN da tela",
                      PIN.set ? "DEFINIDO" : "NÃO DEFINIDO", PIN.set ? TH_OK : TH_BAD, ACT_NONE, 0, 0);
        cy += TH_DP(TH_H_LISTA);
        comp_row_list(x + TH_DP(TH_SP2), cy, w - 2 * TH_DP(TH_SP2), IC_INFO, "Biometria",
                      "NOT AVAILABLE", TH_WARN, ACT_NONE, 0, 0);
        cy += TH_DP(TH_H_LISTA);
        {
            const char *vb = plat_boot_prop("androidboot.verifiedbootstate");
            comp_row_list(x + TH_DP(TH_SP2), cy, w - 2 * TH_DP(TH_SP2), IC_INFO, "Verified boot",
                          vb[0] ? vb : "NOT AVAILABLE", (vb[0] && !strcmp(vb, "green")) ? TH_OK : TH_WARN, ACT_NONE, 0, 0);
        }
        cy += TH_DP(TH_H_LISTA);
        {
            const char *fl = plat_boot_prop("androidboot.flash.locked");
            const char *v = !fl[0] ? "NOT AVAILABLE" : !strcmp(fl, "1") ? "BLOQUEADO" : "DESBLOQUEADO";
            comp_row_list(x + TH_DP(TH_SP2), cy, w - 2 * TH_DP(TH_SP2), IC_INFO, "Bootloader",
                          v, !strcmp(v, "BLOQUEADO") ? TH_OK : TH_WARN, ACT_NONE, 0, 0);
        }
        cy += TH_DP(TH_H_LISTA);
        comp_row_list(x + TH_DP(TH_SP2), cy, w - 2 * TH_DP(TH_SP2), IC_INFO, "Chaves em hardware (TEE)",
                      "NOT AVAILABLE", TH_WARN, ACT_NONE, 0, 0);
        cy += TH_DP(TH_H_LISTA);
        comp_row_list(x + TH_DP(TH_SP2), cy, w - 2 * TH_DP(TH_SP2), IC_INFO, "Estado sobrevive a reinício",
                      plat_persist_ok() ? "SIM" : "NÃO (só memória)", plat_persist_ok() ? TH_OK : TH_WARN, ACT_NONE, 0, 0);
    }

    /* por que a biometria e o TEE dizem NOT AVAILABLE — explicado, não escondido */
    y += comp_wrap(x, y, w, TH_DP(TH_F_XS),
                   "O leitor de digital existe fisicamente, mas o driver e o serviço de chaves do "
                   "fabricante não estão neste kernel/rootfs; o TEE está nas partições (tee_a/b) e é "
                   "inacessível sem o boot verificado do Android. Portanto NÃO HÁ keystore de hardware: "
                   "o PIN é a única barreira, com SHA-256 iterado 50.000 vezes e sal do /dev/urandom.",
                   TH_TXT3);
    y += TH_DP(TH_SP4);

    comp_caps_clip(x, y, w, "POLÍTICA DE TRANSAÇÃO", TH_TXT3);
    y += TH_DP(TH_F_CAPS) + TH_DP(TH_SP2);
    {
        int h = TH_DP(TH_SP4) + 5 * TH_DP(TH_H_LISTA) + TH_DP(TH_SP2);
        int cy = card(&y, h, NULL, ACT_NONE, 0);
        fmt_money(b, sizeof b, A_BRL, M.pol.pin_max);
        snprintf(l, sizeof l, "até %s", b);
        comp_row_kv(x + TH_DP(TH_SP4), cy + TH_DP(TH_SP2), w - 2 * TH_DP(TH_SP4), l, "PIN", TH_TXT, TH_DP(TH_F_S), TH_W_REG); cy += TH_DP(TH_H_LISTA);
        fmt_money(b, sizeof b, A_BRL, M.pol.confirm_max);
        snprintf(l, sizeof l, "até %s", b);
        comp_row_kv(x + TH_DP(TH_SP4), cy + TH_DP(TH_SP2), w - 2 * TH_DP(TH_SP4), l, "PIN + CONFIRMAÇÃO", TH_TXT, TH_DP(TH_F_S), TH_W_REG); cy += TH_DP(TH_H_LISTA);
        snprintf(l, sizeof l, "acima de %s", b);
        comp_row_kv(x + TH_DP(TH_SP4), cy + TH_DP(TH_SP2), w - 2 * TH_DP(TH_SP4), l, "PIN + 2ª CONFIRMAÇÃO", TH_TXT, TH_DP(TH_F_S), TH_W_REG); cy += TH_DP(TH_H_LISTA);
        fmt_money(b, sizeof b, A_BRL, M.pol.per_tx);
        comp_row_kv(x + TH_DP(TH_SP4), cy + TH_DP(TH_SP2), w - 2 * TH_DP(TH_SP4), "Por Pix", b, TH_TXT, TH_DP(TH_F_S), TH_W_REG); cy += TH_DP(TH_H_LISTA);
        fmt_money(b, sizeof b, A_BRL, M.pol.daily);
        comp_row_kv(x + TH_DP(TH_SP4), cy + TH_DP(TH_SP2), w - 2 * TH_DP(TH_SP4), "Por dia", b, TH_TXT, TH_DP(TH_F_S), TH_W_REG);
    }

    comp_button(x, y, w, TH_DP(TH_H_BOTAO_G), M.emergency ? "DESLIGAR MODO EMERGÊNCIA" : "MODO EMERGÊNCIA",
                IC_WARNING, M.emergency ? BK_NEUTRAL : BK_DANGER, ACT_EMERG, 0, M.emergency, 0);
    y += TH_DP(TH_H_BOTAO_G) + TH_DP(TH_SP2);
    y += comp_wrap(x, y, w, TH_DP(TH_F_XS),
                   "O modo emergência trava Pix e troca imediatamente. Desligar exige o PIN. "
                   "Ele é gravado junto com o estado.", TH_TXT3);
    y += TH_DP(TH_SP3);
    comp_button(x, y, w, TH_DP(TH_H_BOTAO), "BLOQUEAR TELA", IC_LOCK, BK_NEUTRAL, ACT_LOCK, 0, 0, 0);
    y += TH_DP(TH_H_BOTAO);
    return y;
}

/* ============================================================== SETTINGS === */

static int scr_settings(int scroll)
{
    (void)scroll;
    int y = th_top() + TH_DP(TH_SP4);
    int x = th_cx(), w = th_cw();
    char l[128];

    comp_caps(x, y, "AJUSTES", TH_TXT3);
    y += TH_DP(TH_F_CAPS) + TH_DP(TH_SP3);

    /* ---- brilho: valor REAL do nó de backlight ---- */
    {
        int pct = hw_brightness_get();
        int h = TH_DP(TH_SP4) + TH_DP(TH_F_XS) + TH_DP(TH_SP3) + TH_DP(TH_TOUCH_MIN_DP)
              + TH_DP(TH_SP3) + TH_DP(TH_H_BOTAO) + TH_DP(TH_SP4);
        int cy = card(&y, h, "TELA", ACT_NONE, 0);
        int ix = x + TH_DP(TH_SP4), iw = w - 2 * TH_DP(TH_SP4);
        if (pct < 0) {
            comp_label(ix, cy, hw_state_str(HW.bl.st), TH_WARN, TH_DP(TH_F_XS), TH_W_STRONG);
            cy += TH_DP(TH_F_XS) + TH_DP(TH_SP3);
            comp_button(ix, cy, iw, TH_DP(TH_H_BOTAO), "REDESCOBRIR NÓ DE BRILHO",
                        IC_SETTINGS, BK_NEUTRAL, ACT_BL_DISCOVER, 0, 0, 0);
        } else {
            snprintf(l, sizeof l, "Brilho: %d%% · %.60s (máx %d)", pct, HW.bl_node, HW.bl_max);
            comp_label(ix, cy, l, TH_TXT2, TH_DP(TH_F_XS), TH_W_REG);
            cy += TH_DP(TH_F_XS) + TH_DP(TH_SP3);
            /* barra: alvo de 44 dp de altura; o valor mostrado é o LIDO de volta do /sys */
            int sl_h = TH_DP(TH_SP2);
            g_rrect(ix, cy + TH_DP(TH_TOUCH_MIN_DP) / 2 - sl_h / 2, iw, sl_h, sl_h / 2, TH_SURF3, 255);
            int fill = iw * pct / 100;
            if (fill > 0) g_rrect(ix, cy + TH_DP(TH_TOUCH_MIN_DP) / 2 - sl_h / 2, fill, sl_h, sl_h / 2, TH_ACC, 255);
            g_disc(ix + fill, cy + TH_DP(TH_TOUCH_MIN_DP) / 2, TH_DP(TH_SP2) + TH_DP(TH_SP1), TH_TXT);
            ui_hit_add(ix, cy, iw, TH_DP(TH_TOUCH_MIN_DP), ACT_BRIGHT, -1);   /* -1 = usa a posição X */
            cy += TH_DP(TH_TOUCH_MIN_DP) + TH_DP(TH_SP3);
            int bwid = (iw - TH_DP(TH_SP3)) / 2;
            comp_button(ix, cy, bwid, TH_DP(TH_H_BOTAO), "ESCURECER", IC_BACK, BK_NEUTRAL, ACT_BRIGHT_STEP, -5, 0, 0);
            comp_button(ix + bwid + TH_DP(TH_SP3), cy, bwid, TH_DP(TH_H_BOTAO), "CLAREAR", IC_PLUS, BK_NEUTRAL, ACT_BRIGHT_STEP, +5, 0, 0);
        }
    }

    /* ---- vibração ---- */
    {
        int h = TH_DP(TH_SP4) + 2 * TH_DP(TH_H_LISTA) + TH_DP(TH_SP4);
        int cy = card(&y, h, "TATO", ACT_NONE, 0);
        snprintf(l, sizeof l, "%s", HW.vib.st == HW_AVAILABLE ? HW.vib.why : HW.vib.why);
        comp_row_list(x + TH_DP(TH_SP2), cy, w - 2 * TH_DP(TH_SP2), IC_INFO, "Vibração",
                      hw_state_str(HW.vib.st), HW.vib.st == HW_AVAILABLE ? TH_OK : TH_WARN, ACT_NONE, 0, HW.vib.st != HW_AVAILABLE);
        cy += TH_DP(TH_H_LISTA);
        comp_row_list(x + TH_DP(TH_SP2), cy, w - 2 * TH_DP(TH_SP2), IC_INFO, "Vibrar ao tocar",
                      ui_haptics_on() ? "LIGADO" : "DESLIGADO", ui_haptics_on() ? TH_ACC : TH_TXT3, ACT_HAPTIC, 0, HW.vib.st != HW_AVAILABLE);
        cy += TH_DP(TH_H_LISTA);
        comp_button(x + TH_DP(TH_SP4), cy, w - 2 * TH_DP(TH_SP4), TH_DP(TH_H_BOTAO),
                    "TESTAR VIBRAÇÃO (200 ms)", IC_WARNING, BK_NEUTRAL, ACT_VIB_TEST, 200, 0, HW.vib.st != HW_AVAILABLE);
    }

    /* ---- sistema ---- */
    {
        int h = TH_DP(TH_SP4) + 4 * TH_DP(TH_H_LISTA) + TH_DP(TH_SP2);
        int cy = card(&y, h, "SISTEMA", ACT_NONE, 0);
        snprintf(l, sizeof l, "%dx%d · %d bpp · stride %d", HW.scr_w, HW.scr_h, HW.scr_bpp, HW.scr_stride);
        comp_row_kv(x + TH_DP(TH_SP4), cy + TH_DP(TH_SP2), w - 2 * TH_DP(TH_SP4), "Tela", l, TH_TXT, TH_DP(TH_F_S), TH_W_REG); cy += TH_DP(TH_H_LISTA);
        comp_row_kv(x + TH_DP(TH_SP4), cy + TH_DP(TH_SP2), w - 2 * TH_DP(TH_SP4), "Relógio",
                    plat_clock_valid() ? "REAL" : "SEM RTC (uptime)", plat_clock_valid() ? TH_OK : TH_WARN, TH_DP(TH_F_S), TH_W_REG); cy += TH_DP(TH_H_LISTA);
        {
            char rl[96];
            if (HW.net.st == HW_AVAILABLE && HW.net_if[0])
                snprintf(rl, sizeof rl, "%s · %s · %s", HW.net_if, HW.net_state, HW.net_up ? "COM IP" : "SEM IP");
            else
                snprintf(rl, sizeof rl, "SEM INTERFACE");
            comp_row_list(x + TH_DP(TH_SP2), cy, w - 2 * TH_DP(TH_SP2), IC_WIFI, "Rede", rl,
                          HW.net_up ? TH_OK : TH_WARN, ACT_NONE, 0, 0);
        }
        cy += TH_DP(TH_H_LISTA);
        comp_row_kv(x + TH_DP(TH_SP4), cy + TH_DP(TH_SP2), w - 2 * TH_DP(TH_SP4), "Estado",
                    plat_persist_ok() ? "PERSISTENTE" : "SÓ MEMÓRIA (RO)", plat_persist_ok() ? TH_OK : TH_WARN, TH_DP(TH_F_S), TH_W_REG);
    }

    comp_button(x, y, w, TH_DP(TH_H_BOTAO_G), "DEVELOPER / DIAGNÓSTICO", IC_INFO, BK_NEUTRAL, ACT_PUSH, SC_DEVELOPER, 0, 0);
    y += TH_DP(TH_H_BOTAO_G) + TH_DP(TH_SP3);
    y += comp_wrap(x, y, w, TH_DP(TH_F_XS),
                   "O que é dinheiro neste aparelho é apenas livro-razão interno: saldos, Pix e trocas "
                   "são demonstração (selo presente em toda tela). O que é REAL está aqui nos ajustes e "
                   "no diagnóstico: brilho, bateria, carregador, temperatura, tela, toque, teclas, rede, "
                   "relógio e persistência.", TH_TXT3);
    return y;
}

/* ============================================================= DEVELOPER === */

/* Uma linha de diagnóstico: nome à esquerda, estado à direita, valor embaixo.
 * O valor é FORMATADO aqui dentro (variádico) — o chamador só passa o formato. */
static int dev_row(int y, int x, int w, const char *nome, const char *estado, uint32_t cor, const char *fmt, ...)
{
    char valor[192];
    valor[0] = 0;
    if (fmt && fmt[0]) {
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(valor, sizeof valor, fmt, ap);
        va_end(ap);
    }
    int px = TH_DP(TH_F_XS);
    th_text(x, y + TH_DP(TH_SP1), px, nome, TH_TXT, 0, TH_W_REG);
    th_text_r(x + w, y + TH_DP(TH_SP1), px, estado, cor, 0, TH_W_STRONG);
    /* O valor QUEBRA dentro da largura da lista. Antes ele saía pela direita e
     * ficava cortado — diagnóstico cortado é diagnóstico que mente por omissão. */
    int hval = 0;
    if (valor[0])
        hval = comp_wrap(x, y + TH_DP(TH_SP1) + px + TH_DP(2), w, TH_DP(TH_F_XS) - TH_DP(2), valor, TH_TXT3);
    int h = TH_DP(TH_SP1) * 2 + px + hval + TH_DP(TH_SP2);
    g_fill(x, y + h - 1, w, 1, TH_LINE);
    return h;
}

static uint32_t st_color(HwState s) { return s == HW_AVAILABLE ? TH_OK : s == HW_UNAVAILABLE ? TH_BAD : TH_WARN; }

static int scr_developer(int scroll)
{
    int y = th_top() + TH_DP(TH_SP4) - scroll;
    int x = th_cx(), w = th_cw();
    const UiDev *d = ui_dev();

    comp_caps(x, y, "DIAGNÓSTICO", TH_TXT3);
    y += TH_DP(TH_F_CAPS) + TH_DP(TH_SP3);

    /* ---- hardware, item por item, com estado e motivo ---- */
    comp_caps_clip(x, y, w, "HARDWARE (lido agora)", TH_TXT3); y += TH_DP(TH_F_CAPS) + TH_DP(TH_SP2);
    y += dev_row(y, x, w, "Bateria", hw_state_str(HW.batt.st), st_color(HW.batt.st), "%s", HW.batt.why);
    if (HW.batt.st == HW_AVAILABLE)
        y += dev_row(y, x, w, "", "", TH_TXT3, "%d%% · %s · %.2f V · %.1f C", HW.capacity, hw_batt_status_str(),
                     HW.voltage_uv / 1000000.0, HW.temp_dc / 10.0);
    y += dev_row(y, x, w, "Carregador", hw_state_str(HW.chg.st), st_color(HW.chg.st), "%s", HW.chg.why);
    if (HW.chg.st == HW_AVAILABLE)
        y += dev_row(y, x, w, "", "", TH_TXT3, "online=%d · tipo=%s", HW.chg_online, HW.chg_type[0] ? HW.chg_type : "?");
    y += dev_row(y, x, w, "Brilho", hw_state_str(HW.bl.st), st_color(HW.bl.st), "%s", HW.bl.why);
    if (HW.bl.st == HW_AVAILABLE)
        y += dev_row(y, x, w, "", "", TH_TXT3, "%d/%d = %d%% · %s", HW.bl_raw, HW.bl_max, HW.bl_pct, HW.bl_node);
    y += dev_row(y, x, w, "Vibração", hw_state_str(HW.vib.st), st_color(HW.vib.st), "%s", HW.vib.why);
    if (HW.vib.st == HW_AVAILABLE)
        y += dev_row(y, x, w, "", "", TH_TXT3, "estilo %d · %d escrita(s) nesta execução", HW.vib_style, HW.vib_writes);
    y += dev_row(y, x, w, "Tela", hw_state_str(HW.scr.st), st_color(HW.scr.st), "%.40s · %dx%d bpp=%d stride=%.0d B",
                 HW.fb_node, HW.scr_w, HW.scr_h, HW.scr_bpp, HW.scr_stride);
    y += dev_row(y, x, w, "Toque", hw_state_str(HW.touch.st), st_color(HW.touch.st), "%s '%s'",
                 HW.touch_node[0] ? HW.touch_node : "-", HW.touch_name[0] ? HW.touch_name : "-");
    y += dev_row(y, x, w, "Teclas", hw_state_str(HW.keys.st), st_color(HW.keys.st), "%d node(s) %s", HW.keys_n, HW.keys_desc);
    y += dev_row(y, x, w, "Rede", hw_state_str(HW.net.st), st_color(HW.net.st), "%s (%s)", HW.net.why, HW.net_state);
    y += TH_DP(TH_SP4);

    /* ---- toque: contadores vivos ---- */
    comp_caps_clip(x, y, w, "TOQUE (contadores vivos)", TH_TXT3); y += TH_DP(TH_F_CAPS) + TH_DP(TH_SP2);
    y += dev_row(y, x, w, "Eventos no node de toque", d->touch_ev ? "CHEGANDO" : "ZERO", d->touch_ev ? TH_OK : TH_BAD,
                 "down=%d · move=%d · up=%d · drop=%d", d->touch_down, d->touch_move, d->touch_up, d->touch_dropped);
    y += dev_row(y, x, w, "FW-KICK (firmware do toque)", d->touch_kicks ? "EXECUTADO" : "NÃO EXECUTADO",
                 d->touch_kicks ? TH_OK : TH_WARN, "%d ciclo(s) · fw_ver=%s", d->touch_kicks, d->fw_ver);
    y += TH_DP(TH_SP4);

    /* ---- auditoria da regra de 44 dp (a regra é conferida, não prometida) ---- */
    comp_caps_clip(x, y, w, "ALVOS DE TOQUE", TH_TXT3); y += TH_DP(TH_F_CAPS) + TH_DP(TH_SP2);
    {
        int abaixo = 0, min_dp = 0, total = ui_hitcount(&abaixo, &min_dp);
        y += dev_row(y, x, w, "Regra de 44 dp (quadro anterior)", abaixo ? "VIOLADA" : "OK",
                     abaixo ? TH_BAD : TH_OK, "%d alvo(s) · menor = %d dp · exigido %d dp", total, min_dp, TH_TOUCH_MIN_DP);
    }
    y += TH_DP(TH_SP4);

    /* ---- tempo de execução ---- */
    comp_caps(x, y, "EXECUÇÃO", TH_TXT3); y += TH_DP(TH_F_CAPS) + TH_DP(TH_SP2);
    y += dev_row(y, x, w, "PID 1", "VIVO", TH_OK, "uptime %lld s · %lu quadro(s) · serial=%s",
                 d->uptime_s, d->frames, d->serial_ok ? "ok" : "NÃO");
    y += dev_row(y, x, w, "Persistência", d->ro ? "SOMENTE LEITURA" : (d->persist_ok ? "ATIVA" : "SÓ MEMÓRIA"),
                 d->ro ? TH_WARN : (d->persist_ok ? TH_OK : TH_WARN),
                 d->ro ? "cmdline bankphone.ro=1 — nenhuma partição é gravada"
                       : "área vem da cmdline: bankphone.state=<partição> (sem isso, nada é gravado) · %s",
                 d->ro ? "" : (ST.part[0] ? ST.part : "sem área definida neste boot"));
    y += dev_row(y, x, w, "Modo", d->safe_mode ? "DIAGNÓSTICO" : "NORMAL", d->safe_mode ? TH_WARN : TH_OK,
                 "%s", d->safe_mode ? "sem tela ou sem fonte: o sistema segue vivo" : "UI em operação");
    y += TH_DP(TH_SP4);

    /* ---- persistência (F2): a área, o motivo e a prova de que não é palpite ---- */
    comp_caps_clip(x, y, w, "ESTADO PERSISTENTE (F2)", TH_TXT3); y += TH_DP(TH_F_CAPS) + TH_DP(TH_SP2);
    {
        const char *ar;
        uint32_t cor;
        if (d->ro)                              { ar = "BLOQUEADA (ro=1)"; cor = TH_WARN; }
        else if (!ST.pedido)                    { ar = "NÃO PEDIDA";         cor = TH_TXT3; }
        else if (ST.ok)                         { ar = ST.claim_now ? "TOMADA AGORA" : "ARMADA"; cor = TH_OK; }
        else                                    { ar = "RECUSADA";          cor = TH_BAD;  }
        y += dev_row(y, x, w, "Área de estado", ar, cor, "%s", ST.why);
        if (ST.part[0])
            y += dev_row(y, x, w, "", "", TH_TXT3,
                         "'%s' (%s): %llu MiB, região de %llu KiB em +%llu KiB do início "
                         "(é o FIM da partição) · erase=%ld",
                         ST.part, ST.node, ST.part_bytes >> 20,
                         ST.span >> 10, ST.base >> 10, ST.erase);
        if (ST.pedido && ST.ok) {
            y += dev_row(y, x, w, "Slots", ST.recovered ? "RECUPERADO" : "NORMAIS",
                         ST.recovered ? TH_WARN : TH_OK,
                         "A seq=%u %s · B seq=%u %s · %u gravação(ões) · %u recusa(s)",
                         ST.seq[0], ST.slot_ok[0] == 1 ? "ok" : ST.slot_ok[0] == 2 ? "PENDENTE" : "vazio",
                         ST.seq[1], ST.slot_ok[1] == 1 ? "ok" : ST.slot_ok[1] == 2 ? "PENDENTE" : "vazio",
                         ST.saves, ST.refusals);
            if (ST.hash_atual[0])
                y += dev_row(y, x, w, "Conferência do backup", ST.claimed ? "DISPENSADA" : "CONFERIDA",
                             ST.claimed ? TH_TXT3 : TH_OK,
                             ST.claimed
                               ? "esta área já era deste sistema: o hash do backup vale só na primeira tomada. medido agora %.16s"
                               : "hash do backup %.16s confere com a região medida agora", ST.hash_atual);
        } else if (ST.pedido) {
            y += dev_row(y, x, w, "O que fazer", "SEM GRAVAR", TH_WARN,
                         "lida do backup %.16s · medida %.16s — iguais? então repita com bankphone.statehash=",
                         ST.hash_esperado[0] ? ST.hash_esperado : "(vazio)", ST.hash_atual[0] ? ST.hash_atual : "(vazio)");
        }
    }
    y += TH_DP(TH_SP4);

    /* ---- ações de teste (cada uma com prova) ---- */
    comp_button(x, y, w, TH_DP(TH_H_BOTAO), "RELER HARDWARE AGORA", IC_SETTINGS, BK_NEUTRAL, ACT_HW_REFRESH, 0, 0, 0);
    y += TH_DP(TH_H_BOTAO) + TH_DP(TH_SP2);
    comp_button(x, y, w, TH_DP(TH_H_BOTAO), "VIBRAR 500 ms (prova)", IC_WARNING, BK_NEUTRAL, ACT_VIB_TEST, 500, 0, HW.vib.st != HW_AVAILABLE);
    y += TH_DP(TH_H_BOTAO) + TH_DP(TH_SP2);
    comp_button(x, y, w, TH_DP(TH_H_BOTAO), "TESTAR FOLHA DE AVISO", IC_INFO, BK_NEUTRAL, ACT_NOTICE_TEST, 0, 0, 0);
    y += TH_DP(TH_H_BOTAO) + TH_DP(TH_SP4);
    y += comp_wrap(x, y, w, TH_DP(TH_F_XS),
                   "Regra Zero: nenhum caminho de hardware é presumido. O que não existe aparece "
                   "UNAVAILABLE com o motivo; o que existe mas não responde aparece UNKNOWN. "
                   "Os estados vêm de leitura real, no aparelho. Nesta tela do Mac, os valores "
                   "são os do próprio Mac — quase tudo NOT AVAILABLE, de propósito.",
                   TH_TXT3);
    return y + scroll + TH_DP(TH_SP4);
}

/* ============================================================== despacho === */

int screens_draw(int scr, int scroll)
{
    switch (scr) {
    case SC_MONEY:     return scr_money(scroll);
    case SC_ACTIVITY:  return scr_activity(scroll);
    case SC_WALLET:    return scr_wallet(scroll);
    case SC_SECURITY:  return scr_security(scroll);
    case SC_SETTINGS:  return scr_settings(scroll);
    case SC_DEVELOPER: return scr_developer(scroll);
    default:           return th_top();
    }
}

/* títulos usados pela barra de status nas telas empilhadas */
const char *screen_title(int scr)
{
    switch (scr) {
    case SC_SETTINGS:  return "AJUSTES";
    case SC_DEVELOPER: return "DIAGNÓSTICO";
    default:           return "";
    }
}

int screen_is_tab(int scr) { return scr >= SC_MONEY && scr <= SC_SECURITY; }
