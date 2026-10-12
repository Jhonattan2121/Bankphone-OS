# Testes que dependem do Infinix Hot 30i X669C / Tests that need the phone

**Tudo é DEMO / TESTNET. / Everything is DEMO / TESTNET.**

**EN summary.** Nothing in this list has been verified in this review. CI, compilation and host tests prove that
the logic behaves as written on a PC, not that the system boots or works on the X669C. Each row says what to do,
what result is expected, what evidence must be filed, and the current state. **No row asks for an automatic
flash.** Anything that writes to the phone must be done by the maintainer, by hand, following `install.sh` and the
A/B rules (flash only the inactive slot, never touch the slot of the original system).

Estados / states: `NÃO VERIFICADO` (nada arquivado) · `RELATADO PELO AUTOR` (o autor diz que viu, sem evidência
no repositório) · `VERIFICADO` (log + foto/captura arquivados no repositório). Hoje **nenhuma linha é VERIFICADO**.

## Regras de evidência / Evidence rules

1. Evidência = arquivo reproduzível no repositório (log serial, captura, foto, hash da imagem usada, versão do
   commit). Texto "funcionou" não conta.
2. Anotar sempre: commit, hash SHA-256 da imagem gravada, slot usado, data, quem executou.
3. **Não gravar a partição `expdb` nem qualquer partição fora do slot inativo** em nenhum destes testes.
   Com a imagem somente leitura (`bankphone.ro=1`) o relatório não grava em partição.
4. Um teste que falha também é resultado: arquivar a falha.
5. Nunca anexar identificadores do aparelho (série, IMEI, MAC, cmdline completa) em issue pública; ver HW-09.

## Tabela / Table

| ID | O que provar | Procedimento (resumo) | Resultado esperado | Evidência necessária | Estado atual | Ligado a |
|---|---|---|---|---|---|---|
| HW-01 | Boot e estabilidade | Gravar a **imagem somente leitura** no slot **inativo**: `tools/test_boot_b.sh --dry-run` primeiro, depois a gravação com a frase de confirmação (**usar a versão da N04**; o script da `main` pode regravar o slot ativo). Ligar a frio 10 vezes seguidas, 5 min cada | O sistema sobe nas 10; relatório de boot completo; sem reinício inesperado | 10 logs seriais + fotos da tela; hash da imagem; commit | **NÃO VERIFICADO** (autor relata boot nos dev logs #1 e #2) | N01, M2 |
| HW-02 | Watchdog (H-1): o aparelho reinicia sozinho ou o PID 1 segura? | Deixar o sistema ligado 30 min sem toque; depois bloqueado; ler o log serial | Sem reinício espontâneo, ou, se houver, o intervalo exato registrado | Log serial contínuo com marcas de tempo; nota do que causou o reinício | **NÃO VERIFICADO** (hipótese H-1) | N01 |
| HW-03 | Fallback A/B (H-2): se o slot novo não sobe, o aparelho volta ao original? | Com a imagem de teste no slot inativo e o slot original intacto: provocar falha de boot controlada e ver para qual slot o bootloader volta; depois voltar ao original com o procedimento de recuperação | Volta ao slot original sozinho, **ou** a falha é documentada e o procedimento manual de volta é passo a passo comprovado | Logs do fastboot (`current-slot`, `slot-retry-count`/`slot-successful` se existirem); fotos; passo a passo executado | **NÃO VERIFICADO**; o README admite que **não foi provado** | N02, #8 |
| HW-04 | Display | Boot; capturar as barras de teste de cor e a tela inicial | Resolução, ordem de canais (BGR) e cores corretas | Foto da tela + log do `bd_fb_*` com a resolução detectada | **NÃO VERIFICADO**; desenho testado só no host (28 verificações) | N08, M3 |
| HW-05 | Toque | Tocar os quatro cantos e o centro; arrastar; soltar; ficar 60 s sem tocar | Coordenadas coerentes; sem travar; sem toque fantasma | Log dos eventos com coordenadas + vídeo curto ou foto sequencial | **NÃO VERIFICADO**; núcleo testado só no host (21 verificações) | N09, M3 |
| HW-06 | Protocolo serial/USB e recuperação | Abrir a serial CDC-ACM no computador; enviar `REBOOT-BOOTLOADER` (e entradas inválidas); medir se o aparelho responde | Reinicia no bootloader só com o comando exato; entrada inválida é ignorada; recuperação USB documentada funciona | Log do host e do aparelho; comando usado; resultado | **PARCIAL, RELATADO PELO AUTOR** (README, commit `c2bd14d`): com uma imagem de teste rodando num X669C real, o `./install.sh` mandou o comando pela USB e o aparelho reiniciou no fastboot sozinho; o `fastboot fetch` foi **recusado** pelo bootloader. **Sem log arquivado** no repositório, então não vale como nível C. Entradas inválidas, protocolo completo e recuperação seguem **NÃO VERIFICADOS**; o parser tem 38 verificações + fuzz no host | N10, N02 |
| HW-07 | **Tempo e memória do scrypt no X669C** | Compilar `tools/bench_kdf.c` para ARM64 e rodar no aparelho (ou medir a tela de desbloqueio com cronômetro de log); repetir 20 vezes; anotar o pior caso | Número real por tentativa; decisão do custo padrão com base nele; tela de desbloqueio aceitável | Saída do benchmark no aparelho + log; versão e hash do binário | **NÃO VERIFICADO**; só há medição no container de CI (113,8 ms para o padrão) | #2, PR #13 |
| HW-08 | Integridade do estado após reinício e falha de energia | Com a área de estado **armada** (hash do backup da região verificado): criar PIN, fazer transações DEMO, reiniciar; depois tirar a bateria/cabo durante uma gravação (várias vezes) | Estado igual após o reinício; nunca um estado parcial; o slot anterior vale se a queda ocorrer no meio | Log do `store` (slot, seq, recuperado ou não); `state-region-hash` antes/depois; **backup da `expdb` feito antes** | **NÃO VERIFICADO**; o `store` tem 171 verificações no host, inclusive queda em cada passo | N15, #9 |
| HW-09 | Identificadores no relatório de boot (F-08) | Capturar um relatório real e procurar série, IMEI, MAC, nomes de nó ou outros identificadores; ver a cmdline completa | Lista do que aparece; decisão do que filtrar | Relatório **com os identificadores borrados** + lista dos campos que continham algo | **NÃO VERIFICADO** | N17 |
| HW-10 | Onde o relatório grava (F-05) | Conferir, com a imagem **não** somente leitura num aparelho de teste com backup, o que o `expdb` contém na região usada (`BD_BASE` = 8 MiB) e se o relatório sobrescreve algo útil | Resposta documentada; padrão de gravação seguro definido | Hash da região antes/depois; backup | **NÃO VERIFICADO**; só leitura de código | N06 |
| HW-11 | Persistência do contador de erros do PIN | Errar o PIN 5 vezes, reiniciar, conferir se o bloqueio continua; repetir cortando a energia durante a conferência | Descrito no modelo de ameaças: contador gravado **depois** da conferência e só com a área armada | Logs; fotos | **NÃO VERIFICADO**; comportamento só lido no código | #9, PR #13 |
| HW-12 | `cpu_boost` e temperatura (H-4) | Medir temperatura/consumo com e sem `cpu_boost` por 30 min | Decisão documentada do padrão | Log de temperatura | **NÃO VERIFICADO** | N19 |
| HW-13 | `getrandom()` cedo no boot (PR #15) | Ver quanto a criação do PIN espera `getrandom()` no primeiro boot; ver se bloqueia | Sem bloqueio perceptível, ou o tempo exato registrado | Log com marcas de tempo | **NÃO VERIFICADO**; não observado no aparelho | PR #15, N05 |

| HW-14 | Comandos `SCREENSHOT` e `TOUCH` por USB (commit `f80968d`, só imagens de teste com `bankphone.devcmd=1`) | Rodar `tools/screenshot.py` com uma imagem de teste; conferir que a captura tem o marcador de fim e bate com a tela | PNG igual à tela; quadro sem marcador de fim é recusado | PNG + log do host + comando usado | **NÃO VERIFICADO**; o commit só declara testes no computador (parser com 38 verificações) | N10 |

## Linhas adicionadas na rodada 3 / Rows added in round 3

Todas **NÃO VERIFICADO**. Nenhuma pede gravação automática. O roteiro de execução de HW-01, HW-04 e HW-05 está em `HW_PLAN_R4.md`.

| ID | O que provar | Procedimento (resumo) | Resultado esperado | Evidência necessária | Ligado a |
|---|---|---|---|---|---|
| HW-15 | Fonte do relógio e salto entre domínios (relógio real × uptime) | Ligar sem rede, anotar `plat_clock_valid()` e `plat_now()` por 10 min; depois com o relógio acertado; reiniciar | Registrar a transição e a ordem de grandeza (≈ 1,7e9 contra segundos de uptime) | Log serial com marcas | O-2, O-3, `fix/pin-lockout-bounded-clock` |
| HW-16 | Bloqueio do PIN ao reiniciar durante o bloqueio | Errar o PIN 5 vezes, reiniciar em seguida (com e sem relógio válido); medir quanto falta | O bloqueio nunca passa de 1920 s e acaba sozinho | Log + foto da contagem na tela | `fix/pin-lockout-bounded-clock`, HW-11 |
| HW-17 | Contagem do bloqueio na tela de PIN | Provocar o bloqueio na tela de autorização de swap e na tela de desbloqueio | A contagem decresce na folha de PIN, não só no desbloqueio | Fotos em 3 instantes | `fix/ui-input-and-display` |
| HW-18 | Tabela de toques com um livro-razão longo | Carregar um estado de teste com 300+ linhas (cópia, nunca o estado real) e abrir a Atividade | A barra de abas e a engrenagem continuam respondendo ao toque | Vídeo curto ou fotos | `fix/ui-input-and-display` |
| HW-19 | Leitura de `/proc/cmdline` maior que o buffer | Imagem de teste com cmdline longa (>4 KiB) e `bankphone.ro=1` no fim | A imagem se comporta como somente leitura (falha fechada) | Log do boot + hash da região de estado antes/depois | `fix/cmdline-truncation-fails-closed` |

| HW-20 | Arquitetura do espaço de usuário no aparelho | Com o sistema rodando (serial), registrar `uname -m` equivalente e o tamanho de `time_t`; conferir que o binário `aarch64-linux-musl` estático do `build.sh` executa | 64 bits, `time_t` de 64 bits | Log serial com a informação | **NÃO VERIFICADO** (o `build.sh` assume; só há compilação e emulação, nível B) | `HW_PLAN_R4.md`, `ISSUE9_CLOCK_AND_ARM64_CI.md` |
| HW-21 | Existe RPMB acessível (contador monotônico em hardware)? | **Somente leitura** de capacidade do eMMC, sem Android. **Nunca** programar a chave da RPMB (irreversível) | Resposta documentada (sim/não) e como se acessa | Saída do comando e hash do relatório | **NÃO VERIFICADO**; só leitura de código e de documentação | `docs/STATE_INTEGRITY.md` §9 |

Ordem: HW-15 antes de HW-16, que depende do domínio do relógio medido.

## O que **não** é teste de aparelho / What does not need the phone

Os itens abaixo se verificam no computador e já têm testes de host: lógica do motor financeiro, parser da
cmdline, validação do estado, script `test_boot_b.sh` com `fastboot` falso, cálculo do store. Eles **não** substituem
as linhas acima.

## Ordem sugerida / Suggested order

HW-01 → HW-04 → HW-05 → HW-02 → HW-07 → HW-09 → HW-08 → HW-03 → HW-06 → o resto. A primeira meta é provar que o
sistema **inicia, mostra tela, recebe toque, recupera-se de falhas e guarda o estado**; só depois fazem sentido
novas funções financeiras.
