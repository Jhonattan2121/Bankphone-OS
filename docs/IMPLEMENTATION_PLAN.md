# BANKPHONE OS: plano de implementação (Fase C)

Depende de `docs/PROJECT_AUDIT.md` e das issues em `docs/issues/`. **Tudo é DEMO / TESTNET.**
Este plano ordena o trabalho para que **cada passo seja pequeno, revisável e verificável**, e para que **nada
que arrisque o aparelho aconteça antes de existir um caminho de volta**.

## 1. Princípios

1. Evidência antes de mudança: se a causa é desconhecida, o primeiro passo é medir (N01), não editar.
2. Nenhuma gravação no aparelho antes de N04 (script perigoso corrigido) e N02 (recuperação documentada).
3. Só imagem **somente leitura** (`bankphone.ro=1`) nos primeiros testes no aparelho.
4. Um PR por issue, com teste. Compilar não é prova de que inicializa.
5. Cada mudança no PID 1, no `store` ou no PIN passa por `make test`, `make test-asan`, `make lint` e `make fuzz`.
6. Nenhum arquivo do aparelho (kernel, firmware, imagens) entra no repositório.

## 2. Primeira issue recomendada: **N04** (`tools/test_boot_b.sh`)

**Por quê, tecnicamente**
- É um defeito **confirmado** no código (F-01) cuja consequência é a pior do projeto: remover o caminho de volta do aparelho.
- É **pré-requisito** do experimento A/B (N02): ninguém deve provocar uma gravação de teste enquanto esse script existir da forma atual.
- Não toca o aparelho, o PID 1, nem o dinheiro. O risco de regressão é praticamente zero.
- É pequena: remover um arquivo, ou adicionar quatro guardas e um teste com `fastboot` falso.

**Arquivos:** `tools/test_boot_b.sh` (remover ou corrigir); novo teste em `tests/` (script de teste com `fastboot` simulado no `PATH`); `Makefile` (ligar o teste); `CONTRIBUTING.md` se o script permanecer.

**Testes:** `fastboot` falso que responde `current-slot: b`; o teste exige que `flash boot_b` **não** seja chamado. Mais: arquivo de imagem ausente e vazio recusados; sem confirmação, nada é gravado.

**Risco:** baixo. **Rollback:** `git revert` do commit.

**Alternativa considerada:** começar por N14 (cmdline) ou N05 (salt). São mais ricas em código, mas protegem menos: N04 elimina um caminho que perde o aparelho.

## 3. Ordem de execução

| # | Issue | Por quê nessa posição | Arquivos principais | Aparelho? | Risco |
|---|---|---|---|---|---|
| 0 | **PR #13** (scrypt, #2) | Já pronto; muda `sec.c`. Mesclar antes de mexer em autenticação para evitar conflito. | `init/sec.c`, `init/sec.h`, `init/ui.c` | não | baixo |
| 1 | **N04** | Pré-requisito de qualquer gravação. | `tools/test_boot_b.sh` | não | ~0 |
| 2 | **N05** | Falha silenciosa de segurança, pequena. | `init/main.c`, `init/ui.c` | não | baixo |
| 3 | **N14** | Trava `bankphone.ro` depende de leitura frouxa. | `init/main.c` (+ novo header) | não | baixo |
| 4 | **N15** | Carregamento idempotente e validado; precede o motor. | `init/main.c`, `init/store.c`, `init/money.c` | não | médio (formato) |
| 5 | **N12** | Constantes DEMO centralizadas; testes de regressão. | `init/demo.h` (novo), `money.c`, `screens.c`, `sheets.c` | não | baixo |
| 6 | **N11** | Especificar e testar as invariantes. | `docs/MONEY_ENGINE.md`, `init/money.c`, `tests/` | não | médio |
| 7 | **N07** | Fronteira de autenticação (depende do PR #13). | `init/money.c`, `init/sec.c`, `init/ui.c` | não | médio |
| 8 | **N18** | CI compila ARM64; ferramentas testadas com fixtures. | `.github/workflows/ci.yml`, `tools/*.py`, `build.sh` | não | baixo |
| 9 | **N16** | Remover referências quebradas; escrever o backup. | scripts, `docs/BACKUP.md` | não | baixo |
| 10 | **N03** | Tratador de falha seguro (precisa de teste no aparelho depois). | `init/main.c` | validar depois | médio |
| 11 | **N10** | Especificar o protocolo (parte de host primeiro). | `docs/SERIAL_PROTOCOL.md`, `init/devcmd.h` | validar depois | baixo |
| 12 | **N06** | Relatório com disciplina do `store`. | `init/bootdiag.c`, `init/main.c` | parcial | médio |
| 13 | **N13**, **N20** | Documentação e processo; usam o resultado dos anteriores. | `README.md`, `ROADMAP.md`, `.github/` | não | ~0 |
| — | **Sessão de hardware** (abaixo) | Reúne tudo o que exige o aparelho. | `docs/evidence/`, `docs/RECOVERY.md`, `docs/BOOT_FLOW.md` | **sim** | alto se mal feito |
| — | **N17**, **N19** | Dependem de relatório real e de medições. | `init/bootdiag.c`, `init/main.c` | sim | baixo |

O passo 0 depende do aceite do autor; os passos 1 a 9 podem ser feitos **sem o aparelho**. Os passos
posteriores melhoram com os dados da sessão de hardware.

## 4. Sessão de hardware (quando o aparelho estiver disponível)

**Pré-requisitos (todos obrigatórios)**
- [ ] N04 resolvido (nenhum script antigo grava no slot ativo).
- [ ] `docs/RECOVERY.md` em versão "planejado" revisado, com o firmware de fábrica do modelo **baixado e verificado antes**.
- [ ] Backup das partições que o projeto toca (`scripts/state-region-hash.sh` e `docs/BACKUP.md`, N16).
- [ ] Aparelho sem dados importantes, bateria carregada, cabo confiável.
- [ ] Imagem **somente leitura** gerada, com sha256 anotado.

**Ordem dos experimentos (parar a qualquer sinal de problema)**
1. Identificar o aparelho e o estado (modelo, slot ativo, bootloader). Nada gravado.
2. `./install.sh --dry-run`. Atenção: ele reinicia o aparelho no bootloader.
3. `fastboot boot work/bankphone-os-ro.img` (RAM, **sem gravar**), se o bootloader aceitar. Fotografar o estágio, ler o relatório e os logs do watchdog (N01, H-1).
4. Display e toque (N08, N09) com evidência arquivada.
5. Protocolo serial: `HELLO`/`STATUS` e o reboot (N10).
6. **Só depois:** gravação no slot inativo com `install.sh`, 10 boots a frio, e o experimento A/B controlado (N02), com o plano de volta aberto na tela.

**Condições de parada:** qualquer comportamento de boot inesperado após uma gravação, perda de acesso ao fastboot, ou impossibilidade de voltar ao slot original. Nesse caso, registrar e **não** insistir; seguir o guia de recuperação.

## 5. Estratégia de rollback por tipo de mudança

| Mudança | Como voltar |
|---|---|
| Código de host (scripts, testes, docs) | `git revert` do commit |
| Mudança no PID 1 / `store` / PIN | `git revert`; em aparelho, `./install.sh --restore` (volta ao slot original) |
| Imagem no slot inativo | `./install.sh --restore`; ou `fastboot set_active <slot original>` |
| Formato de estado | migração aceita o formato antigo; manter leitura retrocompatível por pelo menos uma versão |

## 6. O que não será feito agora

Pix real; movimentação de USDC/USDT reais; carteira de produção; chaves de produção; alegações de segurança
bancária; compatibilidade com outros aparelhos; substituir telefonia e chamadas de emergência;
atualização automática de firmware sem rollback validado; mexer em bootloader ou em novas partições sem
necessidade demonstrada. Os itens de funcionalidade futura (#1, #3, #5, #6, #7, #11) ficam em P3.
