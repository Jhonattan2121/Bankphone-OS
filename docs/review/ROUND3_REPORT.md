# Relatório da rodada 3 / Round 3 report

**Tudo é DEMO / TESTNET. / Everything is DEMO / TESTNET.** Base: `main` em `32932dd`. Nada foi verificado no Infinix Hot 30i X669C.

**EN summary.** Round 3 re-established the facts on the exact `main` SHA, reproduced each suspected defect with a small program (UBSan where it
is arithmetic) **before** writing a fix, and then fixed only what was proven, in small separate branches. Eight branches are pushed and **none is
merged or opened as a PR by this work**. The money engine now validates ranges **before** it multiplies, uses functional limits (not `int64`
limits), recomputes a swap quote instead of trusting the caller, and everything it writes is accepted by its own loader. PR #13 stays at
REQUEST CHANGES. Five new hardware rows, a state-integrity proposal and the invariant matrix are documented. Open items are listed, not hidden.

Níveis de evidência / evidence levels: **[A]** código lido · **[B]** teste ou reprodução no computador · **[C]** aparelho (nenhum) · **[D]** planejado.

## 1. Método

1. Worktree limpa da `main` em `32932dd` (sem tocar no checkout de trabalho). `make test`, `make test-asan`, `make lint` e `make fuzz` reproduzidos.
2. Cada suspeita virou um programa mínimo contra a API pública, compilado com `-fsanitize=undefined -fno-sanitize-recover=undefined`. Só então houve correção.
3. A correção valida **antes** de calcular (limite funcional + `_Static_assert` do produto), não detecta estouro depois.
4. Teste de regressão que **falha** no código antigo (mutação: remover a correção → teste ou fuzz precisa falhar).
5. Cada branch passa `make test`, `make test-asan` e `make lint` sozinha; as oito juntas (worktree de integração) passam 779 verificações sem conflito.
6. Varredura independente (5 buscadores) + 2 verificadores por achado instruídos a **refutar**; ver `MONEY_INVARIANTS.md` §3.1.

## 2. Branches (nenhuma mesclada, nenhum PR aberto por este trabalho)

| # | Branch | Commit | Resolve | Depende de |
|---|---|---|---|---|
| 1 | `fix/money-engine-overflow-limits` | `dbee845` | D-1 a D-5 (overflow de `m_quote`, `m_total_brl`, saída que o carregador recusa, cotação forjada) | `main` |
| 2 | `fix/money-engine-clock-range` | `aa94316` | D-6, D-7 (relógio fora de faixa, Pix sem teto) | 1 |
| 3 | `fix/money-swap-pending-table` | `e7f776d` | D-8, D-9 (tabela de cotações pendentes) | 2 |
| 4 | `fix/cmdline-truncation-fails-closed` | `9a68db0` | trava de escrita `bankphone.ro=1` com cmdline cortada, nome de partição truncado | `main` |
| 5 | `fix/store-never-overwrite-only-good-slot` | `acdef27` | perda de dados: a gravação sobrescrevia o único slot bom | `main` |
| 6 | `fix/pin-lockout-bounded-clock` | `9e74f13` | bloqueio permanente do PIN entre domínios de relógio; contador `int` | `main` |
| 7 | `fix/ui-input-and-display` | `262cbdb` | valor USDC truncado em `int`, tabela de toques cheia, contagem do bloqueio, `approx_usdc` | `main` |
| 8 | `test/image-tools` | `4744fae` | só testes de `mkboot.py` e `mkramdisk.py` | `main` |

Ordem sugerida de mesclagem: 1 → 2 → 3 (empilhadas; abrir o PR de 2 e 3 depois da anterior, ou apontar a base para a branch anterior); 4, 5, 6, 7 e 8 são
independentes entre si. **Aviso:** quando o #13 e o #15 forem mesclados, `fix/pin-lockout-bounded-clock` e o `plat_random` do `ui_test` precisam de ajuste;
`fix/ui-input-and-display` também toca `sheets.c`.

## 3. Resultado dos testes (computador, x86_64)

| Alvo | `main` | com as branches |
|---|---|---|
| `money_test` | 93 | 228 |
| `sec_test` | 7 | 27 |
| `cmdline_test` | 36 | 59 |
| `test_store` | 171 | 257 |
| `ui_test` | — | 40 |
| `image_tools_test` | — | 35 |
| Conjunto (`make test` e `make test-asan`) | — | **779**, 0 falhas |

Fuzz (semente fixa, 25–30 s por alvo): `fuzz_money` sorteia fronteiras e extremos de `int64`, relógios inválidos e cotações forjadas; sem falha nas branches corrigidas.
**Mutação:** com a correção removida, o fuzz aborta em segundos. **Nada rodou em ARM64.**

## 4. Decisões de projeto que valem ser lembradas

- **Limite funcional, não de tipo:** `MAX_AMOUNT 1e15`, `MAX_SWAP_BRL 1e11` (R$ 1 bi), `MAX_SWAP_USDC 1e14`, `MAX_TIME 1e11`. Os tetos são escolhidos para que o produto caiba em `int64` (`_Static_assert`) e para que a saída caiba no que o carregador aceita.
- **Fechamento:** tudo que o motor grava, `m_deserialize` aceita de volta, ou o livro-razão inteiro some no boot seguinte (o carregador é tudo-ou-nada).
- **O motor recalcula a cotação:** `m_prepare_swap` compara a cotação do chamador com `swap_calc` e recusa com "Quote invalid. Nothing was moved.".
- **Bloqueio do PIN:** a data absoluta guardada é ancorada uma só vez e limitada a 1920 s; um relógio fora de faixa devolve o máximo, nunca zero.
- **Store:** `slot_ok` passa a significar "cabeçalho e payload com CRC conferido".

## 5. Não corrigido, e por quê

Ver `MONEY_INVARIANTS.md` §4 (O-1…O-8). Os principais: **O-1** (espera de 30 s > validade de 20 s: swap acima de R$ 5.000 nunca é autorizado; decisão de produto),
**O-2/O-3** (domínio do relógio; precisa da #9), **O-4** (executar antes de gravar; só importa com provedor real).
Não foi feita a centralização das constantes DEMO (N12) nem a proposta de CI ARM64.

## 6. Pendente de hardware

Ver `HARDWARE_TESTS.md` (linhas HW-15 a HW-19 novas). Nenhum teste de aparelho foi executado; **nenhuma gravação foi feita**.

## 7. Riscos que continuam

- Autor = auditor em `sec.c`; falta revisão humana independente.
- PIN de 6 dígitos não resiste a ataque offline; o estado não é autenticado (ver `docs/STATE_INTEGRITY.md`).
- Só existe prova em x86_64; o comportamento em ARM64 e a tela real do X669C não foram vistos.
- A verificação adversarial dos achados depende de agentes automáticos; os resultados dela reduziram a prioridade de vários candidatos, mas não são revisão humana.

## 8. Próxima tarefa recomendada

Abrir os PRs 1 a 3 nesta ordem, decidir O-1 (um swap acima de R$ 5.000 hoje nunca completa) e só então atacar a #9 (domínio do relógio + estado autenticado).
