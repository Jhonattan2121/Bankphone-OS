# Relatório da rodada 4 / Round 4 report

**Tudo é DEMO / TESTNET. / Everything is DEMO / TESTNET.** Base verificada: `main` = `a3846980e421d42e270ef5079d4e619bc52a0aa7` (merge do #28). CI do GitHub **verde** nesse commit.
**Nenhum teste foi feito no Infinix Hot 30i X669C: esta sessão não tem acesso ao aparelho** (sem USB, `adb`, `fastboot` nem `/dev/ttyACM*`).

**EN summary.** Round 4 confirmed the GitHub state, measured the conflicts of PRs #13 and #15 against current `main` in throwaway worktrees (nothing pushed or merged), produced a runbook for HW-01/04/05 (not executed: no device access), revised the state-integrity proposal, drafted the swap-above-R$ 5,000 product issue (not published), and planned issue #9 and ARM64 CI, with an emulated-aarch64 run of the whole suite. The decision on PR #13 stays **REQUEST CHANGES**. Every statement points to code, a reproducible test, or says "not verified".

## 1. Estado real (conferido agora, não presumido)

| Item | Estado |
|---|---|
| `main` | `a384698`; `make test` 779, `make test-asan`, `make lint`, `make fuzz` (30 s por alvo): ok; CI `ci` no commit: success |
| PRs abertos | **#13** (`sec/scrypt-pin-kdf`, `60935bc`) e **#15** (`fix/entropy-no-weak-fallback`, `f74b0f0`); bases `86cda6f`, **46 commits atrás** da `main`; #15 empilhado no #13 |
| Issues abertas | #1 a #11 (todas); **nenhuma foi fechada nem publicada nesta rodada** |
| Branches remotas | 21 (inclui as das rodadas anteriores, já mescladas) |

## 2. Fase 1: hardware (HW-01, HW-04, HW-05): **plano, não execução**
`docs/review/HW_PLAN_R4.md`: portões de segurança, procedimento, evidência, parada e modelo de arquivo. Dois achados do **plano**:
- `tools/screenshot.py` captura o **buffer em memória**, não o painel: para HW-04 é preciso foto do painel real.
- `screenshot.py --tap` **injeta** um toque pelo USB, não exercita o digitalizador: para HW-05 são precisos toques reais com o dedo.
Itens novos: **HW-20** (arquitetura do espaço de usuário) e **HW-21** (RPMB, só leitura). Todos `NÃO VERIFICADO`.

## 3. Fase 2: #13 e #15 com a `main` (`RECONCILE_PR13_PR15.md`)
- #13 + `main`: **7 arquivos** em conflito; #15 sobre o resultado: só o `Makefile` e **um teste que deixa de compilar** (`plat_random` passa de `void` a `int`).
- **Armadilha:** o corpo do `pin_check()` conflita; pegar um lado inteiro apaga o scrypt **ou a concessão de autorização (N07)**. Resolvido à mão **só no worktree de teste**, com os dois.
- Resultados: #13 + `main` = **1698** verificações; #13 + #15 + `main` = **1707**; ASan/UBSan, lint e fuzz (4 alvos) ok; **o mesmo 1707 em aarch64 emulado**; 16 testes de interação (referência em `docs/review/reconcile/`) todos PASS.
- Achado para decidir: o #13 **recusa** contador fora da faixa no carregamento e o `main.c` então sobe com **estado novo** (PIN e livro-razão apagados); a `main` (#30) **limita** em vez de recusar. É política a escolher.
- **Nada foi empurrado, mesclado ou rebaseado.** A decisão do #13 continua **REQUEST CHANGES**.

## 4. Fase 3: integridade do estado (`docs/STATE_INTEGRITY.md`, revisado)
Acrescentado: quatro propriedades que não se misturam (acidental, MAC, cifra, anti-rollback); a consequência de o verificador do PIN estar gravado ao lado do livro-razão (**a força do MAC é a resistência offline do PIN**: ≈ 33 a 39 h em um núcleo, ≈ 20 a 23 min com 100 núcleos, estimativa **não medida no aparelho nem em paralelo**); anti-rollback só com contador em hardware, **não verificado** (HW-21; **nunca gravar a chave da RPMB**); ciclo de vida da chave; critérios de decisão e 5 testes de aceite novos. **Nada implementado.**

## 5. Fase 4: swap acima de R$ 5.000 (`docs/issues/S01-p2-politica-swap-acima-de-5000.md`, **rascunho, não publicado**)
Descrito como decisão de produto, não como bug. Fatos do código; quatro opções; regras comuns; testes de fronteira. **Achado novo:** swaps **não têm teto por transação nem diário** (só Pix têm), então a soma de swaps de até R$ 5.000 não é limitada, enquanto o degrau mais alto nunca completa.

## 6. Fase 5: issue #9 e CI ARM64 (`ISSUE9_CLOCK_AND_ARM64_CI.md`)
- Inventário de usos do tempo; o que falta é a **janela diária** (carimbos gravados em unidades diferentes); regra conservadora proposta; matriz de testes; **sem alegar anti-rollback**.
- ARM64: o `init` compila **sem avisos** com a linha do `build.sh` (`aarch64-linux-musl`, `cortex_a53`, estático); suíte **779** verde sob `qemu-aarch64-static`; `char` é sem sinal no aarch64 e a suíte passa. **Emulação, não aparelho.** Jobs de CI propostos (**não adicionados**).

## 7. Bloqueadores e decisões que precisam de aprovação humana

| # | Decisão ou bloqueador | Quem |
|---|---|---|
| 1 | **Executar HW-01, HW-04, HW-05** no X669C (nenhum acesso nesta sessão) | você |
| 2 | **Revisão independente de `init/sec.c`** (autor = auditor) antes de qualquer merge do #13 | pessoa independente |
| 3 | Política do contador no #13 × `main` (recusar e subir com estado novo, ou limitar) | você |
| 4 | Política do swap acima de R$ 5.000 e teto por swap/diário (S01) | você |
| 5 | Rascunho S01: publicar ou não como issue | você |
| 6 | MAC do estado: vale o custo dado o ataque offline? Cifrar agora? PIN de 6 dígitos ou frase? | você |
| 7 | CI ARM64 (jobs A/B), versão fixada do `zig` | você |
| 8 | HW-07 (custo do scrypt no X669C) antes de escolher o custo definitivo | você + aparelho |

## 8. O que **não** foi feito (de propósito ou por falta de acesso)
Nenhum teste no aparelho; nada gravado; nenhuma issue publicada ou fechada; nenhum PR aberto, mesclado ou alterado; nenhum push em branch existente; nenhuma implementação de MAC, de relógio ou de CI; N12 (constantes DEMO) e as issues N01–N20 não foram publicadas.

## 9. Próxima ação recomendada
1. **Você** executa HW-01, HW-04, HW-05 pelo roteiro e arquiva a evidência.
2. Uma pessoa independente revisa o `sec.c` do #13 enquanto isso.
3. Depois, refazer a reconciliação **com a revisão em mãos** (merge da `main` nos dois branches), e só então decidir o #13/#15.
