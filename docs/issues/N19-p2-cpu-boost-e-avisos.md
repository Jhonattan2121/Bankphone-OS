# [P2] Investigar o `cpu_boost` ligado por padrão e limpar os avisos de compilação do código do aparelho

**Labels sugeridos:** `priority:P2`, `area:hardware`, `type:research`
**Tipo:** investigação (H-4, **hipótese**) + avisos de compilação (F-11)
**Precisa do Infinix Hot 30i:** sim, para a parte de investigação
**EN:** Investigate the default-on `cpu_boost` and clean the device-code compiler warnings.

## Contexto
No início do boot o sistema força o desempenho máximo da CPU para a interface por software não parecer lenta. Isso muda temperatura e consumo, e fica ligado por padrão.

## Estado atual
- `cpu_boost()` (`init/main.c:721-742`) liga os núcleos 1 a 7, escreve `performance` no governador de cada política, copia `cpuinfo_max_freq` para `scaling_min_freq` e lê de volta o que o kernel aceitou. Pode ser desligado com `bankphone.cpuboost=0`.
- Não há medição de temperatura nem de consumo com e sem o boost no repositório.
- Compilação com `-O2 -Wall -Wextra` (auditoria): `init/main.c` tem **11** avisos de truncamento de `snprintf` (linhas 86, 88, 121, 731-736, 764) e `init/gfx.c` tem **4** de indentação enganosa. Os outros arquivos de `init/` têm 0.
- O `make lint` usa `-Wno-misleading-indentation`, portanto esses dois grupos não bloqueiam o CI.

## Problema
1. O efeito de ficar sempre no máximo é desconhecido (temperatura, bateria, estrangulamento térmico).
2. Truncar um nome de nó ou caminho pode apontar para o arquivo errado sem erro visível.

## Objetivo
Decidir com dados se o boost deve ser padrão, opcional ou dinâmico, e zerar os avisos de truncamento.

## Tarefas
- [ ] No aparelho: 15 minutos de uso contínuo da UI com `cpuboost` ligado e desligado, registrando temperatura (`hw`), bateria e as linhas `perf:` (média e pior quadro).
- [ ] Decidir o padrão com base nos números e documentar.
- [ ] Se continuar ligado: aplicar só enquanto a tela está acesa e restaurar o governador ao bloquear (avaliar).
- [ ] Corrigir os 11 avisos de truncamento: aumentar buffers ou checar o retorno de `snprintf` e registrar quando truncar.
- [ ] Corrigir os 4 avisos de indentação de `gfx.c`; considerar remover `-Wno-misleading-indentation` do lint depois.
- [ ] Testes: `make lint` sem o `-Wno-...` passa para os arquivos tratados.

## Critérios de aceite
- [ ] Decisão sobre o padrão registrada com medições.
- [ ] Zero avisos em `-O2 -Wall -Wextra` em `main.c` e `gfx.c`.
- [ ] `make test`, `make test-asan` e `make lint` verdes.

## Testes
Host: compilação sem avisos. Hardware: medições comparativas.

## Riscos
Mudar o padrão pode deixar a UI mais lenta. Medir antes de decidir.

## Dependências
N08 (display) fornece a linha `perf:` de referência.

## Evidências
`init/main.c:721-742`, `docs/PROJECT_AUDIT.md` (H-4, F-11).

## Fora do escopo
Gerenciamento de energia completo e suspensão.

## Definição de concluído
Medições arquivadas, decisão documentada e avisos zerados.
