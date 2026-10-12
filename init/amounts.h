/* BANKPHONE OS: amount conversions for the screens, with no overflow. Pure, so it can be tested on a computer.
 * BANKPHONE OS: conversões de valor para as telas, sem estouro. Puras, para poder testar no computador.
 *
 * The Money tab shows the total as an approximate USDC value using the DEMO rate (1 USDC = R$ 5,60, that is 560 cents). It computed
 * brl_cents * 1000000 / 560, and the product left int64 above R$ 92 billion (9223372036854 cents): signed overflow, undefined behaviour,
 * and a negative or wrong number on the screen. Here the cents are split into whole units of the rate and a remainder, so no product
 * ever exceeds 560 * 1e6 or (cents / 560) * 1e6; a value whose result does not fit int64 is refused, never wrapped.
 * A aba Dinheiro mostra o total como valor aproximado em USDC pela taxa DEMO (1 USDC = R$ 5,60, isto é, 560 centavos). Ela calculava
 * brl_cents * 1000000 / 560, e o produto saía do int64 acima de R$ 92 bilhões (9223372036854 centavos): overflow com sinal, comportamento
 * indefinido, e um número negativo ou errado na tela. Aqui os centavos são separados em unidades inteiras da taxa e um resto, então nenhum
 * produto passa de 560 * 1e6 ou de (centavos / 560) * 1e6; um valor cujo resultado não cabe no int64 é recusado, nunca dado a volta. */
#pragma once
#include <stdint.h>

/* 1 on success (and *out set), 0 if brl_cents is negative or the result does not fit int64. Same result as brl_cents * 1000000 / 560
 * wherever that product is safe (both truncate toward zero).
 * 1 se deu certo (e *out preenchido), 0 se brl_cents é negativo ou o resultado não cabe no int64. Mesmo resultado de brl_cents * 1000000 / 560
 * onde esse produto é seguro (os dois truncam para zero). */
static inline int usdc_micro_from_brl(int64_t brl_cents, int64_t *out)
{
    const int64_t RATE_CENTS = 560, UNIT = 1000000;
    if (brl_cents < 0) return 0;
    int64_t whole = brl_cents / RATE_CENTS, rest = brl_cents % RATE_CENTS;
    if (whole > INT64_MAX / UNIT) return 0;
    int64_t base = whole * UNIT, frac = rest * UNIT / RATE_CENTS;
    if (base > INT64_MAX - frac) return 0;
    *out = base + frac;
    return 1;
}
