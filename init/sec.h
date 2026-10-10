// BANKPHONE OS — security: PIN with a memory-hard KDF (scrypt), plus the primitives it needs.
// BANKPHONE OS — segurança: PIN com KDF que gasta memória (scrypt) e as primitivas que ele usa.
// The PIN itself is never stored. / O PIN nunca é gravado.
//
// Verifier versions / Versões do verificador:
//   1 = legacy: salted SHA-256 iterated 50,000 times, PIN cut at 16 bytes (state line "P ...").
//       Still accepted so old state keeps working; upgraded to 2 at the next correct unlock.
//       Legado: ainda aceito para o estado antigo continuar valendo; vira 2 no próximo desbloqueio certo.
//   2 = scrypt(PIN, salt, N=2^log2n, r, p) -> HKDF -> two independent keys (state line "P2 ...").
//       Only the verifier is stored; the second key (state encryption) never is.
//       Só o verificador é gravado; a segunda chave (cifra do estado) nunca é.
#pragma once
#include <stdint.h>
#include <stddef.h>

#define PIN_MAXLEN       64                /* bytes of PIN/passphrase; longer is refused, never truncated */
#define SCRYPT_MAX_MEM   (128u << 20)      /* 128 MiB hard cap on the memory a stored/parsed cost may ask for */
#define PIN_DEFAULT_LOG2N 15               /* N = 32768, r = 8 -> 32 MiB; measure on the phone before trusting */
#define PIN_DEFAULT_R     8
#define PIN_DEFAULT_P     1

typedef struct {
    uint8_t salt[16], hash[32]; int set, fails; int64_t locked_until;
    int ver;                               /* 0 = no PIN, 1 = legacy, 2 = scrypt */
    uint32_t log2n, r, p;                  /* scrypt cost of the stored verifier (ver 2) */
} Pin;
extern Pin PIN;

/* ---- hashing and key derivation primitives ---- */
typedef struct { uint32_t h[8]; uint8_t buf[64]; size_t n; uint64_t total; } Sha256;
void sha256(const uint8_t *d, size_t n, uint8_t out[32]);
void sha256_init(Sha256 *c);
void sha256_update(Sha256 *c, const void *data, size_t len);
void sha256_final(Sha256 *c, uint8_t out[32]);         /* also wipes the context */
void hmac_sha256(const uint8_t *key, size_t kl, const uint8_t *msg, size_t ml, uint8_t out[32]);
void pbkdf2_sha256(const uint8_t *pw, size_t pl, const uint8_t *salt, size_t sl, uint32_t iters, uint8_t *out, size_t outl);
int  hkdf_sha256(const uint8_t *ikm, size_t il, const uint8_t *salt, size_t sl, const uint8_t *info, size_t infol, uint8_t *out, size_t outl);
int  scrypt_params_ok(uint32_t log2n, uint32_t r, uint32_t p);
int  scrypt_kdf(const uint8_t *pw, size_t pl, const uint8_t *salt, size_t sl, uint32_t log2n, uint32_t r, uint32_t p, uint8_t *out, size_t outl);
void sec_wipe(void *p, size_t n);                      /* zero memory, not optimised away */
int  sec_ct_eq(const uint8_t *a, const uint8_t *b, size_t n);   /* constant time, 1 = equal */

/* ---- PIN ---- */
void pin_derive(const char *pin, const uint8_t salt[16], uint8_t out[32]);   /* LEGACY (ver 1) */
/* Derive both keys from the PIN: verifier (stored) and state key (kept in memory only while unlocked).
 * Deriva as duas chaves do PIN: verificador (gravado) e chave do estado (só na memória, enquanto desbloqueado). */
int  pin_derive_v2(const char *pin, const uint8_t salt[16], uint32_t log2n, uint32_t r, uint32_t p, uint8_t verifier[32], uint8_t statekey[32]);
void pin_kdf_cost(uint32_t log2n, uint32_t r, uint32_t p);   /* cost for NEW verifiers (tests use a small one) */
int  pin_set(const char *pin, const uint8_t salt[16]);       // salt from the kernel (urandom). 0 ok, -1 invalid PIN or KDF failed
int  pin_check(const char *pin, int64_t now);                // 0 ok, 1 wrong, 2 locked, 3 KDF failed (not counted as a wrong try)
int64_t pin_locked_s(int64_t now);
const uint8_t *pin_state_key(void);                          // 32 bytes while unlocked with a ver-2 verifier, else NULL
void pin_forget_key(void);                                   // wipe the state key (call when the screen locks)
size_t pin_serialize(char *buf, size_t cap);
int  pin_deserialize(const char *buf);
