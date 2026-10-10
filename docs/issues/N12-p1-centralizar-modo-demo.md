# [P1] Centralizar o modo DEMO e impedir que a demonstração se confunda com operação real

**Labels sugeridos:** `priority:P1`, `area:finance`, `area:ui`, `type:bug`
**Tipo:** defeito de manutenção confirmado (F-10) + proteção contra regressão
**Precisa do Infinix Hot 30i:** não
**EN:** Centralize the DEMO constants and add tests so the demo can never be mistaken for a real operation.

## Contexto
A interface já se declara DEMO em quase todo lugar. Mas os valores simulados estão espalhados, e nada **impede** uma regressão que faça um valor simulado parecer real.

## Estado atual
- Selo DEMO em todas as telas e no comprovante: `init/ui.c:166`, `init/screens.c:34`, `init/components.c:325`, `init/sheets.c` (`"Ambiente", "DEMO"`, `"Hash on-chain": "NENHUM (DEMO)"`).
- Texto honesto na tela de carteira: "NÃO IMPLEMENTADO" (`init/screens.c:218`).
- **Constantes duplicadas:** a cotação `1 USDC = R$ 5,60` está em `init/money.c:7` (`RATE 560`), `init/screens.c:39` (`/ 560`) e `:93`, e em texto fixo em `init/sheets.c:121`. A tarifa `R$ 3,50` está em `init/money.c:8` e no texto de `init/sheets.c:121`.
- Chave Pix e contatos fictícios em texto fixo (`init/sheets.c:48`; contatos em `init/ui.c:41`).
- O motor não faz nenhuma chamada de rede (não há código de rede no projeto hoje).

## Problema
1. Alterar a cotação num lugar deixa a tela mostrando um valor e o motor calculando outro.
2. Nada impede que, no futuro, um hash ou uma confirmação "real" apareça numa transação simulada.
3. Não há teste que prove que cada tela exibe o selo.

## Objetivo
Uma única fonte de verdade para os valores DEMO, e testes que quebrem se uma tela perder o selo, se uma transação simulada ganhar um hash, ou se o código de rede entrar no motor.

## Tarefas
- [ ] Criar `init/demo.h` com `DEMO_RATE_CENTS_PER_USDC`, `DEMO_FEE_CENTS`, rótulos e contatos; usar em `money.c`, `screens.c` e `sheets.c`.
- [ ] Gerar os textos "R$ 5,60" e "R$ 3,50" **a partir das constantes** (não literais).
- [ ] Teste: nenhuma ocorrência literal de `560`, `5,60` ou `3,50` fora de `demo.h` (verificação por `grep` dentro do `make test`).
- [ ] Teste: todas as transações criadas pelo motor têm `hash` vazio; o comprovante mostra `NENHUM (DEMO)`.
- [ ] Teste: renderizar cada tela com o `host.c` (capturando os textos desenhados via gancho de teste) e exigir o selo DEMO onde houver valor.
- [ ] Teste: `money.o` não referencia símbolos de rede (`socket`, `connect`, `send`) — checagem com `nm` no `make test`.
- [ ] Declarar a condição "nenhum dinheiro real" no README e nos textos da tela em um só lugar.

## Critérios de aceite
- [ ] Mudar `DEMO_RATE_...` muda motor e telas ao mesmo tempo.
- [ ] Os testes acima falham se alguém remover o selo, inserir um hash ou importar rede no motor.
- [ ] O README descreve o que é simulado e o que é real.

## Testes
Host: os cinco testes acima; `make test`, `make lint`.

## Riscos
Baixo. Cuidado com o texto longo de `sheets.c:121`, que mistura rótulo e valor.

## Dependências
Nenhuma. Combina com N11 (documento do motor) e N13 (README).

## Evidências
`init/money.c:7-8`, `init/screens.c:34-39`, `:93`, `:218`, `init/sheets.c:48`, `:121`, `:267`, `init/ui.c:166`, `docs/PROJECT_AUDIT.md` (F-10).

## Fora do escopo
Redesenhar as telas e qualquer integração real.

## Definição de concluído
Constantes centralizadas, testes de regressão mesclados, README atualizado.
