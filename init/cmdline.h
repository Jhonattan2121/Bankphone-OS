/* BANKPHONE OS: leitura da linha de comando do kernel (cmdline). Sem I/O, para poder ser testado no computador.
 * BANKPHONE OS: kernel command line parser. No I/O, so it can be tested on a computer.
 *
 * Regras / Rules:
 *   1. A cmdline é uma lista de TOKENS separados por espaço, tab ou quebra de linha.
 *      A key=value is matched only as a WHOLE token: "bankphone.ro" never matches "xbankphone.ro=1" or
 *      "androidboot.bankphone.ro=1", and "bankphone.state" never matches "bankphone.statehash=...".
 *   2. A ordem dos tokens não importa. / The order of the tokens does not matter.
 *   3. Chave repetida: vale a ÚLTIMA ocorrência. / Repeated key: the LAST one wins.
 *   4. "chave" sem "=" não é um valor: não casa. "chave=" casa, com valor vazio.
 *      "key" without "=" is not a value: no match. "key=" matches with an empty value.
 *   5. Aspas não são interpretadas: o valor vai até o próximo espaço. / Quotes are not interpreted.
 *
 * Esta lógica protege a trava `bankphone.ro=1` (nenhuma gravação). Ela NÃO pode falhar para o lado errado.
 * This logic guards the `bankphone.ro=1` switch (no writes at all). It must not fail the wrong way.
 */
#pragma once
#include <stddef.h>
#include <string.h>

#define CMDLINE_VALUE_MAX 64     /* bytes por valor, contando o NUL / bytes per value, including the NUL */
#define CMDLINE_PROP_SLOTS 8     /* resultados de cmdline_prop() que continuam válidos ao mesmo tempo */

static inline int cmdline_is_space(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

/* Copia o valor de `key` para `out` (sempre com NUL, se n > 0).
 * Devolve 1 = achou; 0 = não achou (ou argumentos inválidos); -1 = achou, mas o valor foi cortado.
 * Copies the value of `key` into `out` (always NUL-terminated if n > 0).
 * Returns 1 = found; 0 = not found (or bad arguments); -1 = found, but the value was truncated. */
static inline int cmdline_get(const char *cl, const char *key, char *out, size_t n)
{
    int res = 0;
    if (out && n) out[0] = 0;
    if (!cl || !key || !out || !n) return 0;
    size_t kl = strlen(key);
    if (!kl) return 0;
    for (size_t i = 0; i < kl; i++) if (key[i] == '=' || cmdline_is_space(key[i])) return 0;

    const char *p = cl;
    while (*p) {
        while (*p && cmdline_is_space(*p)) p++;
        const char *t = p;
        while (*p && !cmdline_is_space(*p)) p++;
        size_t tl = (size_t)(p - t);
        if (tl > kl && !memcmp(t, key, kl) && t[kl] == '=') {      /* token inteiro: chave exata + '=' */
            size_t vl = tl - kl - 1, c = vl < n - 1 ? vl : n - 1;
            memcpy(out, t + kl + 1, c);
            out[c] = 0;
            res = vl > n - 1 ? -1 : 1;                              /* sem `break`: a última ocorrência vale */
        }
    }
    return res;
}

/* Versão que devolve ponteiro, "" se ausente. Usa CMDLINE_PROP_SLOTS buffers girando: o resultado de uma
 * chamada continua válido durante as próximas CMDLINE_PROP_SLOTS - 1 chamadas. (Antes havia UM buffer só:
 * ler três valores em seguida deixava os três ponteiros apontando para o mesmo texto.)
 * Pointer-returning version, "" if absent. It rotates through CMDLINE_PROP_SLOTS buffers: a result stays
 * valid for the next CMDLINE_PROP_SLOTS - 1 calls. (There used to be ONE buffer: reading three values in
 * a row left all three pointers aiming at the same text.) */
static inline const char *cmdline_prop(const char *cl, const char *key)
{
    static char buf[CMDLINE_PROP_SLOTS][CMDLINE_VALUE_MAX];
    static unsigned next;
    char *v = buf[next++ % CMDLINE_PROP_SLOTS];
    cmdline_get(cl, key, v, CMDLINE_VALUE_MAX);
    return v;
}
