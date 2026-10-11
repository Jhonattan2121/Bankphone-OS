# [P1] Auditar e testar o contrato do PID 1: tratador de falha, ordem de inicialização e logs

**Labels sugeridos:** `priority:P1`, `area:boot`, `type:bug`
**Tipo:** defeito parcialmente confirmado (F-09) + documentação do contrato
**Precisa do Infinix Hot 30i:** não para a parte de código; sim para validar no aparelho
**EN:** Audit the PID 1 contract: the fatal-signal handler is not async-signal-safe, the init order is undocumented, and failure paths have no host tests.

## Contexto
O `init/main.c` é o PID 1. Se ele falhar mal, o aparelho fica preso sem diagnóstico, ou reinicia e esconde a causa.

## Estado atual
- Garantias declaradas: nunca reinicia sozinho, toda falha vai para tela, relatório e serial (`init/main.c:5-14`).
- O tratador de falha (`init/main.c:921-934`) chama `bd_flush()`, que usa `malloc` (`init/bootdiag.c:408`), dentro de um tratador de `SIGSEGV`/`SIGBUS`/`SIGILL`/`SIGFPE`/`SIGABRT`, e depois faz `for (;;) pause();`. **Lido no código.**
- O PID 1 faz `fork()` apenas para sondar `/proc` com tempo limite (`probe_proc_line`, `init/main.c:316-353`). Não há tratador de `SIGCHLD` (os filhos são recolhidos com `waitpid`).
- A ordem de inicialização (montagens → kmsg → relatório → pstore → inventário → hardware → USB → tela → backlight → fonte → UI → entrada → laço) existe só no código.
- Nenhum teste de host cobre o `main.c`.

## Problema
1. O tratador pode travar se a falha ocorrer dentro de `malloc`, perdendo justamente o log que deveria salvar.
2. O contrato (ordem, o que é fatal, o que é tolerado) não está documentado nem testado.
3. Não há política escrita para "o PID 1 ficar preso" (por desenho) e como o aparelho sai disso.

## Objetivo
Um tratador de falha que só use chamadas seguras para sinal, a ordem de inicialização documentada, e os caminhos de falha que puderem rodar no computador cobertos por testes.

## Tarefas
- [ ] Reescrever o tratador para usar só `write()` em buffer estático e `_exit`/`pause` (sem `malloc`, sem `snprintf` não seguro), e adiar a gravação do relatório para um caminho seguro.
- [ ] Documentar em `docs/BOOT_FLOW.md` (ou seção do N01) a ordem de inicialização e, para cada passo, se a falha é fatal, tolerada ou repetida.
- [ ] Extrair para funções testáveis no host o que não depende de hardware (por exemplo, a decisão de modo diagnóstico versus UI, e a política de retentativa).
- [ ] Testes de host com falhas simuladas: sem framebuffer, sem fonte, sem partição de relatório, sem `/sys/class/watchdog`. O resultado esperado é diagnóstico claro, nunca silêncio.
- [ ] Decidir e documentar a política de "preso por desenho": como o aparelho sai (teclas Vol-/Vol+ já existem) e se um tempo-limite de autorreinício opcional faz sentido (como opção, desligada por padrão).

## Critérios de aceite
- [ ] O tratador não chama `malloc`, `printf` nem `fsync`.
- [ ] Falhas simuladas em testes de host produzem mensagem de diagnóstico clara.
- [ ] A ordem de inicialização está documentada com arquivo e linha.
- [ ] Os testes não exigem privilégio nem tocam partições.

## Testes
- Host: testes novos para o que for extraído; `make test`, `make test-asan` e `make lint` verdes.
- Hardware: provocar um `SIGSEGV` controlado (build de teste) e conferir que o aviso aparece na tela e na serial.

## Riscos
- Mudar o PID 1 sem aparelho para validar. **Manter a mudança pequena**, só em imagem somente leitura, com recuperação (N02) pronta.
- Perder o relatório persistente no caso de falha: decidir conscientemente o que se grava no tratador.

## Dependências
- N01 (fluxo de boot) para a documentação; N02 antes de qualquer teste no aparelho.

## Evidências
`init/main.c:5-14`, `init/main.c:316-353`, `init/main.c:921-934`, `init/bootdiag.c:408`, `docs/PROJECT_AUDIT.md` (F-09).

## Fora do escopo
- Implementar alimentação do watchdog (decisão do N01).
- Trocar a política "nunca reinicia sozinho" por padrão.

## Definição de concluído
Tratador seguro, ordem documentada, testes de falha no host e verificação no aparelho registrada.
