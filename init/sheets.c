/*
 * BANKPHONE OS — FOLHAS (bottom sheets): receber, escolher contato, valor,
 * revisão, PIN, 2ª confirmação, recibo e aviso.
 *
 * Toda folha declara: título em caixa alta, selo de ambiente e o aviso de que
 * é DEMO quando algum valor está envolvido. Nada aqui move dinheiro real.
 */
#include "ui.h"
#include "nav.h"
#include "components.h"
#include "icons.h"
#include "money.h"
#include "sec.h"
#include "hw.h"

#include <stdio.h>
#include <string.h>
#include <time.h>


/* ------------------------------------------------------------------ apoio -- */

static void aviso_demo(int x, int y, int w, const char *texto)
{
    (void)w;
    int h = TH_DP(TH_SP3) * 2 + TH_DP(TH_F_XS) * 2 + TH_DP(TH_SP2);
    g_rrect(x, y, th_W - 2 * (x - TH_DP(TH_SP2)), h, TH_DP(TH_RAIO_S), TH_SURF2, 255);
    icon(IC_WARNING, x + TH_DP(TH_SP3) + TH_DP(8), y + h / 2, TH_DP(16), TH_DEMO);
    comp_wrap(x + TH_DP(TH_SP3) + TH_DP(TH_SP5), y + TH_DP(TH_SP3), th_W - 2 * x - TH_DP(TH_SP6),
              TH_DP(TH_F_XS), texto, TH_TXT2);
}

static void botao_fechar(int x, int y, int w)
{
    comp_button(x, y, w, TH_DP(TH_H_BOTAO), "FECHAR", IC_CLOSE, BK_NEUTRAL, ACT_CLOSE, 0, 0, 0);
}

/* =============================================================== RECEBER == */

static void sheet_receive(void)
{
    int x, w;
    int y = comp_sheet_begin(TH_DP(300), &x, &w);
    comp_sheet_title(x, y, "RECEBER", "DEMO");
    y += TH_DP(TH_F_CAPS) + TH_DP(TH_SP4);

    char l[160];
    comp_row_kv(x, y, w, "Chave Pix", "demo@bankphone.local", TH_TXT2, TH_DP(TH_F_S), TH_W_REG);
    y += TH_DP(TH_H_LISTA);
    g_fill(x, y, w, 1, TH_LINE);
    y += TH_DP(TH_SP3);
    snprintf(l, sizeof l, "QR code: NÃO IMPLEMENTADO (precisa do gerador próprio)");
    comp_label(x, y, l, TH_WARN, TH_DP(TH_F_XS), TH_W_REG);
    y += TH_DP(TH_F_XS) + TH_DP(TH_SP3);
    y += comp_wrap(x, y, w, TH_DP(TH_F_XS),
                   "Nenhum Pix real chega aqui. Para exercitar o fluxo, simule uma entrada DEMO:",
                   TH_TXT2) + TH_DP(TH_SP3);
    comp_button(x, y, w, TH_DP(TH_H_BOTAO), "SIMULAR ENTRADA DE R$ 100", IC_RECEIVE, BK_NEUTRAL, ACT_DEMOPIX, 10000, 0, 0);
    y += TH_DP(TH_H_BOTAO) + TH_DP(TH_SP2);
    comp_button(x, y, w, TH_DP(TH_H_BOTAO), "SIMULAR ENTRADA DE R$ 1.000", IC_RECEIVE, BK_NEUTRAL, ACT_DEMOPIX, 100000, 0, 0);
}

/* ============================================================ QUEM (PIX) == */

static void sheet_who(void)
{
    int x, w;
    int y = comp_sheet_begin(TH_DP(360), &x, &w);
    comp_sheet_title(x, y, "PARA QUEM?", "CONTATOS DEMO");
    y += TH_DP(TH_F_CAPS) + TH_DP(TH_SP4);
    for (int i = 0; i < 3; i++) {
        comp_button(x, y, w, TH_DP(TH_H_LISTA), SHEET_CONTACTS[i], -1, BK_NEUTRAL, ACT_WHO, i, 0, 0);
        y += TH_DP(TH_H_LISTA) + TH_DP(TH_SP2);
    }
    y += TH_DP(TH_SP2);
    comp_label(x, y, "Digitar chave livre exige teclado alfanumérico (ainda não construído).",
               TH_TXT3, TH_DP(TH_F_XS), TH_W_REG);
}

/* ============================================================== VALOR ===== */

static void sheet_amount(void)
{
    int x, w;
    int y = comp_sheet_begin(TH_DP(430), &x, &w);
    char t[128], b[64];

    if (SH.amt_mode == 0) {
        snprintf(t, sizeof t, "ENVIAR PIX PARA %.20s", SHEET_CONTACTS[SH.who]);
        comp_sheet_title(x, y, t, "DEMO");
    } else {
        comp_sheet_title(x, y, "TROCAR", "DEMO");
    }
    y += TH_DP(TH_F_CAPS) + TH_DP(TH_SP4);

    int asset = SH.amt_mode == 0 ? A_BRL : SH.swap_from;
    sheet_amt_str(b, sizeof b, asset);
    comp_num_c(th_W / 2, y, TH_DP(TH_F_XL), b, b[1] == '0' && !strchr(b, ',') ? TH_TXT3 : TH_TXT, TH_W_STRONG);
    y += TH_DP(TH_F_XL) + TH_DP(TH_SP2);

    {
        char av[64];
        fmt_money(av, sizeof av, asset, m_balance(asset));
        snprintf(t, sizeof t, "Disponível %s", av);
        comp_label_c(th_W / 2, y, t, TH_TXT3, TH_DP(TH_F_XS), TH_W_REG);
        y += TH_DP(TH_F_XS) + TH_DP(TH_SP2);
    }

    int val = sheet_amt_value(asset);
    if (SH.amt_mode == 1) {
        ui_hit_add(x, y, w, TH_DP(TH_H_LISTA), ACT_FLIP, 0);
        comp_label_c(th_W / 2, y + TH_DP(TH_SP2), "tocar para inverter a direção", TH_ACC, TH_DP(TH_F_XS), TH_W_REG);
        y += TH_DP(TH_H_LISTA) - TH_DP(TH_SP2);
        Quote q;
        int to = SH.swap_from == A_BRL ? A_USDC : A_BRL;
        if (val > 0 && m_quote(SH.swap_from, to, val, plat_now(), &q)) {
            fmt_money(b, sizeof b, to, q.to_amt);
            snprintf(t, sizeof t, "Você recebe %s", b);
            comp_label_c(th_W / 2, y, t, TH_ACC, TH_DP(TH_F_S), TH_W_STRONG);
            y += TH_DP(TH_F_S) + TH_DP(TH_SP1);
            comp_label_c(th_W / 2, y, "taxa DEMO 1 USDC = R$ 5,60 · tarifa R$ 3,50 · sem transação on-chain",
                         TH_TXT3, TH_DP(TH_F_XS), TH_W_REG);
            y += TH_DP(TH_F_XS) + TH_DP(TH_SP2);
        } else if (val > 0) {
            comp_label_c(th_W / 2, y, "Valor pequeno demais para cobrir a tarifa.", TH_BAD, TH_DP(TH_F_XS), TH_W_REG);
            y += TH_DP(TH_F_XS) + TH_DP(TH_SP2);
        } else {
            y += TH_DP(TH_SP5);
        }
    } else {
        y += TH_DP(TH_SP2);
    }

    y += comp_keypad(x, y, w);
    y += TH_DP(TH_SP3);
    comp_button(x, y, w, TH_DP(TH_H_BOTAO), "CONTINUAR", IC_CHEVRON,
                val > 0 ? BK_PRIMARY : BK_NEUTRAL, ACT_CONT, 0, 0, val <= 0);
}

/* ============================================================= REVISÃO ==== */

static void sheet_review(void)
{
    int idx = sheet_pend_tx();
    if (idx < 0) { return; }
    Tx *t = &M.tx[idx];
    int x, w;
    int y = comp_sheet_begin(TH_DP(360), &x, &w);
    comp_sheet_title(x, y, "CONFIRMAR TRANSAÇÃO", "DEMO");
    y += TH_DP(TH_F_CAPS) + TH_DP(TH_SP4);

    char b[64];
    if (t->type == T_SWAP) {
        fmt_money(b, sizeof b, t->from, t->from_amt);
        comp_row_kv(x, y, w, "Trocar", b, TH_TXT, TH_DP(TH_F_S), TH_W_REG); y += TH_DP(TH_H_LISTA);
        fmt_money(b, sizeof b, t->to, t->to_amt);
        comp_row_kv(x, y, w, "Você recebe", b, TH_ACC, TH_DP(TH_F_S), TH_W_REG); y += TH_DP(TH_H_LISTA);
    } else {
        fmt_money(b, sizeof b, A_BRL, t->from_amt);
        comp_row_kv(x, y, w, "Enviar", b, TH_TXT, TH_DP(TH_F_S), TH_W_REG); y += TH_DP(TH_H_LISTA);
        comp_row_kv(x, y, w, "Para", t->cp, TH_TXT, TH_DP(TH_F_S), TH_W_REG); y += TH_DP(TH_H_LISTA);
    }
    fmt_money(b, sizeof b, A_BRL, t->fee_brl);
    comp_row_kv(x, y, w, "Tarifa", b, TH_TXT, TH_DP(TH_F_S), TH_W_REG); y += TH_DP(TH_H_LISTA);
    {
        Level need = m_required(t);
        comp_row_kv(x, y, w, "Autorização",
                    need == L_PIN ? "PIN" : need == L_CONFIRM ? "PIN + CONFIRMAÇÃO" : "PIN + CONFIRMAÇÃO + ESPERA",
                    TH_TXT2, TH_DP(TH_F_S), TH_W_REG);
        y += TH_DP(TH_H_LISTA) + TH_DP(TH_SP2);
    }
    aviso_demo(x, y, w, "Transação de demonstração. Nenhum dinheiro real se move e nenhuma transação sai do aparelho.");
    y += TH_DP(TH_SP3) + TH_DP(TH_F_XS) * 2 + TH_DP(TH_SP3);
    {
        int bw = (w - TH_DP(TH_SP3)) / 2;
        comp_button(x, y, bw, TH_DP(TH_H_BOTAO), "CANCELAR", -1, BK_NEUTRAL, ACT_CANCEL, 0, 0, 0);
        comp_button(x + bw + TH_DP(TH_SP3), y, bw, TH_DP(TH_H_BOTAO), "AUTORIZAR", IC_LOCK, BK_PRIMARY, ACT_AUTH, 0, 0, 0);
    }
}

/* ================================================================= PIN ==== */

static void sheet_pin(void)
{
    int x, w;
    int y = comp_sheet_begin(TH_DP(430), &x, &w);
    comp_sheet_title(x, y, SH.purpose ? "PIN PARA DESLIGAR EMERGÊNCIA" : "PIN PARA AUTORIZAR", NULL);
    y += TH_DP(TH_F_CAPS) + TH_DP(TH_SP5);
    comp_dots(th_W / 2, y + TH_DP(TH_SP1), SH.pin_len, 6);
    y += TH_DP(TH_SP6);
    if (SH.locked_s > 0) {
        char l[64];
        snprintf(l, sizeof l, "Bloqueado. Tente de novo em %d s", SH.locked_s);
        comp_label_c(th_W / 2, y, l, TH_BAD, TH_DP(TH_F_XS), TH_W_REG);
    } else if (SH.msg[0]) {
        comp_label_c(th_W / 2, y, SH.msg, TH_BAD, TH_DP(TH_F_XS), TH_W_REG);
    }
    y += TH_DP(TH_F_XS) + TH_DP(TH_SP3);
    y += comp_keypad(x, y, w);
    y += TH_DP(TH_SP3);
    comp_button(x, y, w, TH_DP(TH_H_BOTAO), "CANCELAR", -1, BK_NEUTRAL, ACT_CANCEL, 0, 0, 0);
}

/* ===================================================== 2ª CONFIRMAÇÃO ==== */

static void sheet_confirm2(void)
{
    int idx = sheet_pend_tx();
    if (idx < 0) return;
    Tx *t = &M.tx[idx];
    int x, w;
    int y = comp_sheet_begin(TH_DP(330), &x, &w);
    comp_sheet_title(x, y, "SEGUNDA CONFIRMAÇÃO", "DEMO");
    y += TH_DP(TH_F_CAPS) + TH_DP(TH_SP4);

    char b[64], l[160];
    fmt_money(b, sizeof b, t->from, t->from_amt);
    snprintf(l, sizeof l, "%s %s", t->type == T_SWAP ? "Trocar" : "Enviar", b);
    comp_label(x, y, l, TH_TXT, TH_DP(TH_F_L), TH_W_STRONG);
    y += TH_DP(TH_F_L) + TH_DP(TH_SP2);
    if (t->type == T_PIX_OUT) {
        snprintf(l, sizeof l, "para %s", t->cp);
        comp_label(x, y, l, TH_TXT2, TH_DP(TH_F_S), TH_W_REG);
        y += TH_DP(TH_F_S) + TH_DP(TH_SP2);
    }
    Level need = m_required(t);
    long long left = M.pol.cooldown_s - (plat_now() - t->created);
    int espera = (need == L_COOLDOWN && left > 0);
    if (espera) {
        snprintf(l, sizeof l, "Espera de segurança: %lld s", left);
        comp_label(x, y, l, TH_WARN, TH_DP(TH_F_S), TH_W_STRONG);
    } else {
        comp_label(x, y, "Valor e destinatário conferidos.", TH_TXT2, TH_DP(TH_F_S), TH_W_REG);
    }
    y += TH_DP(TH_F_S) + TH_DP(TH_SP5);
    {
        int bw = (w - TH_DP(TH_SP3)) / 2;
        comp_button(x, y, bw, TH_DP(TH_H_BOTAO), "CANCELAR", -1, BK_NEUTRAL, ACT_CANCEL, 0, 0, 0);
        comp_button(x + bw + TH_DP(TH_SP3), y, bw, TH_DP(TH_H_BOTAO), "CONFIRMAR", IC_CHECK,
                    espera ? BK_NEUTRAL : BK_PRIMARY, ACT_CONFIRM2, 0, 0, espera);
    }
}

/* ============================================================== RECIBO ==== */

static void sheet_receipt(void)
{
    int idx = sheet_rcpt_tx();
    if (idx < 0) return;
    Tx *t = &M.tx[idx];
    int x, w;
    int y = comp_sheet_begin(TH_DP(420), &x, &w);
    comp_sheet_title(x, y, "COMPROVANTE", "DEMO");
    y += TH_DP(TH_F_CAPS) + TH_DP(TH_SP4);

    char b[64], l[160];
    const char *nome = t->type == T_PIX_IN ? "Entrada DEMO" : t->type == T_PIX_OUT ? "Pix DEMO" : "Troca DEMO";
    snprintf(l, sizeof l, "%s%s", t->st == S_CONFIRMED ? "" : "", m_state_name(t->st));
    comp_label(x, y, nome, TH_TXT, TH_DP(TH_F_M), TH_W_REG); y += TH_DP(TH_F_M) + TH_DP(TH_SP1);
    comp_label(x, y, l, t->st == S_CONFIRMED ? TH_OK : t->st == S_FAILED ? TH_BAD : TH_WARN, TH_DP(TH_F_L), TH_W_STRONG);
    y += TH_DP(TH_F_L) + TH_DP(TH_SP3);

    comp_row_kv(x, y, w, "Ambiente", "DEMO", TH_DEMO, TH_DP(TH_F_S), TH_W_REG); y += TH_DP(TH_SP6);
    if (t->from != A_NONE) { fmt_money(b, sizeof b, t->from, t->from_amt); comp_row_kv(x, y, w, "De", b, TH_TXT, TH_DP(TH_F_S), TH_W_REG); y += TH_DP(TH_SP6); }
    if (t->to != A_NONE) { fmt_money(b, sizeof b, t->to, t->to_amt); comp_row_kv(x, y, w, "Para", b, TH_TXT, TH_DP(TH_F_S), TH_W_REG); y += TH_DP(TH_SP6); }
    if (t->fee_brl) { fmt_money(b, sizeof b, A_BRL, t->fee_brl); comp_row_kv(x, y, w, "Tarifa", b, TH_TXT, TH_DP(TH_F_S), TH_W_REG); y += TH_DP(TH_SP6); }
    comp_row_kv(x, y, w, "Hash on-chain", t->hash[0] ? t->hash : "NENHUM (DEMO)", TH_TXT3, TH_DP(TH_F_S), TH_W_REG); y += TH_DP(TH_SP6);
    snprintf(l, sizeof l, "ID %s", t->id);
    comp_label(x, y, l, TH_TXT3, TH_DP(TH_F_XS), TH_W_REG);
    y += TH_DP(TH_F_XS) + TH_DP(TH_SP4);
    botao_fechar(x, y, w);
}

/* ============================================================== AVISO ===== */

static void sheet_notice(void)
{
    int x, w;
    /* altura calculada a partir do texto: aviso nunca é cortado */
    int px = TH_DP(TH_F_S);
    int linhas = 1, larg = th_W - 2 * TH_DP(TH_SP5) - 2 * TH_DP(TH_SP2);
    int usados = 0;
    for (const char *p = SH.msg; *p; ) {
        const char *fim = strchr(p, ' ');
        int n = fim ? (int)(fim - p) : (int)strlen(p);
        char palavra[192];
        if (n >= (int)sizeof palavra) n = (int)sizeof palavra - 1;
        memcpy(palavra, p, (size_t)n); palavra[n] = 0;
        int wp = th_text_w(px, palavra, 0, TH_W_REG);
        if (usados && usados + TH_DP(TH_SP2) + wp > larg) { linhas++; usados = wp; }
        else usados += (usados ? TH_DP(TH_SP2) : 0) + wp;
        p += n; while (*p == ' ') p++;
    }
    int h = TH_DP(TH_SP6) + TH_DP(TH_F_CAPS) + TH_DP(TH_SP4) + linhas * (px * 14 / 10) + TH_DP(TH_SP4)
          + TH_DP(TH_H_BOTAO) + TH_DP(TH_SP5);
    int y = comp_sheet_begin(h, &x, &w);
    comp_sheet_title(x, y, "AVISO", NULL);
    y += TH_DP(TH_F_CAPS) + TH_DP(TH_SP4);
    y += comp_wrap(x, y, w, TH_DP(TH_F_S), SH.msg, TH_TXT) + TH_DP(TH_SP4);
    botao_fechar(x, y, w);
}

/* ============================================================= despacho === */

void sheets_draw(int sheet)
{
    switch (sheet) {
    case SH_RECEIVE:   sheet_receive(); break;
    case SH_WHO:       sheet_who(); break;
    case SH_AMOUNT:    sheet_amount(); break;
    case SH_REVIEW:    sheet_review(); break;
    case SH_PIN:       sheet_pin(); break;
    case SH_CONFIRM2:  sheet_confirm2(); break;
    case SH_RECEIPT:   sheet_receipt(); break;
    case SH_NOTICE:    sheet_notice(); break;
    default: break;
    }
}
