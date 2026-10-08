#include "sec.h"
#include <stdio.h>
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

void pin_derive(const char *pin, const uint8_t salt[16], uint8_t out[32]) {
    uint8_t buf[16 + 32 + 16]; size_t pl = strlen(pin); if (pl > 16) pl = 16;
    memcpy(buf, salt, 16); memcpy(buf + 16, pin, pl);
    uint8_t h[32]; sha256(buf, 16 + pl, h);
    for (int i = 0; i < 50000; i++) { memcpy(buf, h, 32); memcpy(buf + 32, salt, 16); sha256(buf, 48, h); }
    memcpy(out, h, 32);
}

void pin_set(const char *pin, const uint8_t salt[16]) { memcpy(PIN.salt, salt, 16); pin_derive(pin, salt, PIN.hash); PIN.set = 1; PIN.fails = 0; PIN.locked_until = 0; }

int pin_check(const char *pin, int64_t now) {
    if (now < PIN.locked_until) return 2;
    uint8_t h[32]; pin_derive(pin, PIN.salt, h);
    uint8_t diff = 0; for (int i = 0; i < 32; i++) diff |= h[i] ^ PIN.hash[i];
    if (!diff) { PIN.fails = 0; return 0; }
    if (++PIN.fails >= 5) { int e = PIN.fails - 5; if (e > 6) e = 6; PIN.locked_until = now + 30 * (1 << e); }
    return 1;
}
int64_t pin_locked_s(int64_t now) { return now < PIN.locked_until ? PIN.locked_until - now : 0; }

static void hex(char *o, const uint8_t *b, int n) { for (int i = 0; i < n; i++) sprintf(o + 2 * i, "%02x", b[i]); }
static int unhex(uint8_t *o, const char *s, int n) { for (int i = 0; i < n; i++) { unsigned v; if (sscanf(s + 2 * i, "%2x", &v) != 1) return 0; o[i] = (uint8_t)v; } return 1; }
size_t pin_serialize(char *buf, size_t cap) {
    char s[33], h[65]; hex(s, PIN.salt, 16); hex(h, PIN.hash, 32);
    return (size_t)snprintf(buf, cap, "P %d %d %lld %s %s\n", PIN.set, PIN.fails, (long long)PIN.locked_until, s, h);
}
int pin_deserialize(const char *buf) {
    int set, fails; long long lu; char s[40], h[80];
    if (sscanf(buf, "P %d %d %lld %32s %64s", &set, &fails, &lu, s, h) != 5) return 0;
    if (!unhex(PIN.salt, s, 16) || !unhex(PIN.hash, h, 32)) return 0;
    PIN.set = set; PIN.fails = fails; PIN.locked_until = lu; return 1;
}
