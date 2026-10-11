# [P0] Provar o fallback A/B e escrever um guia de recuperação e rollback testado

**Labels sugeridos:** `priority:P0`, `area:recovery`, `area:boot`, `type:research`
**Tipo:** investigação + documentação
**Precisa do Infinix Hot 30i:** sim
**EN:** Prove (or disprove) the A/B fallback on the X669C and write a tested recovery and rollback guide.

## Contexto
A regra de ouro do projeto é "grave só no slot A/B inativo; o slot em uso é o caminho de volta". Essa regra só protege se, quando o slot novo não sobe, o aparelho volta ao antigo, ou se a pessoa sabe voltar à mão.

## Estado atual
- `install.sh` só grava `boot_<slot inativo>`, recusa o alvo igual ao original, exige a frase digitada e tem `--dry-run` e `--restore` (`install.sh:159`, `:214-229`).
- O README admite: "um slot que não sobe **não** volta sozinho garantidamente" e que o comando serial, o `fastboot fetch` e o `adb pull` **não foram verificados** no aparelho.
- Não existe um guia de recuperação passo a passo no repositório.
- `tools/test_boot_b.sh` (F-01) pode regravar o slot ativo e **não deve ser usado** até ser corrigido (N04).

## Problema
Sem prova do fallback e sem guia testado, uma imagem ruim pode deixar o aparelho sem caminho de volta, e o README não diferencia recuperação **testada** de **planejada**.

## Objetivo
Um `docs/RECOVERY.md` com o que foi **testado** e o que é só **planejado**, baseado em um experimento controlado do fallback A/B.

## Tarefas
- [ ] Documentar como identificar o estado do aparelho (Android, BANKPHONE, fastboot) e o slot ativo.
- [ ] Registrar o método **validado** de entrada no fastboot (tecla e comando serial) e o que cada um exige.
- [ ] Listar quais partições o projeto toca (apenas `boot_<slot inativo>`; `expdb` só na imagem não somente leitura) e quais **nunca** devem ser tocadas.
- [ ] Experimento A/B (com o aparelho em mãos e o Android original acessível): gravar uma imagem **inválida de propósito** só no slot inativo, ativá-lo e registrar o que o bootloader faz. Resultado possível: volta sozinho, ou fica preso.
- [ ] Documentar o procedimento para: falha no boot, perda de display, perda de acesso USB.
- [ ] Testar `./install.sh --restore` de verdade e registrar o resultado.
- [ ] Atualizar o README separando "recuperação testada" de "planejada".
- [ ] Se o bootloader **não** voltar sozinho, registrar isso como limite e abrir a issue de mitigação (contador de tentativas, junto do #8).

## Critérios de aceite
- [ ] Alguém segue o guia sem adivinhar comandos.
- [ ] Cada passo está marcado `TESTADO em <data>` ou `PLANEJADO`.
- [ ] O resultado do experimento A/B (positivo ou negativo) está arquivado em `docs/evidence/recovery/`.
- [ ] `--restore` foi executado de verdade e o resultado registrado.
- [ ] O guia diz o que fazer quando **não** é possível recuperar por software.

## Testes
- Host: `./install.sh --dry-run` com um aparelho simulado (já existe cobertura parcial no host; confirmar).
- Hardware: o experimento A/B e o `--restore`.

## Riscos
- O experimento pode deixar o aparelho sem boot. **Só fazer com o slot original intacto, com o firmware de fábrica à mão e sem dados importantes no aparelho.**
- Pode existir um comportamento específico do bootloader que torne o slot ativo irrecuperável sem ferramenta externa. Registrar antes de continuar.

## Dependências
- N04 (corrigir o script antigo) antes do experimento.
- Liga com #8 (atualização assinada com volta A/B): esta issue produz a evidência de que ele precisa.

## Evidências
`install.sh:159`, `install.sh:214-229`, README (trecho "Not verified yet" / "Ainda não verificado"), `tools/test_boot_b.sh:10-14`, `docs/PROJECT_AUDIT.md` (H-2).

## Fora do escopo
- Implementar o mecanismo de fallback por software (isso é #8).
- Mexer em `vbmeta`, `lk` ou `preloader`.

## Definição de concluído
Guia publicado, experimento arquivado, README honesto sobre o que foi e o que não foi testado.
