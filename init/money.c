#include "money.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

Money M;
#define RATE 560          // DEMO: 1 USDC = R$ 5,60 (centavos por USDC)
#define SERVICE_FEE 350   // DEMO: R$ 3,50
#define QUOTE_TTL 20
static const int64_t UNIT[A_N] = {100, 1000000};

static int allowed(TxState a, TxState b) {
    switch (a) {
    case S_CREATED: return b == S_QUOTED || b == S_AWAITING_AUTH || b == S_AUTHORIZED || b == S_CANCELLED || b == S_FAILED;
    case S_QUOTED: return b == S_AWAITING_AUTH || b == S_CANCELLED || b == S_EXPIRED;
    case S_AWAITING_AUTH: return b == S_AUTHORIZED || b == S_CANCELLED || b == S_EXPIRED || b == S_FAILED;
    case S_AUTHORIZED: return b == S_SIGNED || b == S_PENDING || b == S_CONFIRMED || b == S_FAILED;
    case S_SIGNED: return b == S_BROADCASTING || b == S_FAILED;
    case S_BROADCASTING: return b == S_PENDING || b == S_FAILED;
    case S_PENDING: return b == S_CONFIRMED || b == S_FAILED;
    default: return 0;
    }
}
static int advance(Tx *t, TxState n) { if (!allowed(t->st, n)) return 0; t->st = n; return 1; }

const char *m_state_name(TxState s) {
    static const char *N[] = {"CREATED", "QUOTED", "AWAITING AUTH", "AUTHORIZED", "SIGNED", "BROADCASTING", "PENDING", "CONFIRMED", "FAILED", "CANCELLED", "EXPIRED"};
    return N[s];
}

void m_init(void) {
    memset(&M, 0, sizeof M);
    M.pol.pin_max = 50000; M.pol.confirm_max = 500000; M.pol.per_tx = 200000; M.pol.daily = 500000; M.pol.cooldown_s = 30;
}

static Tx *find_idem(const char *k) { for (int i = 0; i < M.n; i++) if (!strcmp(M.tx[i].idem, k)) return &M.tx[i]; return NULL; }

static Tx *add(TxType ty, const char *idem, int64_t now, int from, int64_t fa, int to, int64_t ta, int64_t fee, const char *cp) {
    Tx *ex = find_idem(idem); if (ex) return ex;
    if (M.n >= MAX_TX) return NULL;
    Tx *t = &M.tx[M.n++]; memset(t, 0, sizeof *t);
    snprintf(t->id, sizeof t->id, "%08x", (unsigned)(0x9e3779b1u * (++M.seq) ^ (unsigned)now));
    snprintf(t->idem, sizeof t->idem, "%s", idem); snprintf(t->cp, sizeof t->cp, "%s", cp ? cp : "");
    t->type = ty; t->st = S_CREATED; t->from = from; t->to = to; t->from_amt = fa; t->to_amt = ta; t->fee_brl = fee; t->created = now;
    return t;
}

int64_t m_balance(int a) {
    int64_t b = 0;
    for (int i = 0; i < M.n; i++) { Tx *t = &M.tx[i]; if (t->st != S_CONFIRMED) continue; if (t->from == a) b -= t->from_amt; if (t->to == a) b += t->to_amt; }
    return b;
}
int64_t m_total_brl(void) { return m_balance(A_BRL) + m_balance(A_USDC) * RATE / UNIT[A_USDC]; }

void m_load_demo(int64_t now) {
    static const struct { const char *k; int a; int64_t v; } F[] = {{"demo-fund-brl", A_BRL, 1000000}, {"demo-fund-usdc", A_USDC, 100000000}};
    for (int i = 0; i < 2; i++) {
        int existed = find_idem(F[i].k) != NULL;
        Tx *t = add(T_FUNDING, F[i].k, now, A_NONE, 0, F[i].a, F[i].v, 0, "DEMO");
        if (t && !existed) { advance(t, S_AUTHORIZED); advance(t, S_CONFIRMED); }
    }
}

Tx *m_receive_pix(int64_t cents, const char *from, int64_t now) {
    char k[48]; snprintf(k, sizeof k, "in-%lld-%u", (long long)now, ++M.seq);
    Tx *t = add(T_PIX_IN, k, now, A_NONE, 0, A_BRL, cents, 0, from);
    if (t) { advance(t, S_AUTHORIZED); advance(t, S_CONFIRMED); }
    return t;
}

int64_t m_pix_spent_since(int64_t since) {
    int64_t s = 0;
    for (int i = 0; i < M.n; i++) { Tx *t = &M.tx[i]; if (t->type != T_PIX_OUT || t->created < since) continue; if (t->st == S_CANCELLED || t->st == S_FAILED || t->st == S_EXPIRED) continue; s += t->from_amt; }
    return s;
}

Tx *m_prepare_pix(const char *dest, int64_t cents, int64_t now, const char **err) {
    if (M.emergency) { *err = "EMERGENCY MODE: financial operations locked."; return NULL; }
    if (cents <= 0) { *err = "Invalid amount."; return NULL; }
    if (!dest || strlen(dest) < 3) { *err = "Choose a recipient."; return NULL; }
    if (cents > m_balance(A_BRL)) { *err = "Insufficient BRL balance. Nothing was moved."; return NULL; }
    if (cents > M.pol.per_tx) { *err = "LIMITE: acima do teto por transação."; return NULL; }
    if (m_pix_spent_since(now - 86400) + cents > M.pol.daily) { *err = "LIMITE: teto diário estourado."; return NULL; }
    char k[48]; snprintf(k, sizeof k, "out-%lld-%u", (long long)now, ++M.seq);
    Tx *t = add(T_PIX_OUT, k, now, A_BRL, cents, A_NONE, 0, 0, dest);
    if (!t) { *err = "Ledger full."; return NULL; }
    advance(t, S_AWAITING_AUTH);
    return t;
}

int m_quote(int from, int to, int64_t amt, int64_t now, Quote *q) {
    memset(q, 0, sizeof *q);
    if (amt <= 0 || from == to) return 0;
    int64_t out;
    if (from == A_BRL && to == A_USDC) { int64_t net = amt - SERVICE_FEE; if (net <= 0) return 0; out = net * UNIT[A_USDC] / RATE; }
    else if (from == A_USDC && to == A_BRL) { int64_t gross = amt * RATE / UNIT[A_USDC]; out = gross - SERVICE_FEE; if (out <= 0) return 0; }
    else return 0;
    q->from = from; q->to = to; q->from_amt = amt; q->to_amt = out; q->rate = RATE; q->fee_brl = SERVICE_FEE; q->expires = now + QUOTE_TTL; q->ok = 1;
    snprintf(q->id, sizeof q->id, "q%u", ++M.seq);
    return 1;
}

// cotação ativa de cada swap pendente (por id da tx)
static struct { char id[12]; int64_t expires; } pend[8];
static void pend_set(const char *id, int64_t e) { for (int i = 0; i < 8; i++) if (!pend[i].id[0]) { snprintf(pend[i].id, 12, "%s", id); pend[i].expires = e; return; } }
static int64_t pend_get(const char *id) { for (int i = 0; i < 8; i++) if (!strcmp(pend[i].id, id)) return pend[i].expires; return -1; }
static void pend_del(const char *id) { for (int i = 0; i < 8; i++) if (!strcmp(pend[i].id, id)) pend[i].id[0] = 0; }

Tx *m_prepare_swap(const Quote *q, int64_t now, const char **err) {
    if (M.emergency) { *err = "EMERGENCY MODE: financial operations locked."; return NULL; }
    if (!q || !q->ok) { *err = "Quote unavailable. Nothing was moved."; return NULL; }
    if (now > q->expires) { *err = "Quote expired. Ask for a new one."; return NULL; }
    if (q->from_amt > m_balance(q->from)) { *err = "Insufficient balance. Nothing was moved."; return NULL; }
    char k[48], cp[40]; snprintf(k, sizeof k, "swap-%s-%lld", q->id, (long long)now);
    snprintf(cp, sizeof cp, "%s>%s", q->from == A_BRL ? "BRL" : "USDC", q->to == A_BRL ? "BRL" : "USDC");
    Tx *t = add(T_SWAP, k, now, q->from, q->from_amt, q->to, q->to_amt, q->fee_brl, cp);
    if (!t) { *err = "Ledger full."; return NULL; }
    advance(t, S_QUOTED); advance(t, S_AWAITING_AUTH);
    pend_set(t->id, q->expires);
    return t;
}

static int64_t brl_value(const Tx *t) {
    if (t->from == A_BRL) return t->from_amt;
    if (t->from == A_USDC) return t->from_amt * RATE / UNIT[A_USDC];
    return t->to_amt;
}
Level m_required(const Tx *t) { int64_t v = brl_value(t); return v <= M.pol.pin_max ? L_PIN : v <= M.pol.confirm_max ? L_CONFIRM : L_COOLDOWN; }

const char *m_authorize(Tx *t, Level got, int64_t now) {
    if (t->st != S_AWAITING_AUTH) return "Transaction is not awaiting authorization.";
    if (M.emergency) { advance(t, S_FAILED); snprintf(t->fail, sizeof t->fail, "Emergency Mode"); return "EMERGENCY MODE active."; }
    Level need = m_required(t);
    if (got < need) return "Stronger authorization required.";
    if (need == L_COOLDOWN && now - t->created < M.pol.cooldown_s) return "ESPERA: aguarde antes de autorizar.";
    if (t->type == T_SWAP) { int64_t e = pend_get(t->id); if (e < 0 || now > e) { advance(t, S_EXPIRED); pend_del(t->id); return "Quote expired. Nothing was moved."; } }
    if (t->from != A_NONE && t->from_amt > m_balance(t->from)) { advance(t, S_FAILED); snprintf(t->fail, sizeof t->fail, "Insufficient balance"); return "Insufficient balance. Nothing was moved."; }
    advance(t, S_AUTHORIZED);
    if (t->type == T_PIX_OUT) {
        if (M.pix_offline) { advance(t, S_FAILED); snprintf(t->fail, sizeof t->fail, "Pix provider unavailable. No funds moved."); return t->fail; }
        advance(t, S_CONFIRMED);                    // provedor MOCK: confirma sem mover dinheiro real
    } else if (t->type == T_SWAP) {
        advance(t, S_CONFIRMED); pend_del(t->id);   // DEMO: sem on-chain, sem hash
    }
    return NULL;
}

void m_cancel(Tx *t, int64_t now) { (void)now; if (allowed(t->st, S_CANCELLED)) { t->st = S_CANCELLED; pend_del(t->id); } }

// ---- formatação ----
void fmt_money(char *out, size_t n, int asset, int64_t v) {
    int neg = v < 0; if (neg) v = -v;
    int64_t whole = v / UNIT[asset], frac = (v % UNIT[asset]) / (asset == A_BRL ? 1 : 10000);
    char w[32]; snprintf(w, sizeof w, "%lld", (long long)whole);
    if (asset == A_BRL) {
        char g[48]; int L = (int)strlen(w), j = 0;
        for (int i = 0; i < L; i++) { if (i && (L - i) % 3 == 0) g[j++] = '.'; g[j++] = w[i]; } g[j] = 0;
        snprintf(out, n, "%sR$ %s,%02lld", neg ? "-" : "", g, (long long)frac);
    } else snprintf(out, n, "%s$ %s.%02lld", neg ? "-" : "", w, (long long)frac);
}
int64_t parse_cents(const char *d) { int64_t v = 0; for (; *d; d++) { if (*d < '0' || *d > '9') return -1; v = v * 10 + (*d - '0'); if (v > 100000000000LL) return -1; } return v; }

// ---- persistência (texto, uma tx por linha) ----
size_t m_serialize(char *buf, size_t cap) {
    size_t o = 0;
    o += snprintf(buf + o, cap - o, "V1 %u %d\n", M.seq, M.emergency);
    for (int i = 0; i < M.n && o + 400 < cap; i++) {
        Tx *t = &M.tx[i];
        o += snprintf(buf + o, cap - o, "T|%s|%s|%d|%d|%d|%d|%lld|%lld|%lld|%lld|%s|%s|%s\n", t->id, t->idem, t->type, t->st, t->from, t->to,
                      (long long)t->from_amt, (long long)t->to_amt, (long long)t->fee_brl, (long long)t->created, t->cp, t->fail, t->hash);
    }
    return o;
}
int m_deserialize(const char *buf) {
    m_init();
    char *b = strdup(buf), *sv = NULL; int ok = 1;
    for (char *l = strtok_r(b, "\n", &sv); l; l = strtok_r(NULL, "\n", &sv)) {
        if (l[0] == 'V') { unsigned s; int e; if (sscanf(l, "V1 %u %d", &s, &e) == 2) { M.seq = s; M.emergency = e; } else ok = 0; continue; }
        if (l[0] != 'T' || !l[1] || M.n >= MAX_TX) continue;   // linha "T" sozinha: l + 2 passaria do fim da string
        Tx *t = &M.tx[M.n]; memset(t, 0, sizeof *t);
        char id[16], idem[64], cp[48], fail[100], hash[80]; int ty, st, f, to; long long fa, ta, fee, cr;
        char *p = l + 2; char *f_[13]; int k = 0; f_[k++] = p;
        for (; *p && k < 13; p++) if (*p == '|') { *p = 0; f_[k++] = p + 1; }
        (void)id; (void)idem; (void)cp; (void)fail; (void)hash;
        if (k < 13) { ok = 0; continue; }
        snprintf(t->id, sizeof t->id, "%s", f_[0]); snprintf(t->idem, sizeof t->idem, "%s", f_[1]);
        ty = atoi(f_[2]); st = atoi(f_[3]); f = atoi(f_[4]); to = atoi(f_[5]); fa = atoll(f_[6]); ta = atoll(f_[7]); fee = atoll(f_[8]); cr = atoll(f_[9]);
        snprintf(t->cp, sizeof t->cp, "%s", f_[10]); snprintf(t->fail, sizeof t->fail, "%s", f_[11]); snprintf(t->hash, sizeof t->hash, "%s", f_[12]);
        if (ty < 0 || ty > 3 || st < 0 || st > 10) { ok = 0; continue; }
        t->type = ty; t->st = st; t->from = f; t->to = to; t->from_amt = fa; t->to_amt = ta; t->fee_brl = fee; t->created = cr; M.n++;
        // transação interrompida (app caiu antes de concluir) nunca fica "viva": expira
        if (t->st == S_AWAITING_AUTH || t->st == S_QUOTED || t->st == S_CREATED) t->st = S_EXPIRED;
    }
    free(b); return ok;
}
