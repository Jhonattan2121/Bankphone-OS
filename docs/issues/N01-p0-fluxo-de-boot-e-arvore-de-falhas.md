# [P0] Documentar o fluxo de boot e a árvore de falhas, e fechar a hipótese do watchdog de hardware

**Labels sugeridos:** `priority:P0`, `area:boot`, `type:research`, `area:hardware`
**Tipo:** investigação (não é bug confirmado)
**Precisa do Infinix Hot 30i:** sim
**EN:** Document the boot flow and failure tree on the X669C, and settle the hardware-watchdog hypothesis with evidence.

## Contexto
Tudo no projeto depende de o aparelho inicializar de forma repetível. Hoje não existe um documento que diga, etapa por etapa, o que roda entre o bootloader e o `main()` do PID 1, nem como distinguir uma falha do bootloader, do kernel, do ramdisk ou do `init`.

## Estado atual
- O PID 1 foi desenhado para **nunca reiniciar sozinho** e registrar tudo: `init/main.c:5-14`. Ele pinta o número do estágio (1 a 16) direto no framebuffer, sem fonte (`init/main.c:52-62`, `init/bootdiag.c` `bd_stage`).
- O relatório persistente e a serial existem (`bootdiag.c`), mas o relatório fica **desligado** na imagem somente leitura (`init/main.c:961-967`).
- O watchdog de hardware só é **listado** (`init/bootdiag.c:974-1000`); o PID 1 não o abre nem o alimenta.
- O README afirma que o sistema foi testado em um aparelho; as evidências estão em dev logs externos. **Não há log, foto ou captura arquivada no repositório** (auditoria: `docs/PROJECT_AUDIT.md`, seção 3).

## Problema
Sem uma árvore de falhas documentada e sem evidência arquivada, qualquer reinício espontâneo ou tela preta não pode ser atribuído a uma etapa. A hipótese H-1 (o watchdog armado pelo bootloader reinicia o aparelho porque ninguém o alimenta) não foi confirmada nem descartada.

## Objetivo
Um `docs/BOOT_FLOW.md` com o fluxo real e uma tabela "sintoma → etapa provável → como confirmar", mais a evidência que fecha ou documenta as hipóteses H-1, H-3 e H-6.

## Tarefas
- [ ] Documentar o fluxo: bootloader (LK) → kernel → ramdisk → `/init` → estágios 1..16, com o que cada estágio prova (usar `init/main.c` e `init/bootdiag.h`).
- [ ] Listar os artefatos que entram na imagem (kernel, DTB, ramdisk, cmdline) e de onde vêm (`tools/mkboot.py`, `tools/mkramdisk.py`, `scripts/pack-test-image.sh`).
- [ ] Definir o **procedimento de coleta**: foto do estágio, relatório persistente (modo não somente leitura, só depois do N06), serial, pstore.
- [ ] No aparelho: ler `/sys/class/watchdog/*/{state,timeout,identity}` no log de boot e registrar os valores.
- [ ] No aparelho: 10 boots a frio seguidos, anotando o estágio final e o tempo até qualquer reinício espontâneo.
- [ ] Para cada hipótese (H-1, H-3, H-6 da auditoria), registrar: confirmada, refutada ou ainda aberta, com a evidência.
- [ ] Arquivar a evidência em `docs/evidence/boot/` (texto e fotos), sem IMEI, número de série nem etiquetas.

## Critérios de aceite
- [ ] `docs/BOOT_FLOW.md` existe e cita arquivo e linha para cada etapa.
- [ ] A tabela de sintomas distingue bootloader, kernel, montagem do ramdisk e `init`.
- [ ] H-1 está classificada como confirmada, refutada ou aberta, **com a evidência arquivada**.
- [ ] Nenhuma conclusão sobre hardware se baseia só em teste de host.
- [ ] O procedimento de recuperação (N02) está documentado antes de qualquer gravação nova.

## Testes
- Host: nenhum teste novo é necessário; `make test` deve continuar verde.
- Hardware: os 10 boots a frio e a leitura do watchdog descritos acima.

## Riscos
- Gravar imagens no aparelho para coletar evidência. **Só usar a imagem somente leitura** e o `install.sh` (slot inativo), e só depois do N02.
- Fotos e logs podem expor identificadores (ver F-08 da auditoria). Revisar antes de arquivar.

## Dependências
- N02 (recuperação) antes de qualquer gravação.
- Relacionada a #8 (atualização assinada), mas não depende dela.

## Evidências
`init/main.c:5-14`, `init/main.c:52-62`, `init/main.c:961-967`, `init/bootdiag.c:974-1000`, `docs/PROJECT_AUDIT.md` (H-1, H-3, H-6).

## Fora do escopo
- Alterar o bootloader, partições ou o comportamento do PID 1.
- Implementar alimentação do watchdog (só decidir se é necessária; a mudança seria outra issue).

## Definição de concluído
Documento e evidências no repositório, hipóteses classificadas, nenhuma mudança de código do sistema.
