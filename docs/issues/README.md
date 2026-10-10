# Issues propostas pela auditoria (prontas para publicar)

Cada arquivo desta pasta é o título e o corpo completo de uma issue, no formato exigido pelo plano:
Contexto, Estado atual, Problema, Objetivo, Tarefas, Critérios de aceite, Testes, Riscos, Dependências,
Evidências, Fora do escopo e Definição de concluído. Os números de linha referem-se ao commit `86cda6f` (`main`).

**Nenhuma destas issues foi publicada no GitHub.** Elas são arquivos Markdown. Quem publicar copia o título
(primeira linha) e o corpo, e aplica os rótulos sugeridos.

## Como ler a prioridade

| Prioridade | Significa |
|---|---|
| **P0** | Bloqueia inicialização, recuperação ou integridade, ou coloca o aparelho em risco |
| **P1** | Necessária para uma demonstração confiável no aparelho real |
| **P2** | Qualidade, manutenção e segurança do protótipo |
| **P3** | Evolução futura; não bloqueia a estabilidade básica |

## Novas issues

| ID | Prioridade | Tipo | Título | Aparelho? |
|---|---|---|---|---|
| [N01](N01-p0-fluxo-de-boot-e-arvore-de-falhas.md) | P0 | investigação | Fluxo de boot, árvore de falhas e hipótese do watchdog | sim |
| [N02](N02-p0-recuperacao-e-fallback-ab.md) | P0 | investigação + docs | Provar o fallback A/B e guia de recuperação testado | sim |
| [N03](N03-p1-contrato-do-pid1.md) | P1 | bug (F-09) + docs | Contrato do PID 1: tratador de falha, ordem, logs | não (validar depois) |
| [N04](N04-p1-script-antigo-pode-regravar-slot-ativo.md) | P1 | **bug confirmado** (F-01) | `test_boot_b.sh` pode regravar o slot ativo | não |
| [N05](N05-p1-aleatoriedade-do-salt.md) | P1 | **bug confirmado** (F-04) | `plat_random` com fallback previsível | não |
| [N06](N06-p1-relatorio-em-expdb.md) | P1 | risco confirmado (F-05) | Relatório grava em `expdb` por padrão | parcial |
| [N07](N07-p1-fronteira-de-autenticacao-no-motor.md) | P1 | risco confirmado (F-06) | Motor aceita o nível de autenticação do chamador | não |
| [N08](N08-p1-validar-display.md) | P1 | validação | Display no X669C com evidência | sim |
| [N09](N09-p1-validar-toque.md) | P1 | validação | Toque no X669C com evidência | sim |
| [N10](N10-p1-protocolo-serial-usb.md) | P1 | feature + docs | Protocolo serial/USB especificado e versionado | parcial |
| [N11](N11-p1-invariantes-do-motor-financeiro.md) | P1 | especificação + testes | Invariantes, arredondamento e limites do motor | não |
| [N12](N12-p1-centralizar-modo-demo.md) | P1 | **bug confirmado** (F-10) | Centralizar o modo DEMO e testes de regressão | não |
| [N13](N13-p1-readme-estado-real.md) | P1 | docs | README com estado real e níveis de maturidade | não |
| [N14](N14-p2-parser-da-cmdline.md) | P2 | **bug reproduzido** (F-02) | `plat_boot_prop` depende da ordem e do prefixo | não |
| [N15](N15-p2-carregamento-e-esquema-do-estado.md) | P2 | **bug reproduzido** (F-03) + lacunas | Carregamento idempotente e esquema validado | não |
| [N16](N16-p2-referencias-quebradas.md) | P2 | **bug confirmado** (F-07) | Referências a ferramentas/caminhos inexistentes | não |
| [N17](N17-p2-logs-padronizados-e-sem-identificadores.md) | P2 | feature + risco (F-08) | Logs padronizados e sem identificadores | parcial |
| [N18](N18-p2-build-reproduzivel-e-ci-do-aparelho.md) | P2 | engenharia | Build reproduzível e CI com ARM64 | não |
| [N19](N19-p2-cpu-boost-e-avisos.md) | P2 | investigação (H-4) + avisos | `cpu_boost` padrão e avisos de compilação | sim |
| [N20](N20-p2-roadmap-e-templates.md) | P2 | docs/processo | Roadmap, templates e rótulos | não |

"Bug confirmado/reproduzido" significa que foi lido no código ou reproduzido por experimento nesta
auditoria (veja `docs/PROJECT_AUDIT.md`, seção 5). Itens marcados "investigação" ou "validação" **não** são
bugs confirmados: são hipóteses ou verificações que dependem do aparelho.

## Issues existentes reaproveitadas (nenhuma duplicata aberta)

| Pedido do plano | Issue existente | O que fazer |
|---|---|---|
| Revisar a KDF do PIN e a proteção contra tentativas | [#2](https://github.com/Jhonattan2121/Bankphone-OS/issues/2) e PR #13 | Mesclar o PR #13. O contador persistente de tentativas fica no #9. |
| Modelo de ameaças | [#2](https://github.com/Jhonattan2121/Bankphone-OS/issues/2); `docs/THREAT_MODEL.md` no PR #13 | Estender depois do PR #13 (superfície USB, atualização, rollback). |
| Estado cifrado, rollback, relógio monotônico | [#9](https://github.com/Jhonattan2121/Bankphone-OS/issues/9) | Reaproveitar. Auditoria do **carregamento** é a N15. |
| CI, sanitizers, fuzzing | [#10](https://github.com/Jhonattan2121/Bankphone-OS/issues/10) (≈70% feita) | Reaproveitar. CI do aparelho (ARM64) é a N18. |
| Atualizações assinadas e rollback A/B | [#8](https://github.com/Jhonattan2121/Bankphone-OS/issues/8) | Reaproveitar. **Prova** do fallback é a N02. |
| Tela de confirmação confiável | [#4](https://github.com/Jhonattan2121/Bankphone-OS/issues/4) | Reaproveitar. Fronteira de autenticação no motor é a N07. |
| Arquitetura de integração financeira futura | [#11](https://github.com/Jhonattan2121/Bankphone-OS/issues/11) | Reaproveitar (é a discussão de arquitetura, P3). |
| Carteira, PIN de coação, P2P, recibos, cofre | [#1](https://github.com/Jhonattan2121/Bankphone-OS/issues/1), #3, #5, #6, #7 | Reaproveitar, prioridade **P3**: o plano diz para não priorizar agora. |

## Prioridade sugerida para as issues já existentes

| Issue | Prioridade |
|---|---|
| #2 (KDF do PIN, PR #13 aberto) | P1 |
| #9 (estado cifrado e rollback) | P2 |
| #10 (CI e fuzzing, resto) | P2 |
| #8 (atualização assinada) | P2 |
| #4 (tela de confirmação) | P2 |
| #1, #3, #5, #6, #7, #11 | P3 |

## Rótulos

O repositório hoje usa rótulos como `enhancement`, `security`, `wallet`, `payments`, `testing`, `storage`,
`design`, `documentation`. Os rótulos `priority:P0..P3` e `area:*` ainda **não existem**: criar uma vez,
sem duplicar os que já existem (`security` ≈ `area:security`, `testing` ≈ `area:testing`,
`storage` ≈ `area:storage`, `documentation` ≈ `area:documentation`; usar estes em vez de criar equivalentes).

## Dependências entre as novas issues

```
N04 ──► N02 ──► N01
         │       └─► N08, N09, N10, N17 (coleta de evidência)
N14, N15 ──► N11
N07 ──► (#4)
N05, N14, N04, N12, N16, N19 (avisos) : independentes entre si
N13 e N20 usam o resultado das demais
```
