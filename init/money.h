// BANKPHONE OS — motor de dinheiro. Valores em menor unidade (BRL: centavos, USDC: 1e-6). Sem ponto flutuante.
// Saldo é sempre DERIVADO das transações CONFIRMED. Tudo é DEMO: nenhum dinheiro real.
#pragma once
#include <stdint.h>
#include <stddef.h>

typedef enum { A_BRL = 0, A_USDC = 1, A_N = 2, A_NONE = -1 } Asset;
typedef enum { T_FUNDING, T_PIX_IN, T_PIX_OUT, T_SWAP } TxType;
typedef enum { S_CREATED, S_QUOTED, S_AWAITING_AUTH, S_AUTHORIZED, S_SIGNED, S_BROADCASTING, S_PENDING, S_CONFIRMED, S_FAILED, S_CANCELLED, S_EXPIRED } TxState;
// Não há leitor biométrico no sistema: níveis acima de PIN usam 2ª confirmação explícita e espera.
// O tipo Level e a concessão de autorização ficam em sec.h / The Level type and the authorization grant live in sec.h.
#include "sec.h"

#define MAX_TX 400
/* Functional limits, NOT type limits. Each one is far below what int64 could hold and is chosen so that every
 * product stays inside int64 AND every value the engine writes is one its own loader accepts (see money.c).
 * Limites funcionais, NÃO limites do tipo. Cada um fica muito abaixo do que o int64 aguenta e foi escolhido para
 * que todo produto caiba no int64 E todo valor que o motor grava seja aceito pelo seu próprio carregador. */
#define MAX_AMOUNT     1000000000000000LL   /* 1e15: sanity cap for any stored amount / teto de sanidade de qualquer valor gravado */
#define MAX_SWAP_BRL   100000000000LL       /* 1e11 cents  = R$ 1.000.000.000,00 (same as the UI's typed cap) */
#define MAX_SWAP_USDC  100000000000000LL    /* 1e14 micro  = 100.000.000 USDC */
#define MAX_TIME       100000000000LL       /* 1e11 s ~ year 5138: the largest time the loader stores / o maior tempo que o carregador guarda */
typedef struct {
    char id[12], idem[48], cp[40], fail[96], hash[72];   // hash vazio = sem transação on-chain
    TxType type; TxState st; int from, to;               // from/to: Asset ou A_NONE
    int64_t from_amt, to_amt, fee_brl; int64_t created;  // created: segundos
} Tx;

typedef struct { int from, to; int64_t from_amt, to_amt, rate, fee_brl; int64_t expires; int ok; char id[24]; } Quote;

typedef struct {
    int64_t pin_max, confirm_max, per_tx, daily;   // centavos de BRL
    int64_t cooldown_s;
} Policy;

typedef struct {
    Tx tx[MAX_TX]; int n;
    Policy pol; int emergency; int pix_offline;    // pix_offline: simula provedor fora do ar (teste)
    uint32_t seq;
} Money;

extern Money M;
void    m_init(void);
int64_t m_balance(int asset);
int64_t m_total_brl(void);                          // BRL + USDC convertido pela taxa DEMO
void    m_load_demo(int64_t now);                   // idempotente
Tx     *m_receive_pix(int64_t cents, const char *from, int64_t now);
Tx     *m_prepare_pix(const char *dest, int64_t cents, int64_t now, const char **err);
int     m_quote(int from, int to, int64_t amt, int64_t now, Quote *q);   // 1 = ok
Tx     *m_prepare_swap(const Quote *q, int64_t now, const char **err);
Level   m_required(const Tx *t);
// Autoriza e executa SÓ com uma concessão viva, emitida por sec.c quando o PIN foi conferido (auth_peek).
// Não recebe nível do chamador. Devolve NULL se OK, ou texto do motivo (o estado da tx indica o resultado).
// Authorises and executes ONLY with a live grant issued by sec.c when the PIN was verified (auth_peek).
// It takes no level from the caller. Returns NULL if OK, or the reason text (the tx state tells the result).
const char *m_authorize(Tx *t, int64_t now);
void    m_cancel(Tx *t, int64_t now);
int64_t m_pix_spent_since(int64_t since);
const char *m_state_name(TxState s);

// formatação (sem float)
void    fmt_money(char *out, size_t n, int asset, int64_t v);   // "R$ 1.234,56" / "$ 177.94"
int64_t parse_cents(const char *digits);                         // só dígitos -> centavos de BRL (valor = digits como centavos)

// serialização para persistência
size_t  m_serialize(char *buf, size_t cap);
int     m_deserialize(const char *buf);       // 1 ok · 0 inválido (livro-razão fica vazio; motivo em m_deserialize_error)
const char *m_deserialize_error(void);
