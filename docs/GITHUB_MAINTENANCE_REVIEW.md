# BANKPHONE OS: revisão de manutenção do GitHub / GitHub maintenance review

**Tudo é DEMO / TESTNET. / Everything is DEMO / TESTNET.** Não existe dinheiro real, Pix real nem BRL real.

Data da revisão / review date: 2026-10-11. Repositório: `Jhonattan2121/Bankphone-OS`. `main` em **`86cda6f`**.

**EN summary.** This document records the real state of the repository so that another person can understand it,
review each decision, repeat the host tests and see exactly what is still missing to validate BANKPHONE OS on the
real phone. There are 3 open pull requests (#13, #14, #15), 5 more branches pushed without a pull request, 11 open issues,
and 20 proposed issue drafts (N01–N20), **none published**. All the evidence in this document is **from a PC
(Linux x86_64 container)**: **nothing was verified on the Infinix Hot 30i X669C**. The audit of PR #13 found and fixed
one real defect (a tampered failed-try counter overflowed an integer) and the PIN protection still needs an
independent review and a measurement on the phone before it can be called validated. No issue was closed, no pull
request was merged, no issue was published, and nothing was flashed.

**Documentos desta revisão / documents in this review:**

| Documento | Para quê |
|---|---|
| [`docs/GITHUB_MAINTENANCE_REVIEW.md`](GITHUB_MAINTENANCE_REVIEW.md) (este) | estado, matriz de dependências, ordem de merge, testes, pendências |
| [`docs/review/PR13_CRYPTO_AUDIT.md`](review/PR13_CRYPTO_AUDIT.md) | auditoria criptográfica do PR #13 e a decisão |
| [`docs/review/PR_TEXTS.md`](review/PR_TEXTS.md) | títulos, descrições e critérios dos PRs N04/N14/N15/N05/script; decisão do #14; atualizações das issues #2 e #10 |
| [`docs/review/HARDWARE_TESTS.md`](review/HARDWARE_TESTS.md) | tabela de testes que exigem o aparelho |

## 0. Como ler: níveis de evidência / how to read: evidence levels

Cada afirmação deste documento tem um destes níveis. **Um nível mais alto não é inferido de um mais baixo.**

| Nível | Significa | Exemplo |
|---|---|---|
| **A. Verificado no código** | Eu li o código e o comportamento decorre dele | "`pin_check` devolve 3 se o scrypt falha" |
| **B. Verificado por testes no host** | Um teste ou experimento rodou **num PC** (container Linux x86_64) | "76 verificações do `sec_test` passam" |
| **C. Verificado em hardware real** | Evidência **arquivada** (log, foto, captura) vinda do Infinix Hot 30i X669C | **nenhuma neste momento**. O README da `main` registra, **sem arquivo de evidência**, que o comando USB reiniciou um X669C no fastboot e que o `fastboot fetch` foi recusado (relatado pelo autor, commit `c2bd14d`) |
| **D. Planejado, não implementado ou não comprovado** | Existe como intenção, rascunho ou hipótese | "N02: provar o fallback A/B" |

Regra: **CI verde, compilação e testes de host não provam que o sistema inicia ou funciona no aparelho.** Quando o
autor relata que algo funcionou no aparelho (dev logs #1 e #2), isto fica como **"relatado pelo autor, sem evidência no
repositório"**, que não é o nível C.

## 1. Estado da `main` / state of `main`

| Item | Valor |
|---|---|
| Commit | **`f80968d`** — *feat: real screenshots over USB, bilingual installer*. A revisão e os testes das seções 2–7 foram feitos sobre `86cda6f`; a `main` depois recebeu **2 commits do autor** (`c2bd14d` instalador e README com o que foi conferido num X669C; `f80968d` comandos `SCREENSHOT`/`TOUCH`, `tools/screenshot.py`, instalador bilíngue). Todos os merges abaixo foram **simulados sobre `f80968d`** (seção 10.1) |
| Conteúdo | Código C do sistema (`init/`), testes de host, Makefile e CI (PR #12 já integrado), CONTRIBUTING, scripts |
| CI da `main` | Linux: `make test`, `make test-asan`, `make lint`, `make fuzz FUZZ_TIME=20`. macOS: `make test`, `make lint` (sem os testes e lint só de Linux) |
| Testes da `main` | 283 verificações, ASan/UBSan e lint passam (ver seção 6) |
| O que **não** está na `main` | scrypt do PIN (#13), auditoria e rascunhos (#14), entropia segura (#15), correções N04, N14, N15, script de publicação |

## 2. Pull requests

| PR | Branch | Estado | CI | Commits à frente da `main` | Depende de | Decisão / pendência |
|---|---|---|---|---|---|---|
| **#12** | `ci/makefile-sanitizers-fuzz` | **Fechado**; o conteúdo está na `main` (`86cda6f`) | — | — | — | Feito. A issue #10 continua aberta (seção 4) |
| **#13** | `sec/scrypt-pin-kdf` | Aberto, `mergeable_state: clean`; cabeça `60935bc` | `linux` e `macos` verdes (rodada sobre `0255d37`; `60935bc` ainda sem rodada conferida) | 6 | — | **REQUEST CHANGES → corrigido na branch; falta revisão independente** e medição no aparelho. Ver `review/PR13_CRYPTO_AUDIT.md`. **Não aprovar só porque os testes passam** |
| **#14** | `docs/project-audit` | Aberto, só documentação; cabeça `cd02a59` | `linux` e `macos` verdes | 2 | — | **Recomendo o merge** depois da sua leitura. Ver `review/PR_TEXTS.md` §6. Duas correções ficam neste PR de manutenção (§9) |
| **#15** | `fix/entropy-no-weak-fallback` | Aberto com base `main`; cabeça `cf94032` | `linux` e `macos` verdes | 5 (4 são do #13) | **#13** | Trocar a base para `sec/scrypt-pin-kdf` ou esperar o #13. A branch **não tem** os 2 commits novos do #13 (§3) |
| — | `fix/boot-b-script-slot-safety` (N04) | **Sem PR aberto** | não há PR | 1 | — | Abrir o PR; texto pronto em `review/PR_TEXTS.md` §1 |
| — | `fix/cmdline-exact-token-parser` (N14) | **Sem PR aberto** | não há PR | 1 | — | Texto em §2 |
| — | `fix/state-load-validated-idempotent` (N15) | **Sem PR aberto**; cabeça `6f76322` | não há PR | 2 | — | Texto em §3 |
| — | `tools/publish-issues-script` | **Sem PR aberto**; cabeça `116a140` | não há PR | 3 (inclui o commit da N04) | **N04** | Texto em §5 |
| — | `docs/github-maintenance-review` (esta) | **Sem PR aberto** | — | — | **#14** | Documentação desta revisão |

"CI verde" vem da leitura das execuções do GitHub Actions nos PRs abertos (nível B: CI roda os mesmos testes de host).

### 2.1 Conferência dos diffs (resumo)

| Branch | Arquivos / linhas | Só o esperado? |
|---|---|---|
| #13 | 18 arquivos, +884/−38 (`sec.c/h`, `ui.c`, testes, fuzz, `THREAT_MODEL.md`, `bench_kdf.c`, Makefile) | Sim; nenhum arquivo de aparelho, firmware ou segredo |
| #14 | 24 arquivos, +1584 | Sim; só `ROADMAP.md` e `docs/` |
| #15 | 23 arquivos (4 commits do #13 + 1 próprio: `entropy.h`, `entropy_test.c`, `main.c`, `host.c`, `ui.c/h`, Makefile) | Sim, mais o #13 |
| N04 | 4 arquivos, +249/−17 (`test_boot_b.sh`, teste, Makefile) | Sim |
| N14 | 17 arquivos, +252/−14 (`cmdline.h`, `main.c`, testes, fuzz e corpus) | Sim |
| N15 | 7 arquivos (`money.c/h`, `store.c`, testes, fuzz) | Sim |
| Publicação | 6 arquivos (`publish-issues.sh`, teste, Makefile e os da N04) | Sim |

Verificação extra feita em todas as branches: **autor único** (Jhonattan Ferri, e-mail `noreply` do GitHub), **sem
trailer de coautoria** e **nenhuma menção a ferramenta de IA** nos arquivos, exceto: o `LICENSE` pré-existente (palavra
em contexto jurídico) e o próprio `publish-issues.sh`/seu teste, que **contêm a lista de palavras que ele recusa**.

## 3. Matriz de dependências entre branches e PRs

```
main (86cda6f)
 ├── #14  docs/project-audit ──────────────► docs/github-maintenance-review   (docs; depende do #14)
 ├── #13  sec/scrypt-pin-kdf ──► #15 fix/entropy-no-weak-fallback              (#15 usa o scrypt do #13)
 ├── N04  fix/boot-b-script-slot-safety ──► tools/publish-issues-script        (compartilham SCRIPT_TESTS)
 ├── N14  fix/cmdline-exact-token-parser                                       (independente)
 └── N15  fix/state-load-validated-idempotent                                  (independente)
```

| Par | Tipo | Conflito esperado | Observação |
|---|---|---|---|
| #15 → #13 | Dependência **real** de código (`pin_set` do scrypt) | — | Falta trazer `0255d37` e `60935bc` do #13 para a branch do #15 (merge, sem reescrever histórico). **Eu não fiz**: o ambiente bloqueou o `merge`+`push` na branch do #15 e eu não contornei |
| Publicação → N04 | Dependência de **Makefile** (`SCRIPT_TESTS`) | Pequeno, no `Makefile` | A branch contém o commit da N04; ao mesclar a N04 primeiro, o diff da publicação encolhe |
| N04, N14, N15, #13, #15 entre si | Todos mexem no `Makefile` (lista de testes) e alguns em `main.c`/`ui.c` | **Sim, pequenos, no `Makefile`**; `main.c` entre N14 e N15 (funções diferentes) | Resolver com merge da `main`; nunca com force push |
| Docs → #14 | A revisão cita arquivos do #14 | Nenhum | Abrir depois do merge do #14 |

## 4. Issues #1–#11

Prioridade: **P0** bloqueia boot/recuperação/integridade · **P1** necessária para demonstração confiável no aparelho ·
**P2** qualidade e segurança do protótipo · **P3** evolução futura. As prioridades vêm de `docs/issues/README.md` (#14) e
do plano; são **propostas**.

| # | Título (resumo) | Estado | Prior. | Progresso **verificável** | Relação com PRs | Próxima ação | Fechar? |
|---|---|---|---|---|---|---|---|
| 1 | Carteira offline: BIP39, secp256k1, Hive | Aberta | P3 | Nada implementado (nível D) | — | Esperar hardware validado (M2–M5) | Não |
| 2 | KDF que gasta memória + modelo de ameaças | Aberta | P1 | PR #13 aberto; KDF e modelo em código/docs (A, B); **custo no X669C não medido** | #13 (+ #15 para o sal) | Revisão independente do `sec.c`, medir no aparelho (HW-07), decidir downgrade | **Não** até o #13 mesclado **e** medido |
| 3 | PIN de coação | Aberta | P3 | Nada (D) | — | Depois do #2 e #9 | Não |
| 4 | Tela de confirmação do sistema, amarrada ao hash | Aberta | P3 (liga à N07, P1) | Nada (D). F-06: o motor aceita o nível de autenticação do chamador | — | Fazer a N07 antes | Não |
| 5 | Pagamento offline celular a celular | Aberta | P3 | Nada (D) | — | — | Não |
| 6 | Recibos assinados | Aberta | P3 | Nada (D); recibo DEMO existe na UI | — | — | Não |
| 7 | Cofre com tempo | Aberta | P3 | Nada (D) | — | — | Não |
| 8 | Atualizações assinadas, A/B, anti-downgrade | Aberta | P1 | Nada de assinatura; o script A/B seguro vem na N04 (B); **fallback A/B não provado** | N04 (parcial) | Primeiro N02 (provar o fallback, HW-03) | Não |
| 9 | Estado cifrado, escrita atômica, rollback, relógio monotônico | Aberta | P1 | **Parcial:** `store.c` com 2 slots, PENDING e CRC, 171 verificações no host. Falta cifra, autenticação, rollback, relógio monotônico, contador antes da conferência | N15 (carga validada) | Depois do #13; a chave de estado do #13 ainda não é usada | Não |
| 10 | CI, Makefile, sanitizers e fuzzing | Aberta | P2 | **Parcial** (PR #12 mesclado). A estimativa "≈ 70%" **não é critério**; a lista exata do que falta está em `review/PR_TEXTS.md` §9 | #12 (feito) | Publicar a atualização da lista; ARM64 no CI é a N18 | **Não** até a lista fechar |
| 11 | Pix/BRL reais via parceiro licenciado (discussão) | Aberta | P3 | Só discussão; nada implementado. **Fora do roteiro** | — | Manter como discussão de arquitetura | Não |

**Nenhuma issue deve ser fechada agora.** Os critérios de aceite de #2 e #10 não foram cumpridos.

## 5. Rascunhos N01–N20 e as issues existentes

**Estado:** os 20 são arquivos Markdown em `docs/issues/` (PR #14). **Nenhum está publicado.** O repositório só tem as
issues #1–#11. A conferência foi feita comparando cada rascunho com os títulos e o conteúdo de #1–#11.

| ID | Prior. | Título (resumo) | Duplica? | Relação com existente | Estado do código hoje |
|---|---|---|---|---|---|
| N01 | P0 | Fluxo de boot, árvore de falhas, hipótese do watchdog | Não | liga a #8 | Sem correção; precisa do aparelho |
| N02 | P0 | Provar o fallback A/B; guia de recuperação | Não | **Prova** o que o #8 assume | Sem correção; precisa do aparelho |
| N03 | P1 | Contrato do PID 1 (tratador de falha, logs) | Não | — | Sem correção (F-09) |
| N04 | P1 | `test_boot_b.sh` pode regravar o slot ativo | Não | liga a #8 | **Corrigido na branch N04** (B), sem PR |
| N05 | P1 | `plat_random` com plano B previsível | Não | liga a #2 | **Corrigido no PR #15** (B), depende do #13 |
| N06 | P1 | Relatório grava em `expdb` por padrão | Não | — | Sem correção (F-05) |
| N07 | P1 | Motor aceita o nível de autenticação do chamador | Não | pré-requisito do #4 | Sem correção (F-06) |
| N08 | P1 | Display no X669C com evidência | Não | — | Precisa do aparelho |
| N09 | P1 | Toque no X669C com evidência | Não | — | Precisa do aparelho |
| N10 | P1 | Protocolo serial/USB especificado | Não | — | Sem correção |
| N11 | P1 | Invariantes e limites do motor financeiro | Não | — | Parcial: 40 verificações + fuzz; falta a especificação |
| N12 | P1 | Centralizar o modo DEMO (cotação, tarifa) | Não | — | Sem correção (F-10) |
| N13 | P1 | README com estado real e níveis de maturidade | Não | — | Sem correção |
| N14 | P2 | Parser da cmdline por token exato | Não | — | **Corrigido na branch N14** (B), sem PR |
| N15 | P2 | Carregamento idempotente e esquema validado | Não | liga a #9 | **Corrigido na branch N15** (B), sem PR. O texto do rascunho tem um erro (§9) |
| N16 | P2 | Referências quebradas a ferramentas e caminhos | Não | — | Sem correção (F-07) |
| N17 | P2 | Logs padronizados, sem identificadores | Não | — | Sem correção (F-08); HW-09 |
| N18 | P2 | Build reproduzível e CI com ARM64 | Não | **Resto do #10** | Sem correção |
| N19 | P2 | `cpu_boost` padrão e avisos de compilação | Não | — | Sem correção (hipótese H-4) |
| N20 | P2 | Roadmap, templates, rótulos | Não | — | Parcial: `ROADMAP.md` no #14 |

Conclusão: nenhum rascunho duplica uma issue existente. Vários se relacionam com #2, #4, #8, #9 e #10 e **referenciam**
a issue em vez de repetir. N02 (provar o fallback A/B) e N18 (ARM64 no CI) **se sobrepõem em parte** a #8 e #10: são
complementares (a prova, o que falta do CI), não duplicatas; confirme isso na hora de publicar. Publicar com `scripts/publish-issues.sh` só **depois** do merge do #14 e da revisão. Mesmo
sem publicar as 20, **as de maior risco concreto** são N04, N14 e N15, que já têm correção (a publicar como issue só
se você quiser rastrear: o PR basta).

## 6. Testes executados / tests run

**Ambiente / environment:** container Linux x86_64 (kernel 6.18), `cc` do sistema, Python 3 com `hashlib.scrypt`, 4
núcleos. **Não é o aparelho e não é ARM64.** Comandos: `make test`, `make test-asan`, `make lint`, `make fuzz`
(30 s por alvo, semente fixa). Cada branch foi testada **isolada** num `git worktree` do commit citado.
Nível B para tudo abaixo.

| Branch @ commit | `make test` (verificações) | `make test-asan` | `make lint` | `make fuzz` (30 s por alvo, execuções) |
|---|---|---|---|---|
| `main` @ `86cda6f` | 283 OK | OK | OK | devcmd 4,63 M · money 2,52 M |
| #13 `sec/scrypt-pin-kdf` @ `60935bc` | 1202 OK (`sec` 76, `crypto_diff` 850) | OK | OK | devcmd 4,62 M · money 2,60 M · **pin 19,71 M** |
| #14 `docs/project-audit` @ `cd02a59` | 283 OK | OK **(ver nota)** | OK | devcmd 4,64 M · money 2,58 M |
| #15 `fix/entropy-no-weak-fallback` @ `cf94032` | 1205 OK (`entropy` 9) — **sem** os 2 commits novos do #13 | OK | OK | devcmd 4,73 M · money 2,67 M · pin 19,86 M |
| N04 `fix/boot-b-script-slot-safety` @ `537d1ab` | 303 OK (`boot_b` 20) | OK | OK | devcmd 4,58 M · money 2,64 M |
| N14 `fix/cmdline-exact-token-parser` @ `153e963` | 319 OK (`cmdline` 36) | OK | OK | devcmd 4,53 M · money 2,64 M · **cmdline 10,39 M** |
| N15 `fix/state-load-validated-idempotent` @ `6f76322` | 313 OK (`money` 69, `store` 172) | OK | OK | devcmd 4,60 M · money 2,24 M |
| `tools/publish-issues-script` @ `116a140` | 334 OK (`boot_b` 20, `publish_issues` 31) | OK | OK | devcmd 4,63 M · money 2,66 M |

Todas as execuções de fuzz terminaram **sem falha**. As contagens de execuções variam de uma rodada para outra e
não são comparáveis entre máquinas.

**Limites destes números:**
- "Verificações" são linhas `PASS` (ou o contador do próprio teste); não medem cobertura.
- Fuzzing de 30 s por alvo **não é** prova de ausência de erros. O fuzz de 180 s do `fuzz_money` que eu havia
  planejado **não foi repetido** nesta rodada.
- `crypto_diff` só roda com `hashlib.scrypt`; sem ele é pulado (neste ambiente rodou).
- O macOS foi conferido só pelo CI do GitHub, não localmente.
- O teste `store` usa o caminho fixo `/tmp/bp-store-test.img`: **dois `make test` ao mesmo tempo na mesma máquina se
  atrapalham** (isso me deu uma falha falsa, explicada abaixo). Rode um de cada vez.
- Nada disso foi executado no aparelho.

**Nota sobre o `test-asan` do #14.** Na primeira rodada em lote, o `test-asan` do #14 **falhou** no teste `store`.
Eu estava rodando outro `make test` em paralelo na mesma máquina e o teste `store` usa o arquivo fixo
`/tmp/bp-store-test.img`: os dois se atrapalharam. Repeti **sozinho, duas vezes**: passou nas duas (283 verificações). O
resultado da tabela é o da repetição isolada. Isto é uma **fragilidade do teste** (caminho fixo, sem isolamento) e
não do código; fica registrada como melhoria pequena (usar um diretório temporário por execução).

### 6.1 Como reproduzir / how to reproduce

```sh
git fetch origin
git worktree add /tmp/bp-check origin/<branch>        # uma branch por vez
cd /tmp/bp-check && make test && make test-asan && make lint && make fuzz FUZZ_TIME=30
```
O benchmark do KDF (`make bench-kdf`) existe só na branch do #13 e mede **o computador onde roda**.

## 7. Bugs corrigidos, evidências e regressões a verificar

| ID | Bug | Como foi confirmado (A/B) | Correção | Teste de regressão | A verificar depois |
|---|---|---|---|---|---|
| F-01 / N04 | `test_boot_b.sh` regravava o slot ativo | Lido no código (A) | branch N04 | 20 verificações com `fastboot` falso (B) | `fastboot getvar` real no X669C (HW-01/03) |
| F-02 / N14 | `plat_boot_prop` por `strstr`: depende de ordem/prefixo | **Reproduzido** com a função antiga (B) | branch N14 | 36 verificações + fuzz (B) | cmdline real do aparelho (HW-09) |
| aliasing | `plat_boot_prop` devolvia 1 buffer estático para 3 chaves | **Reproduzido** (B) | branch N14 (8 buffers + cópia local) | teste "3 leituras não apontam para o mesmo buffer" (B) | — |
| F-03 / N15 | `store_open` vazava 1 descritor por reabertura | **Reproduzido** (B) | branch N15 | teste de 20 aberturas (falha sem a correção) (B) | — |
| N15 | `m_deserialize` sem validação; erro ignorado no `load_state` | Lido + fuzz (A,B) | branch N15 | 7 estados inválidos, cortes byte a byte, fuzz (B) | persistência real (HW-08) |
| N15 | `m_receive_pix` aceitava 0 e negativo | **Achado pelo fuzzer** (B) | branch N15 | 5 verificações (B) | — |
| F-04 / N05 | `plat_random` com plano B previsível | Lido (A) | PR #15 | 9 verificações do `entropy.h` (B). **Sem teste do caminho de `ui.c`** | bloqueio do `getrandom()` no boot (HW-13) |
| A-1 (#13) | Contador/`set`/`locked_until` do estado sem validação: **overflow com sinal** | **Reproduzido com UBSan** (B) | `0255d37` | 5 verificações que falham no código antigo (B) | — |
| A-2 (#13) | Dígitos do PIN ficavam nos buffers da UI | Lido (A) | `60935bc` | **sem teste automático** da UI | — |
| #12 | NUL depois do comando USB; leitura fora do limite na linha `T` sozinha | Fuzzer (B) | PR #12 (mesclado) | testes na `main` (B) | — |
| #13 | `memcpy(NULL, 0)`; PIN não definido gravado em forma ilegível | UBSan / fuzzer (B) | PR #13 | testes (B) | — |
| docs | "contador de erros só na memória" estava **errado** | Lido (A) | corrigido nos PRs #13 e #14 | — | corrigir também o **texto do PR #13 no GitHub** (`review/PR_TEXTS.md` §7) |
| Makefile | O contador do `make test` herdava o valor do teste anterior se o teste não estava na lista | Achado ao adicionar `entropy` (B) | branches N05 e publicação | — | — |

**Regressões ainda por verificar:** (a) depois de mesclar N14 e N15, rodar de novo a suíte completa (as duas mexem no
`Makefile`, e N15 e N14 mexem em `main.c`); (b) depois do #13 e do #15, repetir `crypto_diff` e `fuzz_pin`; (c) a UI
(`ui.c`) **não tem teste automático**: PIN, limpeza dos buffers e recusa por falta de entropia só se veem no
simulador ou no aparelho.

## 8. Pendências obrigatórias de hardware

A tabela completa (procedimento, resultado esperado, evidência necessária e estado) está em
[`docs/review/HARDWARE_TESTS.md`](review/HARDWARE_TESTS.md). Lista curta, **todas `NÃO VERIFICADAS`** (nível C: nenhum):

1. Boot e estabilidade (10 boots a frio) — HW-01
2. Watchdog (H-1) — HW-02
3. Fallback A/B (H-2) — HW-03
4. Display — HW-04 · Toque — HW-05
5. Protocolo serial e recuperação USB — HW-06
6. **Tempo do scrypt no X669C** — HW-07
7. Integridade do estado após reinício e queda de energia — HW-08
8. Identificadores no relatório de boot (F-08) — HW-09
9. Onde o relatório grava (F-05) — HW-10
10. Persistência do contador de erros do PIN — HW-11
11. `cpu_boost` e temperatura — HW-12 · `getrandom()` no boot — HW-13

Nenhum teste de hardware deve **gravar** nada automaticamente; qualquer gravação é manual e segue as regras do A/B.

## 9. Correções neste PR em relação ao #14 / corrections relative to PR #14

1. **N15 (rascunho):** o texto dizia para tornar `load_state()` idempotente "guardando que já carregou". Isso está
   **errado**: `ui_init()` zera `M` (livro-razão) e `PIN`, então o estado precisa ser carregado de novo depois dele. O
   defeito real é o descritor vazado (corrigido). O rascunho `docs/issues/N15-…md` foi corrigido nesta branch.
2. **Estado "não corrigido":** a auditoria descreve o commit `86cda6f`. N04, N05, N14, N15 e o contador de erros do
   #13 estão corrigidos fora do #14; isso é registrado aqui, sem reescrever o que foi observado.
3. **Contador de erros do PIN:** a redação do PR #13 no GitHub ainda diz "ainda não é persistente"; o correto está em
   `review/PR_TEXTS.md` §7.
4. **Alternativas a escrever o KDF do zero:** o #13 implementa SHA-256, HMAC, PBKDF2, HKDF e scrypt à mão porque o
   projeto não tem dependências externas. Isso é **risco criptográfico**: foi mitigado com vetores oficiais e 850
   casos contra o OpenSSL, mas uma biblioteca revisada (por exemplo libsodium, que tem scrypt e Argon2id) reduziria o
   risco e **não foi avaliada** aqui. Decidir isso é parte da revisão independente do #13.

## 10. Ordem de merges proposta / proposed merge order

**Proposta**, sujeita à leitura dos diffs e à sua aprovação. **Nada foi mesclado.** Antes de cada merge: conferir a
**base**, os **commits incluídos**, o **CI na cabeça** e se o diff tem **só o esperado**. Nunca force push nem reset
destrutivo; conflito se resolve com merge da `main` na branch.

| Passo | Merge | Condição antes | Depois |
|---|---|---|---|
| 1 | **#14** (documentação) | Você leu o diff; CI verde | Publicar as issues com o script, **uma** (`--only N05`) para testar |
| 2 | **#13** (scrypt do PIN) | **Revisão independente do `sec.c`**; decisão sobre o downgrade (R-1); texto do PR corrigido (§7 de `PR_TEXTS`). Merge **só para DEMO**; a issue #2 **fica aberta** | Medir no aparelho (HW-07) |
| 3 | **#15** (entropia) | Trazer os 2 commits novos do #13; trocar a base ou esperar o #13 | — |
| 4 | **N04** (script A/B) | PR aberto; CI verde | — |
| 5 | **Script de publicação** | N04 mesclada; conteúdo de N01–N20 revisado | Primeira execução real controlada |
| 6 | **N14** e **N15** | Independentes; atualizar com a `main` e rodar a suíte; resolver o conflito do `Makefile` | — |
| 7 | **Esta revisão** | #14 mesclado | — |

Se preferir priorizar risco concreto: **N04, N14 e N15** primeiro (gravação de partição, interpretação de parâmetros
de boot, integridade do estado), em paralelo com a revisão do #13.

### 10.1 Simulação dos merges sobre a `main` atual (`f80968d`)

Feita **localmente**, num `git worktree` descartável, **sem publicar nada**: cada branch sozinha e depois todas em sequência.

- **Cada branch sozinha sobre `f80968d`:** merge limpo, sem conflito, as 9.
- **Em sequência** (ordem da tabela acima, com a N07 antes desta revisão): **conflitos**, todos previstos, todos em arquivos de lista:
  1. `Makefile` ao juntar a N04 depois do #15 (lista de testes e laço do `make test`);
  2. `Makefile` ao juntar o script de publicação (lista de testes);
  3. `Makefile`, `README.md` e `CONTRIBUTING.md` ao juntar a N14 (alvos de teste e de fuzz; texto da lista de fuzzers);
  4. **N07 × #13**: `init/sec.c`, `init/sec.h`, `init/ui.c`, `Makefile` — as duas mexem no `pin_check` e nas mesmas linhas de limpar o PIN.
- **Resolução:** união das duas listas em cada conflito; no `pin_check` ficou a versão do #13 com a chamada que emite a concessão; na UI ficaram as limpezas do #13 mais `auth_expect`/`auth_clear` da N07. Nenhuma lógica foi descartada.
- **Resultado do conjunto integrado** (todas as branches mais os 2 commits novos da `main`): `make test` **1375 verificações OK**, `make test-asan` OK, `make lint` OK, `make fuzz` 20 s por alvo (devcmd, money, pin, cmdline) **sem falha**.
- **O que isso não prova:** é a **minha** resolução, feita no computador, e **não substitui** a revisão de cada diff nem o CI do GitHub; o macOS não foi simulado; nada rodou no aparelho.
- **Consequência prática:** a cada merge no GitHub, as branches seguintes podem ficar com conflito e o GitHub vai pedir resolução. Resolver com merge da `main` na branch (nunca force push). Se você preferir, eu faço esses merges nas branches antes de você clicar.

## 11. Próxima ação recomendada por item

| Item | Próxima ação | Quem |
|---|---|---|
| #14 | Ler o diff e fazer o merge | Você |
| #13 | Colar o texto corrigido (§7), pedir revisão independente do `sec.c`, decidir o downgrade | Você + revisor |
| #15 | Trazer os commits novos do #13 e trocar a base | Você (ou me autorize) |
| N04, N14, N15, publicação | Abrir os PRs com os textos prontos | Você |
| #2 | Colar a atualização (§8 de `PR_TEXTS`); manter aberta | Você |
| #10 | Substituir o comentário de "70%" pela lista (§9); manter aberta | Você |
| N01–N20 | Depois do #14: rodar `scripts/publish-issues.sh` sem `--publish`, depois com `--only` | Você |
| Aparelho | HW-01, HW-04, HW-05, HW-02, HW-07, HW-09, HW-08… nesta ordem | Você, com o aparelho |
| N07, N11, N06, N18 | Próximas correções de software | Eu, se você pedir |

## 12. O que esta revisão **não** fez

Não publicou issue, não fechou issue, não fez merge, não fez force push, não gravou nada no aparelho, não leu
firmware nem arquivos do aparelho, não consultou fontes externas sobre parâmetros do scrypt, não mediu nada em ARM64.
As únicas mudanças fora de documentação foram **correções de código com teste nas branches de PR já abertas ou
próprias** (`0255d37` e `60935bc` no #13; `6f76322` na N15; `116a140` no script de publicação) — listadas acima.
