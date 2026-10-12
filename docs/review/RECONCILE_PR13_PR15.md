# Reconciliação dos PRs #13 e #15 com a `main` / Reconciling PRs #13 and #15 with `main`

**Tudo é DEMO / TESTNET. / Everything is DEMO / TESTNET.** Medição feita em worktrees descartáveis; **nada foi empurrado, nenhum PR foi alterado, nada foi mesclado.**
Referência: `main` = `a384698` (CI verde). #13 = `sec/scrypt-pin-kdf` (`60935bc`), #15 = `fix/entropy-no-weak-fallback` (`f74b0f0`). Os dois têm base `86cda6f` e estão **46 commits atrás** da `main`.
O #15 está **empilhado** no #13 (o #13 é ancestral dele).

**EN summary.** Merging current `main` into #13 conflicts in 7 files (`init/sec.c`, `init/sec.h`, `init/ui.c`, `tests/sec_test.c`, `Makefile`, `README.md`, `CONTRIBUTING.md`);
merging that result into #15 conflicts in `Makefile` only, plus one test stub that no longer compiles. The dangerous conflict is the body of `pin_check()`:
**taking either side whole silently removes either the scrypt verifier or the authorization grant (N07)**. It was resolved by hand in a throwaway worktree and re-tested
(1698 checks for #13, 1707 for #13+#15; ASan/UBSan, lint and fuzz clean; the same totals under emulated aarch64). This is **not** an approval of #13: the security review is still missing.

## 1. Conflitos de #13 + `main`

| Arquivo | Hunks | O que conflita | Resolução usada no teste |
|---|---|---|---|
| `init/sec.c` | 1 | **Corpo do `pin_check()`**: o #13 troca o verificador por scrypt; a `main` (N07) emite a concessão `auth_on_pin_ok(now)` só no `pin_check` certo | Corpo do #13 (legado v1 + v2) **com** `auth_on_pin_ok(now)` no caminho de sucesso (vale também para a migração v1→v2) |
| `init/sec.h` | 1 | #13 declara `pin_state_key`/`pin_forget_key`; a `main` declara limites do bloqueio (#30) e a API da concessão. **`PIN_MAX_FAILS` ficou definido duas vezes** | União dos dois; uma só definição de `PIN_MAX_FAILS` |
| `init/ui.c` | 3 | #13 apaga `pinbuf` com `sec_wipe`; a `main` chama `auth_expect`/`auth_clear` nos mesmos pontos | `sec_wipe` **e** `auth_expect`/`auth_clear` |
| `tests/sec_test.c` | 1 | Estruturas diferentes (o #13 usa a função `T()` e 67 verificações; a `main` usa `puts` e tem os testes do bloqueio, da concessão e do relógio) | Corpo do #13 + os testes da `main` reescritos para `T()`, com custo de scrypt pequeno (`pin_kdf_cost(10,8,1)`) |
| `Makefile` | 6 | `TESTS`, fontes, padrão de contagem, alvos de fuzz (`pin` do #13, `cmdline` da `main`) | União; `crypto_diff` mantido com o `sh -c` para o pipe |
| `README.md`, `CONTRIBUTING.md` | 1 + 2 | Linha do `make fuzz` | União das duas descrições |

Do #13 para o #15 (sobre o resultado acima): conflito só no `Makefile` (`TESTS` e padrão de contagem, para acrescentar `entropy`) e **um teste que deixa de compilar**: `tests/ui_test.c` define `void plat_random(...)`, e o #15 muda a assinatura para `int plat_random(...)` (0 ok, -1 falha). Já estava previsto no texto do PR 31. Ajuste: devolver `int` (0).

## 2. O ponto perigoso: `pin_check()`

```c
/* resultado usado no teste (resumo) */
if (PIN.ver == 1) { /* verificador antigo; se certo, migra para v2 com o PIN em mãos */ ... }
else               { /* scrypt: recusa tamanho 0/ >64; falha do KDF devolve 3 e NÃO conta como erro */ ... }
if (ok) { PIN.fails = 0; auth_on_pin_ok(now); return 0; }   /* o ÚNICO lugar que emite concessão */
```

- Se alguém resolver o conflito **pegando o lado do #13**, o `auth_on_pin_ok(now)` some: nenhum pagamento seria autorizado. Falha para o lado seguro, mas quebra o produto e **ninguém perceberia pela suíte do #13 sozinha** (ela não conhece a concessão).
- Se pegar o **lado da `main`**, o scrypt some e o verificador volta a SHA-256 × 50.000, e a suíte do #13 falha (isso se perceberia).
- O resultado correto tem as duas coisas. Os 16 testes de interação (`docs/review/reconcile/pr13_main_interaction_test.c`, só referência) verificam: a concessão sai no PIN certo e **também** na migração v1→v2; PIN errado não emite; a concessão é só da transação esperada; **PIN bloqueado não roda o KDF** (a verificação bloqueada leva ~0 ms com o custo padrão, então não há consumo de CPU/memória nem oráculo); a chave de estado só existe depois de um PIN certo e some em `pin_forget_key`; o bloqueio vindo do outro relógio fica ≤ 1920 s com uma linha `P2`.

## 3. Política do contador: o #13 recusa, a `main` limita

| | #13 (`pin_deserialize`) | `main` (#30, `pin_check`) |
|---|---|---|
| Contador `INT_MAX`, negativo ou > 1000 no arquivo | **recusa a linha inteira** | satura/normaliza e segue |
| `locked_until` negativo | recusa | n/a |
| `locked_until` longe demais (outro relógio) | aceita | ancora uma vez, limite 1920 s |

As duas políticas coexistem sem contradição, mas têm uma **consequência que vale decidir de propósito**: quando `pin_deserialize` recusa, o `load_state()` de `init/main.c` restaura o PIN anterior (não definido num boot novo) e sobe com **estado novo** (`"linha do PIN recusada — seguindo com estado novo"`). Ou seja, um contador fora da faixa com CRC válido vira **"criar PIN novo, livro-razão vazio"**, não "PIN mantido com contador limitado". Isso não dá acesso a fundos de ninguém (o livro-razão some junto) e é coerente com o modelo "tudo ou nada" já aceito (N15), mas **é perda de dados por um byte adulterado ou corrompido com CRC certo**. Decisão para o mantenedor: manter a recusa (simples, previsível) ou tolerar e limitar como a `main` faz. Nenhum teste existente cobre este comportamento ponta a ponta no `main.c` (ele não compila no computador).

## 4. Resultados (computador)

| Conjunto | `make test` | `make test-asan` | `make lint` | `make fuzz` (15 s por alvo) | aarch64 emulado |
|---|---|---|---|---|---|
| `main` `a384698` | 779 | ok | ok | ok (30 s) | 779 |
| #13 + `main` (teste) | 1698 | ok | ok | ok (`devcmd`, `money`, `cmdline`, `pin`) | — |
| #13 + #15 + `main` (teste) | **1707** | ok | ok | ok | **1707** |

`sec`: 96 verificações (os testes do #13 mais os do bloqueio, da concessão e do relógio que a `main` ganhou; os 7 testes básicos da `main` que repetiam os do #13 não foram duplicados). Compilação nativa para o alvo do `build.sh` (`aarch64-linux-musl`, `cortex_a53`, estático) não foi refeita com o #13/#15; o teste em aarch64 é emulado (ver `ISSUE9_CLOCK_AND_ARM64_CI.md`), **não é o aparelho**.

## 5. O que NÃO foi verificado

- Nada rodou no X669C. O custo do scrypt no aparelho é desconhecido (HW-07); a tela de desbloqueio **trava** enquanto o KDF roda.
- **Revisão independente de `init/sec.c`: continua faltando.** Quem escreveu e quem auditou é a mesma pessoa. O resultado verde aqui **não** é aprovação.
- O caminho `ui.c` → `plat_random` → "não cria PIN se não há aleatoriedade" (#15) só tem teste do `entropy.h`, não do `ui.c`.
- A resolução acima foi feita por mim, à mão, para medir. Ela deve ser **refeita e revisada** por quem fizer o merge real.

## 6. Procedimento recomendado (para o mantenedor, depois da revisão de segurança)

1. Revisar `init/sec.c` do #13 (independente) e decidir a política do contador (seção 3).
2. No branch do #13: `git merge origin/main` (merge, não rebase; sem force-push). Resolver pela tabela da seção 1.
3. Rodar `make test`, `make test-asan`, `make lint`, `make fuzz` e o teste de interação.
4. Só então mesclar o #13. Depois, no branch do #15: `git merge origin/main` (o conflito do `Makefile` e o `plat_random` do `ui_test`).
5. A decisão sobre o #13 continua **REQUEST CHANGES** até os itens da seção 11 de `PR13_CRYPTO_AUDIT.md`.
