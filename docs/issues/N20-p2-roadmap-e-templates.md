# [P2] Publicar o roadmap técnico e criar templates de issue e de pull request

**Labels sugeridos:** `priority:P2`, `area:documentation`, `type:feature`
**Tipo:** documentação/processo
**Precisa do Infinix Hot 30i:** não
**EN:** Publish the technical roadmap and add issue and pull-request templates.

## Contexto
O projeto é aberto e quer receber contribuições. Hoje não há roadmap público com marcos verificáveis, nem modelo para relatar um bug com os dados certos, nem lembrete de que um teste de hardware não foi feito.

## Estado atual
- `CONTRIBUTING.md` existe (bilíngue) com comandos de teste e regras do repositório.
- Não existem `ROADMAP.md`, `.github/ISSUE_TEMPLATE/` nem `.github/pull_request_template.md`.
- As issues #1 a #11 são de funcionalidades futuras; nenhuma tem rótulo de prioridade; o repositório usa rótulos como `enhancement`, `security`, `wallet`, `payments`, `testing`.

## Problema
Sem roadmap, o caminho "primeiro provar o boot, depois o resto" não está visível. Sem templates, relatos de bug chegam sem log, sem modelo de aparelho e sem dizer se foi testado em hardware.

## Objetivo
`ROADMAP.md` com marcos e critérios objetivos, templates que exigem evidência, e prioridade nas issues.

## Tarefas
- [ ] Criar `ROADMAP.md` (bilíngue) com os marcos: auditoria e build reproduzível; boot confiável e diagnóstico; display e toque; recuperação documentada; persistência testada; UI utilizável; demonstração financeira segura em DEMO; revisão de segurança; pesquisa de integração futura.
- [ ] Para cada marco: issues associadas, **critério de conclusão** e **estado real** (não iniciado, em andamento, feito com evidência).
- [ ] Template de **bug**: versão/commit, imagem usada (teste ou completa), modelo do aparelho, logs, passos, e uma pergunta obrigatória "foi testado em hardware real? sim/não".
- [ ] Template de **funcionalidade**: motivação, critérios de aceite, riscos, e o que **não** faz parte.
- [ ] Template de **PR**: testes executados (comandos e resultados), "teste em hardware: executado / não executado", checklist de segredos e de arquivos do aparelho.
- [ ] Atualizar `CONTRIBUTING.md` com como propor mudanças no boot ou nas imagens (só em imagem somente leitura, com plano de recuperação).
- [ ] Criar os rótulos `priority:P0` a `priority:P3` e as áreas usadas (`area:boot`, `area:hardware`, `area:ui`, `area:recovery`, `area:security`, `area:finance`, `area:storage`, `area:testing`, `area:documentation`), **sem duplicar** os rótulos que já existem.
- [ ] Aplicar prioridade às issues existentes (#1 a #11) conforme `docs/issues/README.md`.

## Critérios de aceite
- [ ] `ROADMAP.md` publicado e linkado no README.
- [ ] Templates aparecem ao abrir issue/PR.
- [ ] Todas as issues abertas têm um rótulo de prioridade.
- [ ] Nenhum rótulo redundante foi criado.

## Testes
Abrir uma issue e um PR de teste (e fechá-los) para conferir os templates.

## Riscos
Baixo. Manter o roadmap curto para não envelhecer; cada marco aponta para issues e evidências, não para promessas de data.

## Dependências
N13 (README) usa o roadmap.

## Evidências
`CONTRIBUTING.md`, lista de issues #1 a #11, `docs/IMPLEMENTATION_PLAN.md`.

## Fora do escopo
Prometer datas.

## Definição de concluído
Roadmap, templates, rótulos e prioridades aplicados.
