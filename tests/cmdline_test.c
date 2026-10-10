/* Testes do parser da linha de comando do kernel (init/cmdline.h). / Tests for the kernel cmdline parser.
 * Cada linha PASS/FAIL é uma verificação. / Each PASS/FAIL line is one check. */
#include "../init/cmdline.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
#define CHECK(n, c) do { int ok_ = (c); printf("%s %s\n", ok_ ? "PASS" : "FAIL", n); if (!ok_) fails++; } while (0)

static int get(const char *cl, const char *key, char *out, size_t n) { return cmdline_get(cl, key, out, n); }
static int is(const char *cl, const char *key, const char *want) {
    char b[CMDLINE_VALUE_MAX]; int r = get(cl, key, b, sizeof b);
    return want ? (r == 1 && !strcmp(b, want)) : (r == 0 && b[0] == 0);
}

/* o valor existe e e vazio / the value exists and is empty */
static int is_empty_value(const char *cl, const char *key) { char t[CMDLINE_VALUE_MAX]; int r = cmdline_get(cl, key, t, sizeof t); return r == 1 && t[0] == 0; }
/* o valor cabe exato num buffer de 6 bytes / the value fits a 6-byte buffer exactly */
static int fits_exactly(void) { char t[6]; int r = cmdline_get("a=hello", "a", t, sizeof t); return r == 1 && !strcmp(t, "hello"); }
static int zero_size_untouched(void) { char z = 'Q'; int r = cmdline_get("a=1", "a", &z, 0); return r == 0 && z == 'Q'; }
static int null_cl(void) { char t[8] = "x"; int r = cmdline_get(NULL, "a", t, sizeof t); return r == 0 && t[0] == 0; }
static int null_key(void) { char t[8] = "x"; int r = cmdline_get("a=1", NULL, t, sizeof t); return r == 0 && t[0] == 0; }
static int prop_absent(void) { const char *v = cmdline_prop("a=1", "bankphone.ro"); return v && v[0] == 0; }

int main(void) {
    char b[CMDLINE_VALUE_MAX];

    /* ---- o básico / the basics ---- */
    CHECK("valor simples", is("console=ttyS0 bankphone.ro=1", "bankphone.ro", "1"));
    CHECK("chave ausente devolve 0 e vazio", is("console=ttyS0 quiet", "bankphone.ro", NULL));
    CHECK("cmdline vazia", is("", "bankphone.ro", NULL));
    CHECK("valor termina no espaco", is("a=1 bankphone.state=expdb b=2", "bankphone.state", "expdb"));
    CHECK("separador tab", is("a=1\tbankphone.ro=1\tb=2", "bankphone.ro", "1"));
    CHECK("separador quebra de linha (como /proc/cmdline)", is("a=1 bankphone.ro=1\n", "bankphone.ro", "1"));
    CHECK("varios espacos e espacos nas pontas", is("   a=1    bankphone.ro=1   ", "bankphone.ro", "1"));
    CHECK("valor com '=' dentro", is("root=UUID=abc=def bankphone.ro=1", "root", "UUID=abc=def"));

    /* ---- os casos REPRODUZIDOS na auditoria (F-02) / the cases reproduced in the audit ---- */
    CHECK("state antes de statehash", is("console=ttyS0 bankphone.state=expdb bankphone.statehash=0123456789abcdef", "bankphone.state", "expdb"));
    CHECK("statehash ANTES de state: antes devolvia vazio", is("console=ttyS0 bankphone.statehash=0123456789abcdef bankphone.state=expdb", "bankphone.state", "expdb"));
    CHECK("statehash lido nas duas ordens", is("bankphone.state=expdb bankphone.statehash=0123456789abcdef", "bankphone.statehash", "0123456789abcdef")
                                           && is("bankphone.statehash=0123456789abcdef bankphone.state=expdb", "bankphone.statehash", "0123456789abcdef"));
    CHECK("'xbankphone.ro=1' NAO casa com bankphone.ro", is("console=ttyS0 xbankphone.ro=1", "bankphone.ro", NULL));
    CHECK("'androidboot.bankphone.ro=1' NAO casa com bankphone.ro", is("console=ttyS0 androidboot.bankphone.ro=1", "bankphone.ro", NULL));
    CHECK("'bankphone.rox=1' NAO casa com bankphone.ro", is("bankphone.rox=1", "bankphone.ro", NULL));
    CHECK("'bankphone.ro=1' DEPOIS de um parecido e reconhecido", is("bankphone.rox=0 bankphone.ro=1", "bankphone.ro", "1"));
    CHECK("a chave e prefixo de outra: nao casa com a mais longa", is("bankphone.state=expdb", "bankphone.statehash", NULL));

    /* ---- sem '=' / vazio ---- */
    CHECK("'bankphone.ro' sem '=' nao casa", is("console=ttyS0 bankphone.ro quiet", "bankphone.ro", NULL));
    CHECK("'bankphone.ro=' casa com valor vazio", is_empty_value("bankphone.ro= quiet", "bankphone.ro"));

    /* ---- repetida: a ultima vale ---- */
    CHECK("repetida: a ultima ocorrencia vale", is("bankphone.ro=0 x=1 bankphone.ro=1", "bankphone.ro", "1"));
    CHECK("repetida: ultima vazia vence a anterior", is_empty_value("bankphone.ro=1 bankphone.ro=", "bankphone.ro"));

    /* ---- limites do buffer / buffer limits ---- */
    { char v[CMDLINE_VALUE_MAX + 40]; char cl[300]; memset(v, 'z', sizeof v - 1); v[sizeof v - 1] = 0;
      snprintf(cl, sizeof cl, "bankphone.state=%s", v);
      int r = get(cl, "bankphone.state", b, sizeof b);
      CHECK("valor maior que o buffer: devolve -1 e termina com NUL", r == -1 && strlen(b) == sizeof b - 1 && b[sizeof b - 1] == 0); }
    { char one[1]; int r = get("a=hello", "a", one, 1); CHECK("buffer de 1 byte: so o NUL, devolve -1", r == -1 && one[0] == 0); }
    { char two[2]; int r = get("a=hello", "a", two, 2); CHECK("buffer de 2 bytes: 1 caractere + NUL", r == -1 && two[0] == 'h' && two[1] == 0); }
    CHECK("valor com tamanho exato do buffer util nao e cortado", fits_exactly());
    CHECK("n = 0 nao escreve e devolve 0", zero_size_untouched());

    /* ---- argumentos invalidos / bad arguments ---- */
    CHECK("cl NULL", null_cl());
    CHECK("key NULL", null_key());
    CHECK("out NULL", cmdline_get("a=1", "a", NULL, 8) == 0);
    CHECK("chave vazia", is("a=1 =2", "", NULL));
    CHECK("chave com '='", is("a=1", "a=1", NULL));
    CHECK("chave com espaco", is("a b=1", "a b", NULL));

    /* ---- cmdline longa (4096) com a chave no fim / long cmdline with the key at the end ---- */
    { static char big[4096]; size_t o = 0;
      while (o < sizeof big - 40) o += (size_t)snprintf(big + o, sizeof big - o, "androidboot.item%zu=value%zu ", o, o);
      snprintf(big + o, sizeof big - o, "bankphone.ro=1");
      CHECK("cmdline de ~4 KiB: acha a chave no fim", is(big, "bankphone.ro", "1")); }

    /* ---- o defeito do buffer compartilhado (descoberto na revisao): cmdline_prop ---- */
    { const char *cl = "console=ttyS0 bankphone.ro=0 bankphone.state=expdb bankphone.statehash=0123456789abcdef";
      const char *ro = cmdline_prop(cl, "bankphone.ro");
      const char *spec = cmdline_prop(cl, "bankphone.state");
      const char *hash = cmdline_prop(cl, "bankphone.statehash");
      CHECK("tres leituras seguidas NAO apontam para o mesmo buffer", ro != spec && spec != hash && ro != hash);
      CHECK("cada ponteiro guarda o SEU valor (ro, state, statehash)", !strcmp(ro, "0") && !strcmp(spec, "expdb") && !strcmp(hash, "0123456789abcdef")); }
    { const char *cl = "bankphone.ro=1 bankphone.state=expdb";
      const char *a[CMDLINE_PROP_SLOTS - 1]; for (int i = 0; i < CMDLINE_PROP_SLOTS - 1; i++) a[i] = cmdline_prop(cl, i % 2 ? "bankphone.state" : "bankphone.ro");
      int okk = 1; for (int i = 0; i < CMDLINE_PROP_SLOTS - 1; i++) okk &= !strcmp(a[i], i % 2 ? "expdb" : "1");
      CHECK("os ultimos SLOTS-1 resultados continuam validos ao mesmo tempo", okk); }
    CHECK("cmdline_prop de chave ausente devolve string vazia (nunca NULL)", prop_absent());

    puts(fails ? "FALHAS" : "TODOS OK");
    return fails != 0;
}
