# Contributing / Contribuindo

English first, português abaixo.

---

## English

BANKPHONE OS is a phone operating system written from scratch in C. **Everything is DEMO / TESTNET.** There is no real money, no real Pix and no real BRL.

### Run the checks (no phone needed)

You need a C compiler and `make`. Nothing else.

```bash
make test        # builds and runs every host test, prints a summary
make test-asan   # same, with AddressSanitizer and UndefinedBehaviorSanitizer
make lint        # -Wall -Wextra -Werror, syntax check only
make fuzz        # fuzzes the USB command parser and the money engine (FUZZ_TIME=30 seconds each)
make clean
```

On macOS the `touchcore` and `bootdiag_fb` tests (and the lint of `bootdiag.c`, `main.c`, `hw.c`) are skipped, because that code includes Linux-only headers; the Linux CI job runs them.

`make test` prints one line per test program and a total. It exits with an error if anything fails, which is what CI uses.

Run `make test-asan` before sending a change that touches `init/money.c`, `init/sec.c`, `init/store.c` or `init/devcmd.h`. A few bugs only show up there.

### Fuzzing

`make fuzz` uses a small built-in driver (`tests/fuzz/driver.c`) that works with any C compiler: it runs the corpus in `tests/fuzz/corpus/`, then mutates inputs for `FUZZ_TIME` seconds. The seed is fixed, so a failure repeats; set `FUZZ_SEED=n` to explore other paths. A crashing input is saved as `crash-<hash>` in the current directory. With clang you can use libFuzzer instead: `make fuzz FUZZ_ENGINE=libfuzzer`.

When the fuzzer finds a bug: fix it, add the crashing input to `tests/fuzz/corpus/<target>/`, and add a regression test next to the existing ones.

### Rules for the repository

1. **Nothing from a phone goes into this repository**: no kernel, no boot or vendor images, no touch firmware, no dumps. The `.gitignore` blocks the usual paths; do not work around it.
2. No credentials, keys, tokens or personal data, ever.
3. Flash only the inactive A/B slot, never an image that has not run, and keep test images read-only (`bankphone.ro=1`). See the README.
4. Amounts are integer cents. No floating point for money.
5. Be honest in the UI and in the docs: if something is not real or not implemented, say so.
6. Keep changes small and focused, and explain the reason in the commit message.

### Tests for new code

Put host tests in `tests/` (or `init/tests/` for the storage and display code) and add them to the `TESTS` list in the `Makefile`. Tests must run on a computer, without hardware.

---

## Português

BANKPHONE OS é um sistema operacional de celular escrito do zero em C. **Tudo é DEMO / TESTNET.** Não existe dinheiro real, Pix real nem BRL real.

### Rodar as checagens (sem o celular)

Você só precisa de um compilador C e do `make`.

```bash
make test        # compila e roda todos os testes no computador e mostra um resumo
make test-asan   # o mesmo, com AddressSanitizer e UndefinedBehaviorSanitizer
make lint        # -Wall -Wextra -Werror, só checa a sintaxe
make fuzz        # fuzzing do parser de comando USB e do motor de dinheiro (FUZZ_TIME=30 segundos cada)
make clean
```

No macOS os testes `touchcore` e `bootdiag_fb` (e o lint de `bootdiag.c`, `main.c`, `hw.c`) são pulados, porque esse código inclui headers só do Linux; o job de CI do Linux roda todos.

O `make test` mostra uma linha por programa de teste e o total. Ele termina com erro se algo falhar, que é o que o CI usa.

Rode o `make test-asan` antes de enviar uma mudança em `init/money.c`, `init/sec.c`, `init/store.c` ou `init/devcmd.h`. Alguns bugs só aparecem ali.

### Fuzzing

O `make fuzz` usa um driver pequeno e próprio (`tests/fuzz/driver.c`) que funciona com qualquer compilador C: ele roda o corpus de `tests/fuzz/corpus/` e depois muta as entradas por `FUZZ_TIME` segundos. A semente é fixa, então uma falha se repete; use `FUZZ_SEED=n` para explorar outros caminhos. Uma entrada que derruba o programa é salva como `crash-<hash>` na pasta atual. Com clang dá para usar o libFuzzer: `make fuzz FUZZ_ENGINE=libfuzzer`.

Quando o fuzzer achar um bug: corrija, coloque a entrada que quebrou em `tests/fuzz/corpus/<alvo>/` e acrescente um teste de regressão junto dos existentes.

### Regras do repositório

1. **Nada de celular entra neste repositório**: nada de kernel, imagens de boot ou vendor, firmware do toque, dumps. O `.gitignore` bloqueia os caminhos comuns; não contorne.
2. Nunca coloque credenciais, chaves, tokens ou dados pessoais.
3. Grave só no slot A/B inativo, nunca uma imagem que ainda não rodou, e mantenha as imagens de teste somente leitura (`bankphone.ro=1`). Veja o README.
4. Valores são centavos inteiros. Nada de ponto flutuante para dinheiro.
5. Seja honesto na interface e na documentação: se algo não é real ou não está implementado, diga.
6. Mantenha as mudanças pequenas e focadas, e explique o motivo na mensagem do commit.

### Testes para código novo

Coloque testes de computador em `tests/` (ou `init/tests/` para armazenamento e tela) e acrescente-os à lista `TESTS` do `Makefile`. Os testes precisam rodar em um computador, sem hardware.
