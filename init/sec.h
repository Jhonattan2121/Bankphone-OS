// BANKPHONE OS — segurança: PIN com sal e hash iterado (SHA-256). O PIN nunca é gravado.
#pragma once
#include <stdint.h>
#include <stddef.h>

typedef struct { uint8_t salt[16], hash[32]; int set; int fails; int64_t locked_until; } Pin;
extern Pin PIN;

void sha256(const uint8_t *d, size_t n, uint8_t out[32]);
void pin_derive(const char *pin, const uint8_t salt[16], uint8_t out[32]);
void pin_set(const char *pin, const uint8_t salt[16]);                 // salt vindo do kernel (urandom)
int  pin_check(const char *pin, int64_t now);                          // 0 ok, 1 errado, 2 bloqueado
int64_t pin_locked_s(int64_t now);

/* Bounds of the lockout. The lock deadline is saved as an absolute time, but plat_now() switches between the wall clock (about 1.7e9) and
 * the uptime (small): a deadline written under one clock and read under the other used to last decades (55.8 years, reproduced on 32932dd).
 * The longest lock the code ever sets is 30 * 2^6 = 1920 s, so a saved deadline further away than that cannot be real and is clamped.
 * Limites do bloqueio. O prazo do bloqueio é gravado como hora absoluta, mas o plat_now() alterna entre o relógio real (cerca de 1,7e9) e o
 * uptime (pequeno): um prazo gravado num relógio e lido no outro durava décadas (55,8 anos, reproduzido no 32932dd). O maior bloqueio que o código
 * define é 30 * 2^6 = 1920 s, então um prazo salvo mais longe que isso não pode ser real e é limitado. */
#define PIN_LOCK_MAX_S   1920
#define PIN_MAX_FAILS    1000                   /* the saved counter is saturated here, never allowed to overflow / o contador salvo satura aqui */
#define SEC_NOW_MAX      100000000000LL         /* a clock outside [0, this] is refused: now +/- a constant would leave int64 / relógio fora de [0, isto] é recusado */

/* ---- authorization grant / concessão de autorização ----
 * A payment is authorised only with a grant that THIS module issues when a PIN is verified. The money engine
 * asks for it; a caller can no longer say "level 3 reached". One grant at a time, tied to one transaction,
 * single use, and it expires. Limit (said plainly): this is one process, so code that is already inside it can
 * still call anything; the grant removes the "pass the level you like" hole, it is not a sandbox.
 * Um pagamento só é autorizado com uma concessão que ESTE módulo emite quando um PIN é conferido. O motor
 * pede essa concessão; o chamador não pode mais dizer "nível 3 alcançado". Uma concessão por vez, ligada a
 * uma transação, de uso único, e expira. Limite (dito com clareza): é um processo só, então código que já está
 * dentro ainda pode chamar qualquer coisa; a concessão fecha o buraco de "passe o nível que quiser", não é
 * uma sandbox. */
typedef enum { L_NONE = 0, L_PIN = 1, L_CONFIRM = 2, L_COOLDOWN = 3 } Level;
#define AUTH_TTL_S 120                          /* grant lifetime in seconds / vida da concessão em segundos */
void  auth_expect(const char *tx_id);           /* UI: the next PIN that is verified is FOR this transaction / o próximo PIN conferido é PARA esta transação */
void  auth_clear(void);                         /* drop the expectation and any grant (lock, cancel) / descarta expectativa e concessão (bloqueio, cancelar) */
int   auth_confirm(const char *tx_id, int64_t now);   /* the person tapped "confirm": raise a live grant. 1 raised, 0 no live grant / a pessoa confirmou: sobe uma concessão viva */
Level auth_peek(const char *tx_id, int64_t now);      /* level of the live grant for this tx, L_NONE if none / expired / other tx */
void  auth_consume(const char *tx_id);                /* single use: called by the engine when the tx is decided / uso único */
#ifdef BANKPHONE_AUTH_TEST
void  auth_test_mint(const char *tx_id, Level lvl, int64_t now);   /* TEST BUILDS ONLY (fuzzer): never defined by build.sh / SÓ em builds de teste (fuzzer): nunca definido pelo build.sh */
#endif
size_t pin_serialize(char *buf, size_t cap);
int  pin_deserialize(const char *buf);
