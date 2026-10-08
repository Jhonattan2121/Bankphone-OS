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
size_t pin_serialize(char *buf, size_t cap);
int  pin_deserialize(const char *buf);
