# [P1] `tools/test_boot_b.sh` pode regravar o slot ativo e não pede confirmação

**Labels sugeridos:** `priority:P1`, `area:recovery`, `type:bug`, `good first issue`
**Tipo:** defeito confirmado (F-01), lido no código
**Precisa do Infinix Hot 30i:** não
**EN:** `tools/test_boot_b.sh` can overwrite the active slot and asks for no confirmation.

## Contexto
A regra de segurança do projeto é nunca gravar no slot em uso, porque ele é o caminho de volta. O instalador (`install.sh`) cumpre essa regra. Um script mais antigo não.

## Estado atual
`tools/test_boot_b.sh:10-14`:
- espera o fastboot com `until ... sleep 1` **sem limite de tempo**;
- imprime `current-slot`, mas **não confere** o valor;
- roda `fastboot flash boot_b "$IMG"` e `fastboot set_active b` **sem confirmação digitada**;
- `boot_b` é fixo no código.

O script não é citado no README, mas continua em `tools/`.

## Problema
Depois de uma primeira instalação, o slot `b` fica **ativo**. Rodar o script outra vez regrava o slot em uso e remove o caminho de volta.

## Objetivo
Ou remover o script (o `install.sh` o substitui), ou fazê-lo cumprir as mesmas regras do instalador.

## Tarefas
- [ ] Decidir: remover `tools/test_boot_b.sh` ou corrigi-lo.
- [ ] Se corrigir: ler o slot ativo, **recusar** gravar nele, escolher o slot inativo, aceitar `--dry-run`, validar que a imagem existe e não está vazia, exibir o sha256 e exigir confirmação digitada.
- [ ] Limite de tempo na espera do fastboot.
- [ ] Se remover: apagar o arquivo e conferir que nada o referencia (`grep -rn test_boot_b`).
- [ ] Teste de host com `fastboot` simulado (um script falso no `PATH`) que verifica que o script recusa gravar no slot ativo.

## Critérios de aceite
- [ ] Nenhum script do repositório grava num slot sem checar que ele é o inativo.
- [ ] Existe teste automatizado para essa recusa (ou o arquivo foi removido).
- [ ] O README não menciona o script como caminho suportado.

## Testes
Host: `fastboot` falso que responde `current-slot: b` e verifica que `flash boot_b` **não** é chamado.

## Riscos
Baixo. O risco é o de alguém ainda usar o script antigo em um fluxo pessoal; avisar na descrição do PR.

## Dependências
Nenhuma. É pré-requisito do experimento do N02.

## Evidências
`tools/test_boot_b.sh:10-14`, `install.sh:224` (comportamento correto para comparar).

## Fora do escopo
Mudar o `install.sh`.

## Definição de concluído
Script removido ou corrigido com teste, README coerente.
