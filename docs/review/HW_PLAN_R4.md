# Plano de execução no aparelho: HW-01, HW-04 e HW-05 / Phone run plan

**Tudo é DEMO / TESTNET. / Everything is DEMO / TESTNET.** Plano, **não resultado**. Esta sessão não tem acesso ao Infinix Hot 30i X669C (sem USB, `adb`, `fastboot` nem porta serial), então
**HW-01, HW-04 e HW-05 continuam NÃO VERIFICADO**. Este documento diz exatamente o que fazer, o que arquivar e quando parar. **Nada aqui grava no aparelho sem você executar.**

**EN summary.** A step-by-step plan for the three phone tests that matter most, with the evidence to file for each, the stop conditions, and two traps in the existing tooling:
`tools/screenshot.py` captures the **frame buffer in memory** (not the panel), and its `--tap` **injects** a touch over USB (it does not exercise the touch screen). So HW-04 and HW-05 need
a photo of the real panel and real finger taps, respectively. Nothing is flashed automatically.

## 0. Portões de segurança (todos precisam ser "sim" antes de gravar qualquer coisa)

| # | Pergunta | Como saber | Se "não" |
|---|---|---|---|
| G1 | O aparelho é o X669C de teste, com bootloader desbloqueado, e você tem o procedimento de volta ao Android? | `fastboot getvar product`; README e `docs/review/HARDWARE_TESTS.md` (HW-03) | **Parar.** Sem caminho de volta comprovado, não gravar |
| G2 | Você tem **backup** de `boot_a`, `boot_b` e `expdb` fora do repositório? | arquivos e hashes no seu computador | Fazer o backup primeiro (somente leitura) |
| G3 | A imagem a gravar é a **somente leitura** (`bankphone.ro=1`)? | `strings` na cmdline do cabeçalho / `tools/mkboot.py unpack` | **Parar.** Não gravar imagem com escrita de estado nestes testes |
| G4 | O script usado é o da `main` atual (N04: nunca grava o slot ativo)? | `git log -1 -- tools/test_boot_b.sh` ≥ PR #20 | Atualizar |
| G5 | `tools/test_boot_b.sh --dry-run` mostra slot ativo, alvo diferente do ativo e nenhuma gravação? | saída do `--dry-run` | **Parar** |
| G6 | O SHA-256 da imagem está anotado antes da gravação? | `shasum -a 256 work/bankphone-os-ro.img` | Anotar |

**Nunca:** gravar `expdb`, gravar o slot ativo, usar a imagem que escreve estado, rodar `fastboot flash` à mão, anexar série/IMEI/MAC/cmdline completa em issue pública (HW-09).

## 1. Preparação (no computador, sem tocar no aparelho)

1. `git rev-parse HEAD` e `git status` limpo; anotar o **commit**. O build precisa de `zig`, do `boot.img` **do seu próprio aparelho** (`STOCK_BOOT` em `local.env`, nunca versionado) e do firmware do toque (`tools/get-touch-firmware.sh`). A imagem de teste é `work/bankphone-os-ro.img` (`scripts/pack-test-image.sh`, que acrescenta `bankphone.ro=1 bankphone.devcmd=1`).
2. Registrar: SHA-256 da imagem, versão do `zig`, SO e versão do `fastboot`, data. Guardar a saída do `build.sh`.
3. **Pendência de ABI (HW-20):** o `build.sh` compila `aarch64-linux-musl -mcpu=cortex_a53 -static`. Confirmar no aparelho (serial, depois do HW-01) que o kernel é 64 bits e o espaço de usuário roda esse binário sem erro. Hoje isso só está **inferido** do fato de o autor relatar boot.

## 2. HW-01: boot e reinício

| Item | Conteúdo |
|---|---|
| Objetivo | O sistema sobe de forma repetível e não reinicia sozinho |
| Procedimento | `tools/test_boot_b.sh --dry-run` → ler a saída → `tools/test_boot_b.sh --capture` (pede a frase de confirmação) → esperar 5 min → desligar a frio e ligar **10 vezes** seguidas. Registrar cada uma |
| Esperado | Sobe nas 10; relatório de boot completo na serial; nenhum reinício espontâneo em 5 min; `slot ativo` é o alvo gravado |
| Evidência | 10 capturas seriais com marca de tempo (`work/serial-capture.txt` renomeado por tentativa), foto da tela por boot, SHA da imagem, commit, saída do `--dry-run` e da gravação |
| Passa se | 10/10 boots completos. Qualquer falha **é resultado**: arquivar e parar |
| Parar e voltar ao Android se | Bootloop; o aparelho não responde a `fastboot`; `current-slot` inesperado; qualquer mensagem de erro do script |
| Fora de escopo | O fallback A/B (HW-03) e o watchdog (HW-02) têm linhas próprias |

## 3. HW-04: display (cuidado: `screenshot.py` **não** prova o painel)

- `tools/screenshot.py` captura **o buffer que a UI desenhou na memória**, não o painel físico. Ele prova que o sistema desenhou a tela esperada, **não** que o painel a mostra certa (ordem de canais BGR, resolução, cores, bordas).
- Procedimento: (a) as barras de teste de cor do `bootdiag` e a tela inicial **fotografadas com a câmera de outro aparelho**, de frente, sem reflexo; (b) o log `bd_fb_*` com a resolução e o formato detectados; (c) um `screenshot.py` da mesma tela, para comparar o buffer com a foto.
- Esperado: resolução lida do framebuffer e registrada no log (os comentários do código falam em ~1612 px de altura; o código **detecta**, não fixa: o número vem do log, não daqui), faixas na ordem esperada, texto legível, sem corte nas bordas, sem inversão de cor entre foto e buffer.
- Evidência: foto, PNG do buffer, trecho do log (sem identificadores), commit e SHA.
- Passa se: a foto bate com o buffer em canais, geometria e legibilidade. Diferença é achado: **arquivar**, não "ajustar até passar".

## 4. HW-05: toque, teclado e fluxo do PIN (cuidado: `--tap` **injeta**, não usa o painel)

- `screenshot.py --tap x,y` manda um toque **pelo cabo USB**. Serve para automatizar telas, **não** exercita o digitalizador. Para HW-05 use **o dedo**.
- Procedimento, nesta ordem, com o log serial aberto:
  1. Tocar os 4 cantos e o centro; anotar as coordenadas do log contra a posição física.
  2. Arrastar e soltar; 60 s sem tocar (sem toque fantasma).
  3. **Fluxo do PIN** (a imagem é somente leitura: o PIN existe só na memória deste boot): criar um PIN de 6 dígitos, confirmar; bloquear; digitar errado 5 vezes; ver a **contagem do bloqueio** na tela (`fix/ui-input-and-display`, #31) e confirmar que o PIN certo **não** entra durante o bloqueio; esperar 30 s; entrar.
  4. Teclar um valor de swap em USDC com 6 a 9 dígitos e ver se o valor mostrado bate com o digitado (#31).
- Esperado: coordenadas coerentes (erro de poucos pixels), sem travar, sem toque fantasma; bloqueio e contagem como no host.
- Evidência: log com coordenadas, vídeo curto da tela (câmera externa), fotos da contagem do bloqueio, commit e SHA.
- **Atenção:** a tela de desbloqueio com scrypt (#13, **ainda não mesclado**) **trava** enquanto o KDF roda. Isso é o HW-07, **não** deste teste; esta imagem usa o verificador atual (SHA-256 × 50.000).

## 5. Onde arquivar (sem identificadores)

`docs/evidence/hw/AAAA-MM-DD-HW-NN/` com: `README.md` (modelo abaixo), logs com **série, IMEI, MAC e nomes de nó borrados**, fotos, PNGs. Um teste que falha também vai para lá.

```markdown
# HW-NN <título> — AAAA-MM-DD
- Quem executou: <nome>            - Commit: <sha>            - SHA-256 da imagem: <sha>
- Slot gravado / ativo antes: <b> / <a>        - fastboot: <versão>        - zig: <versão>
- Procedimento seguido: <links e desvios>
- Resultado esperado:    <...>
- Resultado observado:   <...>
- Passou? <sim | não | parcial>    - Evidência: <arquivos>
- Anomalias e o que NÃO foi medido: <...>
```

Marcar `VERIFICADO` em `HARDWARE_TESTS.md` **só** com o log + foto/captura **arquivados no repositório** (regra 1 daquele arquivo). Relato sem arquivo continua `RELATADO PELO AUTOR`.

## 6. Itens novos descobertos ao preparar o plano

| ID | Item | Estado |
|---|---|---|
| HW-20 | Confirmar arquitetura do espaço de usuário no aparelho (aarch64, 64 bits) e `time_t` de 64 bits | **NÃO VERIFICADO** (o `build.sh` assume; musl estático) |
| HW-21 | O eMMC tem partição RPMB acessível a um espaço de usuário sem Android? (ligado à proposta de contador monotônico em `docs/STATE_INTEGRITY.md`) | **NÃO VERIFICADO**, só **leitura de informação**. **Nunca** gravar a chave da RPMB: a programação é irreversível |
