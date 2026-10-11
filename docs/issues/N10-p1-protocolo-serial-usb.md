# [P1] Especificar e versionar o protocolo do canal USB (serial) de diagnóstico e recuperação

**Labels sugeridos:** `priority:P1`, `area:recovery`, `area:hardware`, `type:feature`
**Tipo:** funcionalidade/documentação (o canal existe; o protocolo não está especificado)
**Precisa do Infinix Hot 30i:** parcialmente (para validar)
**EN:** Specify and version the USB serial diagnostic/recovery protocol.

## Contexto
O sistema expõe uma porta serial USB (gadget ACM). Hoje ela serve para duas coisas: despejar o relatório quando o host abre a porta, e aceitar um único comando para o instalador.

## Estado atual
- Gadget configurado via configfs em `init/main.c:103-126` (VID/PID de teste `1d6b:0104`, serial `BANKPHONE01`). **Sem teste de host.**
- Despejo do relatório quando o host levanta o DTR (`init/bootdiag.c`, `bd_serial_pump`).
- Único comando: linha exata `BANKPHONE:REBOOT-BOOTLOADER` (`init/devcmd.h`), só com `bankphone.devcmd=1`. Testado no host (16 checks + fuzzing com modelo de referência).
- O instalador envia o comando três vezes (`install.sh`, função `serial_to_fastboot`).
- Não há versão de protocolo, identificação de versão do sistema, nem resposta ao comando.
- README: o comando serial **não foi verificado** num X669C real.

## Problema
Sem especificação, qualquer evolução (mais comandos, identificação, diagnóstico) fica ambígua e arriscada, e o instalador não consegue saber com que versão está falando.

## Objetivo
Um `docs/SERIAL_PROTOCOL.md` versionado, com comandos de leitura/diagnóstico não destrutivos, respostas previsíveis e as regras de segurança, mais testes de host do leitor.

## Tarefas
- [ ] Especificar o enquadramento (linhas terminadas em `\n`, tamanho máximo, tratamento de `\r`, de NUL e de linhas longas).
- [ ] Especificar `HELLO`/`VERSION` (versão do protocolo e do sistema, modo: teste ou produção, somente leitura sim/não), com resposta de uma linha.
- [ ] Especificar `STATUS` (estágio, tela, toque, persistência), somente leitura.
- [ ] Manter `REBOOT-BOOTLOADER` apenas com `bankphone.devcmd=1`; **imagem de produção não aceita nenhum comando.**
- [ ] Nenhum comando de gravação, nenhum shell. Documentar o que fica explicitamente proibido.
- [ ] Fazer o leitor responder a cada comando (`OK`/`ERR <código>`), e o instalador usar `HELLO` antes de enviar o reboot.
- [ ] Documentar quando o canal **não** existe (antes do gadget, com falha de UDC, no bootloader).
- [ ] Testes de host: tabela de comandos válidos/inválidos, linha longa, NUL, fragmentação; estender o fuzzer.
- [ ] Validar no aparelho: tempo até a porta aparecer após o boot.

## Critérios de aceite
- [ ] Especificação publicada com versão.
- [ ] Todos os comandos têm teste de host e entram no fuzzer.
- [ ] Nenhum comando destrutivo ou de escrita existe.
- [ ] README informa em que fase do boot o canal está disponível, com evidência.

## Testes
Host: estender `tests/devcmd_test.c` e `tests/fuzz/fuzz_devcmd.c`. Hardware: abrir a porta e rodar `HELLO` e `STATUS`.

## Riscos
Aumentar a superfície de ataque. Por isso: só leitura, só em imagem de teste, entradas limitadas.

## Dependências
N01 (para informar a fase do boot). Independente de N08 e N09.

## Evidências
`init/main.c:103-126`, `init/devcmd.h`, `init/main.c:811-833`, `install.sh` (`serial_to_fastboot`), `tests/devcmd_test.c`, `tests/fuzz/fuzz_devcmd.c`.

## Fora do escopo
Recuperação a partir do bootloader (nesse momento o código do sistema ainda não executa), e qualquer protocolo de pagamento.

## Definição de concluído
Especificação, código e testes mesclados; validação no aparelho registrada.
