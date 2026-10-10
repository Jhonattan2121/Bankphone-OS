# BANKPHONE OS: roadmap técnico / technical roadmap

**Tudo é DEMO / TESTNET. / Everything is DEMO / TESTNET.** Não existe dinheiro real, Pix real nem BRL real.
No real money, no real Pix, no real BRL.

Este roadmap lista **marcos verificáveis**, não datas. Um marco só é "feito" quando o critério foi cumprido
**com evidência no repositório**. Compilar não conta como "inicializa no aparelho".
This roadmap lists **verifiable milestones**, not dates. A milestone is "done" only when its criterion is met
**with evidence in the repository**. A successful build does not mean "boots on the phone".

Estado em 2026-10-10, commit `86cda6f`. Detalhes e evidências: [`docs/PROJECT_AUDIT.md`](docs/PROJECT_AUDIT.md).
Ordem de execução: [`docs/IMPLEMENTATION_PLAN.md`](docs/IMPLEMENTATION_PLAN.md).
Issues propostas (ainda a publicar): [`docs/issues/`](docs/issues/).

## Legenda de estado

`FEITO` com evidência · `PARCIAL` · `NÃO INICIADO` · `NÃO VERIFICADO` (código existe, sem prova no aparelho)

## Marcos

| # | Marco | Critério de conclusão (objetivo) | Estado real | Issues |
|---|---|---|---|---|
| M1 | **Auditoria e build reproduzível** | `PROJECT_AUDIT.md` publicado; CI compila o binário ARM64; ferramentas de imagem testadas com fixtures; guia de build com hashes | **PARCIAL**: auditoria feita; CI de host verde; build ARM64 no CI **não existe** | N18, N16, #10 |
| M2 | **Boot confiável e diagnóstico** | 10 boots a frio seguidos com relatório e foto arquivados; fluxo de boot e árvore de falhas documentados; hipótese do watchdog fechada | **NÃO VERIFICADO**: código de diagnóstico existe; sem evidência arquivada | N01, N03, N06, N17 |
| M3 | **Display e toque funcionais** | Evidência (log + foto) de resolução, cores e coordenadas corretas; sem travar sem toque | **NÃO VERIFICADO**: lógica testada no host (28 e 21 checks); aparelho não | N08, N09 |
| M4 | **Recuperação documentada e testada** | `RECOVERY.md` com cada passo marcado `TESTADO`; fallback A/B provado ou refutado; `--restore` executado | **NÃO INICIADO**; README admite que o fallback não está provado | N02, N04, #8 |
| M5 | **Persistência testada** | `store` validado no aparelho; carregamento idempotente e validado; contador de tentativas gravado antes da conferência | **PARCIAL**: `store` testado no host (171 checks); aparelho não; contador gravado depois da conferência e só com a área armada | N15, #9 |
| M6 | **UI utilizável** | Ações da UI ou implementadas ou desabilitadas; estados de falha; captura de uma execução real | **PARCIAL**: UI e selo DEMO existem; sem evidência de uso real | N12, #4 |
| M7 | **Demonstração financeira segura em DEMO** | Invariantes documentadas e testadas; autenticação imposta pelo motor; DEMO centralizado e à prova de confusão | **PARCIAL**: motor com 40 checks e fuzzing; autenticação ainda vive na UI | N11, N07, N12 |
| M8 | **Revisão de segurança** | KDF forte mesclado; modelo de ameaças; salt seguro; revisão externa | **PARCIAL**: KDF no PR #13 (não mesclado); salt com fallback fraco | #2 (PR #13), N05, #3, #8 |
| M9 | **Pesquisa de integração futura** | Contratos e arquitetura documentados, sem dinheiro real | **NÃO INICIADO** (discussão aberta no #11) | #11, #1, #5, #6, #7 |

## O primeiro grande resultado

Não é movimentar dinheiro. É mostrar que **o sistema inicializa repetidamente no aparelho-alvo, oferece uma
interface utilizável, aceita entrada real, preserva seu estado, fornece diagnóstico e permite uma recuperação
documentada** (M2 a M5). Só depois disso faz sentido avançar para uma arquitetura financeira mais sofisticada e
para revisões de segurança independentes.

The first big result is not moving money. It is showing that **the system boots repeatedly on the target device,
offers a usable interface, accepts real input, keeps its state, provides diagnostics, and allows a documented
recovery** (M2 to M5).

## Fora do roteiro por enquanto / Not on the roadmap yet

Pix real; USDC/USDT reais; carteira de produção; alegações de segurança bancária; outros aparelhos; telefonia
e chamadas de emergência; atualização automática sem rollback validado; mudanças no bootloader.
Real Pix; real USDC/USDT; a production wallet; banking-grade security claims; other devices; telephony and
emergency calls; automatic updates without a validated rollback; bootloader changes.
