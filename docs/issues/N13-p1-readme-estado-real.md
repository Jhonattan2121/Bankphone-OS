# [P1] Atualizar o README com o estado real, níveis de maturidade e guia de contribuição

**Labels sugeridos:** `priority:P1`, `area:documentation`, `type:feature`
**Tipo:** documentação
**Precisa do Infinix Hot 30i:** não (mas depende de evidências das issues de hardware para marcar "verificado")
**EN:** Update the README with the real state, maturity levels and contribution guidance.

## Contexto
O README é honesto sobre DEMO/TESTNET e sobre o aparelho único, mas não diz, funcionalidade por funcionalidade, **o que foi visto no aparelho, o que só roda no computador, o que é simulado e o que falta**.

## Estado atual
- README bilíngue (inglês e português) com instalação, riscos e testes (`README.md`).
- Admite o que não foi verificado: comando serial, `fastboot fetch`, `adb pull`, fallback A/B (`README.md:49`, `:133`).
- Não há tabela de maturidade, nem explicação de "sistema nativo versus APK", nem diagrama de boot, nem link para recuperação, roadmap ou auditoria.
- Referência a ferramentas inexistentes está em scripts, não no README (ver N16).

## Problema
Alguém que nunca viu o projeto não consegue saber o que realmente funciona. A palavra "testado" mistura teste de computador com teste no aparelho.

## Objetivo
Um README que diferencia, para cada capacidade, **Implementado / Testado no host / Testado no hardware / Parcial / Planejado / Não verificado**, sem prometer além da evidência.

## Tarefas
- [ ] Seção "O que é" explicando sistema nativo em C sobre o kernel do aparelho, sem Android e sem APK, e o aparelho-alvo.
- [ ] **Tabela de maturidade** por capacidade (boot, display, toque, serial, persistência, PIN, motor, instalador, recuperação), com data e link para a evidência (`docs/evidence/...` ou `docs/PROJECT_AUDIT.md`).
- [ ] Seção "Arquitetura": boot e userland (diagrama curto), apontando para `docs/BOOT_FLOW.md` (N01).
- [ ] Seção "O que é simulado": tudo no modo DEMO, com a lista (N12).
- [ ] Seção "O que não existe": carteira, rede, chaves em hardware, verified boot, Pix real.
- [ ] Seção "Riscos conhecidos e limitações", incluindo o contador de PIN em memória e a ausência de TEE.
- [ ] Link para `docs/RECOVERY.md` (N02) **marcando o que está testado e o que é planejado**.
- [ ] Como compilar, rodar testes, ler a auditoria e contribuir (links para `CONTRIBUTING.md`, `ROADMAP.md`, templates).
- [ ] Não afirmar que é substituto do Android nem sistema bancário pronto.
- [ ] Manter as duas línguas coerentes.

## Critérios de aceite
- [ ] Cada linha da tabela de maturidade cita uma evidência ou diz `NÃO VERIFICADO`.
- [ ] Alguém sem contexto consegue entender o objetivo, rodar `make test` e ver o que falta.
- [ ] Nenhum termo como "seguro", "pronto" ou "banco" aparece sem a ressalva.

## Testes
Verificação de links (script simples no CI) e revisão por uma pessoa que não conhece o projeto.

## Riscos
O README envelhecer. Mitigação: a tabela aponta para arquivos de evidência com data, e o checklist de PR pede para atualizá-la.

## Dependências
Evidências de N01, N02, N08, N09 deixam as linhas "verificado em <data>". Sem elas, ficam `NÃO VERIFICADO`.

## Evidências
`README.md`, `docs/PROJECT_AUDIT.md`.

## Fora do escopo
Site do projeto (repositório privado) e dev logs.

## Definição de concluído
README e tabela de maturidade publicados, links funcionando.
