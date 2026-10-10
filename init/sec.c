#include "sec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Pin PIN;
static const uint32_t K[64] = {
 0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
 0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
 0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
 0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void block(uint32_t h[8], const uint8_t *p) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++) w[i] = (uint32_t)p[4*i] << 24 | p[4*i+1] << 16 | p[4*i+2] << 8 | p[4*i+3];
    for (int i = 16; i < 64; i++) { uint32_t s0 = ROR(w[i-15],7) ^ ROR(w[i-15],18) ^ (w[i-15] >> 3), s1 = ROR(w[i-2],17) ^ ROR(w[i-2],19) ^ (w[i-2] >> 10); w[i] = w[i-16] + s0 + w[i-7] + s1; }
    uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t t1 = hh + (ROR(e,6)^ROR(e,11)^ROR(e,25)) + ((e&f)^(~e&g)) + K[i] + w[i], t2 = (ROR(a,2)^ROR(a,13)^ROR(a,22)) + ((a&b)^(a&c)^(b&c));
        hh=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
    }
    h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d; h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=hh;
}

void sha256(const uint8_t *d, size_t n, uint8_t out[32]) {
    uint32_t h[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    size_t i = 0; for (; i + 64 <= n; i += 64) block(h, d + i);
    uint8_t t[128] = {0}; size_t r = n - i; memcpy(t, d + i, r); t[r] = 0x80;
    size_t L = r < 56 ? 64 : 128; uint64_t bits = (uint64_t)n * 8;
    for (int k = 0; k < 8; k++) t[L - 1 - k] = (uint8_t)(bits >> (8 * k));
    block(h, t); if (L == 128) block(h, t + 64);
    for (int k = 0; k < 8; k++) { out[4*k] = h[k] >> 24; out[4*k+1] = h[k] >> 16; out[4*k+2] = h[k] >> 8; out[4*k+3] = h[k]; }
}


/* ---------------------------------------------------------------------------
 * Primitives for the memory-hard PIN KDF (scrypt, RFC 7914).
 * Primitivas do KDF do PIN que gasta memória (scrypt, RFC 7914).
 * ------------------------------------------------------------------------- */

/* Zero memory in a way the compiler may not remove.
 * Apaga memória de um jeito que o compilador não pode eliminar. */
void sec_wipe(void *p, size_t n) { volatile uint8_t *q = (volatile uint8_t *)p; while (n--) *q++ = 0; }

/* Constant-time equality: 1 if equal, 0 otherwise, no early exit.
 * Igualdade em tempo constante: 1 se igual, 0 se diferente, sem sair cedo. */
int sec_ct_eq(const uint8_t *a, const uint8_t *b, size_t n) {
    uint8_t d = 0; for (size_t i = 0; i < n; i++) d |= a[i] ^ b[i];
    return d == 0;
}

void sha256_init(Sha256 *c) {
    static const uint32_t iv[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    memcpy(c->h, iv, sizeof iv); c->n = 0; c->total = 0;
}
void sha256_update(Sha256 *c, const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data; c->total += len;
    if (!len) return;                                   /* data may be NULL when len is 0: memcpy(NULL, 0) is undefined / data pode ser NULL com len 0 */
    if (c->n) {
        size_t k = 64 - c->n; if (k > len) k = len;
        memcpy(c->buf + c->n, p, k); c->n += k; p += k; len -= k;
        if (c->n < 64) return;
        block(c->h, c->buf); c->n = 0;
    }
    for (; len >= 64; p += 64, len -= 64) block(c->h, p);
    if (len) { memcpy(c->buf, p, len); c->n = len; }
}
void sha256_final(Sha256 *c, uint8_t out[32]) {
    uint64_t bits = c->total * 8; uint8_t pad[72] = {0x80}; size_t padlen = (c->n < 56) ? 56 - c->n : 120 - c->n;
    for (unsigned k = 0; k < 8; k++) pad[padlen + k] = (uint8_t)(bits >> (56 - 8 * k));
    sha256_update(c, pad, padlen + 8);
    for (int k = 0; k < 8; k++) { out[4*k] = (uint8_t)(c->h[k] >> 24); out[4*k+1] = (uint8_t)(c->h[k] >> 16); out[4*k+2] = (uint8_t)(c->h[k] >> 8); out[4*k+3] = (uint8_t)c->h[k]; }
    sec_wipe(c, sizeof *c);
}

/* HMAC-SHA256 with the key pads absorbed once, so PBKDF2 does not redo them per iteration.
 * HMAC-SHA256 com os pads da chave absorvidos uma vez, para o PBKDF2 não refazer a cada iteração. */
typedef struct { Sha256 in, out; } HmacKey;
static void hmac_key(HmacKey *k, const uint8_t *key, size_t kl) {
    uint8_t kb[64] = {0}, pad[64];
    if (kl > 64) { sha256(key, kl, kb); } else if (kl) memcpy(kb, key, kl);
    for (int i = 0; i < 64; i++) pad[i] = kb[i] ^ 0x36;
    sha256_init(&k->in); sha256_update(&k->in, pad, 64);
    for (int i = 0; i < 64; i++) pad[i] = kb[i] ^ 0x5c;
    sha256_init(&k->out); sha256_update(&k->out, pad, 64);
    sec_wipe(kb, sizeof kb); sec_wipe(pad, sizeof pad);
}
static void hmac_begin(const HmacKey *k, Sha256 *c) { *c = k->in; }
static void hmac_end(const HmacKey *k, Sha256 *c, uint8_t out[32]) {
    uint8_t ih[32]; sha256_final(c, ih);
    Sha256 o = k->out; sha256_update(&o, ih, 32); sha256_final(&o, out); sec_wipe(ih, sizeof ih);
}
void hmac_sha256(const uint8_t *key, size_t kl, const uint8_t *msg, size_t ml, uint8_t out[32]) {
    HmacKey k; Sha256 c; hmac_key(&k, key, kl); hmac_begin(&k, &c); sha256_update(&c, msg, ml); hmac_end(&k, &c, out);
    sec_wipe(&k, sizeof k);
}

void pbkdf2_sha256(const uint8_t *pw, size_t pl, const uint8_t *salt, size_t sl, uint32_t iters, uint8_t *out, size_t outl) {
    HmacKey k; hmac_key(&k, pw, pl);
    for (uint32_t blk = 1; outl; blk++) {
        uint8_t be[4] = {(uint8_t)(blk >> 24), (uint8_t)(blk >> 16), (uint8_t)(blk >> 8), (uint8_t)blk}, u[32], t[32];
        Sha256 c; hmac_begin(&k, &c); sha256_update(&c, salt, sl); sha256_update(&c, be, 4); hmac_end(&k, &c, u);
        memcpy(t, u, 32);
        for (uint32_t j = 1; j < iters; j++) { hmac_begin(&k, &c); sha256_update(&c, u, 32); hmac_end(&k, &c, u); for (int i = 0; i < 32; i++) t[i] ^= u[i]; }
        size_t n = outl < 32 ? outl : 32; memcpy(out, t, n); out += n; outl -= n;
        sec_wipe(u, sizeof u); sec_wipe(t, sizeof t);
    }
    sec_wipe(&k, sizeof k);
}

/* HKDF-SHA256 (RFC 5869). salt may be NULL/0 (then 32 zero bytes). outl <= 255*32.
 * HKDF-SHA256 (RFC 5869). salt pode ser NULL/0 (então 32 bytes zero). outl <= 255*32. */
int hkdf_sha256(const uint8_t *ikm, size_t il, const uint8_t *salt, size_t sl, const uint8_t *info, size_t infol, uint8_t *out, size_t outl) {
    if (outl > 255 * 32) return -1;
    uint8_t zero[32] = {0}, prk[32], t[32] = {0}; size_t tl = 0;
    hmac_sha256(sl ? salt : zero, sl ? sl : 32, ikm, il, prk);
    HmacKey k; hmac_key(&k, prk, 32);
    for (uint8_t ctr = 1; outl; ctr++) {
        Sha256 c; hmac_begin(&k, &c); sha256_update(&c, t, tl); sha256_update(&c, info, infol); sha256_update(&c, &ctr, 1); hmac_end(&k, &c, t); tl = 32;
        size_t n = outl < 32 ? outl : 32; memcpy(out, t, n); out += n; outl -= n;
    }
    sec_wipe(prk, sizeof prk); sec_wipe(t, sizeof t); sec_wipe(&k, sizeof k);
    return 0;
}

/* ---- scrypt (RFC 7914): Salsa20/8, BlockMix, ROMix ---- */
#define ROL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))
static void salsa20_8(uint32_t B[16]) {
    uint32_t x[16]; memcpy(x, B, sizeof x);
    for (int i = 0; i < 8; i += 2) {
        x[ 4] ^= ROL(x[ 0]+x[12], 7); x[ 8] ^= ROL(x[ 4]+x[ 0], 9); x[12] ^= ROL(x[ 8]+x[ 4],13); x[ 0] ^= ROL(x[12]+x[ 8],18);
        x[ 9] ^= ROL(x[ 5]+x[ 1], 7); x[13] ^= ROL(x[ 9]+x[ 5], 9); x[ 1] ^= ROL(x[13]+x[ 9],13); x[ 5] ^= ROL(x[ 1]+x[13],18);
        x[14] ^= ROL(x[10]+x[ 6], 7); x[ 2] ^= ROL(x[14]+x[10], 9); x[ 6] ^= ROL(x[ 2]+x[14],13); x[10] ^= ROL(x[ 6]+x[ 2],18);
        x[ 3] ^= ROL(x[15]+x[11], 7); x[ 7] ^= ROL(x[ 3]+x[15], 9); x[11] ^= ROL(x[ 7]+x[ 3],13); x[15] ^= ROL(x[11]+x[ 7],18);
        x[ 1] ^= ROL(x[ 0]+x[ 3], 7); x[ 2] ^= ROL(x[ 1]+x[ 0], 9); x[ 3] ^= ROL(x[ 2]+x[ 1],13); x[ 0] ^= ROL(x[ 3]+x[ 2],18);
        x[ 6] ^= ROL(x[ 5]+x[ 4], 7); x[ 7] ^= ROL(x[ 6]+x[ 5], 9); x[ 4] ^= ROL(x[ 7]+x[ 6],13); x[ 5] ^= ROL(x[ 4]+x[ 7],18);
        x[11] ^= ROL(x[10]+x[ 9], 7); x[ 8] ^= ROL(x[11]+x[10], 9); x[ 9] ^= ROL(x[ 8]+x[11],13); x[10] ^= ROL(x[ 9]+x[ 8],18);
        x[12] ^= ROL(x[15]+x[14], 7); x[13] ^= ROL(x[12]+x[15], 9); x[14] ^= ROL(x[13]+x[12],13); x[15] ^= ROL(x[14]+x[13],18);
    }
    for (int i = 0; i < 16; i++) B[i] += x[i];
}
/* B: 2r blocks of 16 words. Y: scratch, same size. / B: 2r blocos de 16 palavras. Y: rascunho, mesmo tamanho. */
static void blockmix(uint32_t *B, uint32_t *Y, uint32_t r) {
    uint32_t X[16]; memcpy(X, &B[(2 * r - 1) * 16], 64);
    for (uint32_t i = 0; i < 2 * r; i++) {
        for (uint32_t k = 0; k < 16; k++) X[k] ^= B[i * 16 + k];
        salsa20_8(X); memcpy(&Y[i * 16], X, 64);
    }
    for (uint32_t i = 0; i < r; i++) { memcpy(&B[i * 16], &Y[(2 * i) * 16], 64); memcpy(&B[(r + i) * 16], &Y[(2 * i + 1) * 16], 64); }
}
static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static void put_le32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
static void romix(uint8_t *B, uint32_t r, uint32_t N, uint32_t *V, uint32_t *XY) {
    uint32_t *X = XY, *Y = XY + 32 * r, words = 32 * r;
    for (uint32_t i = 0; i < words; i++) X[i] = le32(B + 4 * i);
    for (uint32_t i = 0; i < N; i++) { memcpy(&V[(size_t)i * words], X, words * 4); blockmix(X, Y, r); }
    for (uint32_t i = 0; i < N; i++) {
        uint32_t j = X[(2 * r - 1) * 16] & (N - 1);               /* Integerify: low word is enough, N <= 2^30 */
        for (uint32_t k = 0; k < words; k++) X[k] ^= V[(size_t)j * words + k];
        blockmix(X, Y, r);
    }
    for (uint32_t i = 0; i < words; i++) put_le32(B + 4 * i, X[i]);
}

/* Reject cost parameters that are invalid or would ask for too much memory.
 * Recusa parâmetros de custo inválidos ou que pediriam memória demais. */
int scrypt_params_ok(uint32_t log2n, uint32_t r, uint32_t p) {
    if (log2n < 1 || log2n > 24 || r < 1 || r > 32 || p < 1 || p > 16) return 0;
    if ((uint64_t)r * p >= (1u << 30)) return 0;
    return ((uint64_t)128 * r << log2n) <= SCRYPT_MAX_MEM;
}

/* scrypt(pw, salt, N = 2^log2n, r, p) -> out. Returns 0, or -1 for bad parameters / no memory.
 * scrypt(pw, salt, N = 2^log2n, r, p) -> out. Devolve 0, ou -1 se parâmetros ruins / sem memória. */
int scrypt_kdf(const uint8_t *pw, size_t pl, const uint8_t *salt, size_t sl, uint32_t log2n, uint32_t r, uint32_t p, uint8_t *out, size_t outl) {
    if (!scrypt_params_ok(log2n, r, p)) return -1;
    uint32_t N = 1u << log2n; size_t blen = (size_t)128 * r * p;
    uint8_t *B = malloc(blen); uint32_t *V = malloc((size_t)128 * r * N), *XY = malloc((size_t)256 * r);
    if (!B || !V || !XY) { free(B); free(V); free(XY); return -1; }
    pbkdf2_sha256(pw, pl, salt, sl, 1, B, blen);
    for (uint32_t i = 0; i < p; i++) romix(B + (size_t)128 * r * i, r, N, V, XY);
    pbkdf2_sha256(pw, pl, B, blen, 1, out, outl);
    sec_wipe(B, blen); sec_wipe(V, (size_t)128 * r * N); sec_wipe(XY, (size_t)256 * r);
    free(B); free(V); free(XY); return 0;
}

/* LEGACY (state format "P"): salted SHA-256 iterated 50,000 times, PIN cut at 16 bytes.
 * Kept only to verify old verifiers and migrate them; new PINs use scrypt (see pin_set).
 * LEGADO (formato de estado "P"): SHA-256 com sal iterado 50.000 vezes, PIN cortado em 16 bytes.
 * Mantido só para verificar verificadores antigos e migrá-los; PINs novos usam scrypt (veja pin_set). */
void pin_derive(const char *pin, const uint8_t salt[16], uint8_t out[32]) {
    uint8_t buf[16 + 32 + 16]; size_t pl = strlen(pin); if (pl > 16) pl = 16;
    memcpy(buf, salt, 16); memcpy(buf + 16, pin, pl);
    uint8_t h[32]; sha256(buf, 16 + pl, h);
    for (int i = 0; i < 50000; i++) { memcpy(buf, h, 32); memcpy(buf + 32, salt, 16); sha256(buf, 48, h); }
    memcpy(out, h, 32);
}

/* ---- v2: scrypt + HKDF ---- */
static uint32_t g_log2n = PIN_DEFAULT_LOG2N, g_r = PIN_DEFAULT_R, g_p = PIN_DEFAULT_P;
void pin_kdf_cost(uint32_t log2n, uint32_t r, uint32_t p) { if (scrypt_params_ok(log2n, r, p)) { g_log2n = log2n; g_r = r; g_p = p; } }

static uint8_t g_statekey[32]; static int g_statekey_ok;
const uint8_t *pin_state_key(void) { return g_statekey_ok ? g_statekey : NULL; }
void pin_forget_key(void) { sec_wipe(g_statekey, sizeof g_statekey); g_statekey_ok = 0; }

int pin_derive_v2(const char *pin, const uint8_t salt[16], uint32_t log2n, uint32_t r, uint32_t p, uint8_t verifier[32], uint8_t statekey[32]) {
    static const uint8_t DOM[] = "bankphone/pin/v2", I_VER[] = "bankphone/pin-verifier/v2", I_KEY[] = "bankphone/state-key/v2";
    size_t pl = strlen(pin); if (pl == 0 || pl > PIN_MAXLEN) return -1;
    uint8_t k[32]; if (scrypt_kdf((const uint8_t *)pin, pl, salt, 16, log2n, r, p, k, 32)) return -1;
    /* Two independent outputs: knowing the verifier must not give the state key.
     * Duas saídas independentes: conhecer o verificador não pode dar a chave do estado. */
    int rc = hkdf_sha256(k, 32, DOM, sizeof DOM - 1, I_VER, sizeof I_VER - 1, verifier, 32);
    rc |= hkdf_sha256(k, 32, DOM, sizeof DOM - 1, I_KEY, sizeof I_KEY - 1, statekey, 32);
    sec_wipe(k, sizeof k); return rc ? -1 : 0;
}

int pin_set(const char *pin, const uint8_t salt[16]) {
    uint8_t v[32], sk[32];
    if (pin_derive_v2(pin, salt, g_log2n, g_r, g_p, v, sk)) return -1;
    memcpy(PIN.salt, salt, 16); memcpy(PIN.hash, v, 32); PIN.set = 1; PIN.fails = 0; PIN.locked_until = 0;
    PIN.ver = 2; PIN.log2n = g_log2n; PIN.r = g_r; PIN.p = g_p;
    memcpy(g_statekey, sk, 32); g_statekey_ok = 1;
    sec_wipe(v, sizeof v); sec_wipe(sk, sizeof sk); return 0;
}

int pin_check(const char *pin, int64_t now) {
    if (now < PIN.locked_until) return 2;
    int ok = 0;
    if (PIN.ver == 1) {                                  /* legacy verifier / verificador antigo */
        uint8_t h[32]; pin_derive(pin, PIN.salt, h);
        ok = sec_ct_eq(h, PIN.hash, 32); sec_wipe(h, sizeof h);
        if (ok) {                                        /* upgrade now, while we hold the PIN / migra agora, com o PIN em mãos */
            uint8_t v[32], sk[32];
            if (pin_derive_v2(pin, PIN.salt, g_log2n, g_r, g_p, v, sk) == 0) {
                memcpy(PIN.hash, v, 32); PIN.ver = 2; PIN.log2n = g_log2n; PIN.r = g_r; PIN.p = g_p;
                memcpy(g_statekey, sk, 32); g_statekey_ok = 1;
            }                                            /* on failure it stays legacy and is retried next time / se falhar, segue legado e tenta de novo */
            sec_wipe(v, sizeof v); sec_wipe(sk, sizeof sk);
        }
    } else {
        uint8_t v[32], sk[32]; size_t pl = strlen(pin);
        if (pl == 0 || pl > PIN_MAXLEN) ok = 0;          /* refused, counts as wrong / recusado, conta como errado */
        else if (pin_derive_v2(pin, PIN.salt, PIN.log2n, PIN.r, PIN.p, v, sk)) return 3;
        else { ok = sec_ct_eq(v, PIN.hash, 32); if (ok) { memcpy(g_statekey, sk, 32); g_statekey_ok = 1; } }
        sec_wipe(v, sizeof v); sec_wipe(sk, sizeof sk);
    }
    if (ok) { PIN.fails = 0; return 0; }
    if (++PIN.fails >= 5) { int e = PIN.fails - 5; if (e > 6) e = 6; PIN.locked_until = now + 30 * (1 << e); }
    return 1;
}
int64_t pin_locked_s(int64_t now) { return now < PIN.locked_until ? PIN.locked_until - now : 0; }

static void hex(char *o, const uint8_t *b, int n) { for (int i = 0; i < n; i++) sprintf(o + 2 * i, "%02x", b[i]); }
static int unhex(uint8_t *o, const char *s, int n) { for (int i = 0; i < n; i++) { unsigned v; if (sscanf(s + 2 * i, "%2x", &v) != 1) return 0; o[i] = (uint8_t)v; } return 1; }
size_t pin_serialize(char *buf, size_t cap) {
    char s[33], h[65]; hex(s, PIN.salt, 16); hex(h, PIN.hash, 32);
    if (PIN.ver != 2)                                    /* legacy line (and the unset PIN) stay byte-identical / linha antiga (e o PIN não definido) continuam idênticas */
        return (size_t)snprintf(buf, cap, "P %d %d %lld %s %s\n", PIN.set, PIN.fails, (long long)PIN.locked_until, s, h);
    return (size_t)snprintf(buf, cap, "P2 %d %d %lld %s %s %u %u %u\n", PIN.set, PIN.fails, (long long)PIN.locked_until, s, h,
                            (unsigned)PIN.log2n, (unsigned)PIN.r, (unsigned)PIN.p);
}
int pin_deserialize(const char *buf) {
    int set, fails; long long lu; char s[40], h[80]; unsigned ln = 0, r = 0, p = 0; int ver;
    if (!strncmp(buf, "P2 ", 3)) {
        if (sscanf(buf, "P2 %d %d %lld %32s %64s %u %u %u", &set, &fails, &lu, s, h, &ln, &r, &p) != 8) return 0;
        if (set && !scrypt_params_ok(ln, r, p)) return 0; /* never trust stored cost blindly; an unset PIN has none / nunca confie no custo gravado; PIN não definido não tem */
        ver = 2;
    } else {
        if (sscanf(buf, "P %d %d %lld %32s %64s", &set, &fails, &lu, s, h) != 5) return 0;
        ver = 1;
    }
    if (strlen(s) != 32 || strlen(h) != 64) return 0;
    if (!unhex(PIN.salt, s, 16) || !unhex(PIN.hash, h, 32)) return 0;
    PIN.set = set; PIN.fails = fails; PIN.locked_until = lu;
    PIN.ver = set ? ver : 0; PIN.log2n = set && ver == 2 ? ln : 0; PIN.r = set && ver == 2 ? r : 0; PIN.p = set && ver == 2 ? p : 0;
    pin_forget_key(); return 1;
}
