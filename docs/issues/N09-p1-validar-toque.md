# [P1] Validar o caminho de toque no Infinix Hot 30i (nó, firmware, coordenadas), com evidência arquivada

**Labels sugeridos:** `priority:P1`, `area:hardware`, `area:ui`, `type:research`
**Tipo:** validação (a lógica existe e é testada no host; a prova no aparelho não está no repositório)
**Precisa do Infinix Hot 30i:** sim
**EN:** Validate the touch path on the X669C (node, firmware, coordinates) and archive the evidence.

## Contexto
O controlador Novatek NT36528 é *flashless*: precisa de firmware carregado pelo driver. O sistema tenta provocar esse carregamento (ciclo POWERDOWN→UNBLANK) e depois lê eventos.

## Estado atual
- Núcleo de eventos (`init/touchcore.h`) testado no host com 21 verificações (protocolos A e B, `SYN_DROPPED`, recorte de borda).
- Descoberta do nó por pontuação de capacidades, sem presumir `eventN` (`init/main.c:572-635`).
- "Kick" de firmware e leitura de `/proc/nvt_fw_version` com tempo limite (`init/main.c:316-444`).
- Autocura: até 3 repetições do kick se não houver eventos (`init/main.c:1172-1176`).
- O firmware vem de arquivos que cada pessoa extrai do próprio aparelho (`tools/get-touch-firmware.sh`); `adb pull` **não foi verificado** (README).
- Se não houver toque, a UI continua usável pelas teclas? **NÃO VERIFICADO.**

## Problema
Sem log e captura arquivados, não se sabe se o toque funciona, em qual nó, com qual versão de firmware, nem se as coordenadas batem com a tela.

## Objetivo
Evidência de ponta a ponta: firmware presente, nó escolhido, eventos recebidos, coordenadas corretas nos quatro cantos, e comportamento sem toque.

## Tarefas
- [ ] Rodar `tools/get-touch-firmware.sh` num Android de fábrica e registrar o resultado (tamanho esperado 139264 bytes).
- [ ] Capturar as linhas `input:`, `toque: candidato`, `toque: USANDO`, `fw_ver` e `FW-KICK` do log.
- [ ] Com `bankphone.touchbox=1`, tocar nos quatro cantos e no centro; fotografar e anotar `x`/`y` reportados.
- [ ] Comparar o ponto tocado com o ponto desenhado; registrar erro e rotação, se houver.
- [ ] Testar sem o arquivo de firmware: a UI não pode travar, e o log deve explicar.
- [ ] Testar o que acontece se o nó de toque sumir durante o uso (`read()` com erro).
- [ ] Documentar o resultado: funcional, parcial ou indisponível.
- [ ] Arquivar em `docs/evidence/touch/`.

## Critérios de aceite
- [ ] Toques convertidos em coordenadas corretas, com evidência.
- [ ] Ausência de toque não causa travamento nem laço de log infinito.
- [ ] README declara o toque como "funcional", "parcial" ou "indisponível", com data.
- [ ] Os testes de host continuam passando.

## Testes
Host: `init/tests/test_touchcore.c` (já existe). Hardware: os passos acima.

## Riscos
Baixo. O firmware do toque **não** deve ser comitado no repositório (regra do projeto).

## Dependências
N01 (coleta); depende de N08 só para saber a resolução.

## Evidências
`init/touchcore.h`, `init/main.c:263-700`, `init/main.c:1172-1176`, `tools/get-touch-firmware.sh`, `init/tests/test_touchcore.c`.

## Fora do escopo
Gestos além de toque e arrasto; múltiplos dedos.

## Definição de concluído
Evidência arquivada e README com o estado real do toque.
