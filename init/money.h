// BANKPHONE OS — motor de dinheiro. Valores em menor unidade (BRL: centavos, USDC: 1e-6). Sem ponto flutuante.
// Saldo é sempre DERIVADO das transações CONFIRMED. Tudo é DEMO: nenhum dinheiro real.
#pragma once
#include <stdint.h>
#include <stddef.h>

typedef enum { A_BRL = 0, A_USDC = 1, A_N = 2, A_NONE = -1 } Asset;
typedef enum { T_FUNDING, T_PIX_IN, T_PIX_OUT, T_SWAP } TxType;
typedef enum { S_CREATED, S_QUOTED, S_AWAITING_AUTH, S_AUTHORIZED, S_SIGNED, S_BROADCASTING, S_PENDING, S_CONFIRMED, S_FAILED, S_CANCELLED, S_EXPIRED } TxState;
// Não há leitor biométrico no sistema: níveis acima de PIN usam 2ª confirmação explícita e espera.
typedef enum { L_PIN = 1, L_CONFIRM = 2, L_COOLDOWN = 3 } Level;

#define MAX_TX 400
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
// Executa após autenticação. Devolve NULL se OK, ou texto do motivo (estado da tx indica o resultado).
const char *m_authorize(Tx *t, Level achieved, int64_t now);
void    m_cancel(Tx *t, int64_t now);
int64_t m_pix_spent_since(int64_t since);
const char *m_state_name(TxState s);

// formatação (sem float)
void    fmt_money(char *out, size_t n, int asset, int64_t v);   // "R$ 1.234,56" / "$ 177.94"
int64_t parse_cents(const char *digits);                         // só dígitos -> centavos de BRL (valor = digits como centavos)

// serialização para persistência
size_t  m_serialize(char *buf, size_t cap);
int     m_deserialize(const char *buf);
