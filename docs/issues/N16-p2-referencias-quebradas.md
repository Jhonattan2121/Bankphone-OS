# [P2] Scripts e comentários apontam para ferramentas e caminhos que não existem no repositório

**Labels sugeridos:** `priority:P2`, `area:documentation`, `type:bug`, `good first issue`
**Tipo:** defeito de documentação confirmado (F-07), reproduzido com `grep`
**Precisa do Infinix Hot 30i:** não
**EN:** Scripts and comments point to tools and paths that do not exist in the repository.

## Contexto
O código foi reorganizado de `src/init/` para `init/`, e parte do fluxo usava ferramentas de uma máquina própria que não vieram para o repositório público. Quem lê os scripts não consegue seguir as instruções.

## Estado atual (todas as ocorrências, por `grep`)
- `bankphonectl ingest ... --deep` — `scripts/pack-test-image.sh:46`. A ferramenta **não existe** no repositório.
- `python3 -m bankphone_host.cli backup --partition <part> --dir <backups>` e "backup verificado do bankphonectl" — `scripts/state-region-hash.sh:24-25`. **Não existe** no repositório. O backup é pré-requisito de segurança do `store` (regra R3), então o caminho para obtê-lo precisa estar documentado.
- `src/init/store.c`, `src/init/store.h`, `src/init` — `scripts/state-region-hash.sh:7`, `:28`, `:66`, `:71`; `scripts/host-preview.sh:33-36`, `:67`; `init/host.c:229-233`, `:443`; `init/store.h:35`; `init/tests/test_bootdiag_fb.c:10-11`.
- `src/init/README-bootdiag.md` — `init/bootdiag.c:3`. Arquivo **inexistente**.
- Os comentários dizem que `scripts/state-region-hash.sh` e `store.c` têm que "chegar ao mesmo resultado": o script ainda procura os fontes em `src/init`, então a autoverificação (`--selftest`) pode não achar o código.

## Problema
1. Instruções que levam a comandos inexistentes.
2. O passo de backup, que protege o aparelho, não tem procedimento utilizável.
3. A autoverificação do `state-region-hash.sh` pode procurar no lugar errado.

## Objetivo
Toda referência aponta para algo que existe, e existe um procedimento de backup que qualquer pessoa consiga seguir.

## Tarefas
- [ ] Corrigir os caminhos `src/init` → `init` em scripts e comentários (manter a tolerância dos dois layouts só onde for útil, com comentário).
- [ ] Decidir sobre `bankphonectl`/`bankphone_host`: ou publicar a ferramenta, ou remover as menções e documentar o backup com comandos do `adb`/`fastboot`/`dd` já conhecidos, **marcando o que foi testado**.
- [ ] Escrever `docs/BACKUP.md` com o procedimento de backup da partição que o `store` vai usar e como obter o hash com `scripts/state-region-hash.sh`.
- [ ] Remover ou criar `init/README-bootdiag.md` (preferir um link para `docs/BOOT_FLOW.md`, do N01).
- [ ] Rodar `scripts/state-region-hash.sh --selftest` e `scripts/host-preview.sh` num clone limpo e registrar o resultado.
- [ ] Teste no CI: um `grep` que falha se reaparecer `src/init` ou `bankphonectl` fora de um arquivo de exceções documentado.

## Critérios de aceite
- [ ] `grep -rn "bankphonectl\|bankphone_host\|src/init" .` não encontra referências a algo inexistente.
- [ ] `--selftest` do script passa num clone limpo.
- [ ] `docs/BACKUP.md` existe e diz o que foi testado.

## Testes
Host: `scripts/state-region-hash.sh --selftest`; verificação por `grep` no `make lint` ou no CI.

## Riscos
Baixo. Cuidado para não apagar a tolerância de layout que o `host-preview.sh` usa.

## Dependências
Relacionada a N06 (o relatório também precisará de backup) e N01.

## Evidências
Listadas acima, linha a linha; `docs/PROJECT_AUDIT.md` (F-07).

## Fora do escopo
Publicar uma ferramenta nova de backup completa.

## Definição de concluído
Sem referências quebradas, procedimento de backup documentado, verificação no CI.
