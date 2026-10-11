# BANKPHONE OS: auditoria técnica (Fase A)

> **English summary.** This is an evidence-based audit of the repository at commit `86cda6f` (branch `main`).
> It was done **without the phone** and **without a cross-compiler**, so nothing here proves that the system
> boots, draws, or reads touch on the Infinix Hot 30i X669C. Host tests and static reading are labelled as such.
> Everything that needs the phone is marked `NÃO VERIFICADO` and listed in section 9.
> Confirmed defects (each reproduced or read directly in code) are in section 5; unproven suspicions are in
> section 6. The document is in Portuguese because the project plan is.

**Tudo é DEMO / TESTNET.** Este documento não afirma que o sistema seja seguro para guardar dinheiro real.

---

## 1. Escopo e método

| Item | Valor |
|---|---|
| Repositório | `Jhonattan2121/Bankphone-OS` |
| Commit auditado | `86cda6f` (`main`) |
| Branches abertas no momento | `sec/scrypt-pin-kdf` (PR #13 aberto, **não** mesclado); `docs/project-audit` (esta auditoria) |
| Hardware-alvo | Infinix Hot 30i X669C, MediaTek MT6765H, ARM64 (kernel do próprio aparelho) |
| Auditor | feita **sem acesso ao aparelho** |

**O que foi feito**
- Leitura integral de `init/main.c`, `init/bootdiag.c` (partes de relatório, serial, watchdog e reboot), `init/store.c` e `store.h`, `init/sec.c`, `init/money.c`, `init/devcmd.h`; leitura dirigida de `init/hw.c`, `init/ui.c`, `init/screens.c`, `init/sheets.c`.
- Leitura integral de `install.sh`, `build.sh`, `scripts/pack-test-image.sh`, `tools/mkboot.py`, `tools/mkramdisk.py`, `tools/test_boot_b.sh`, `tools/get-touch-firmware.sh`, cabeçalho de `scripts/state-region-hash.sh`.
- Execução dos testes de computador, ASan/UBSan, lint e fuzzing (seção 8).
- Três experimentos pequenos para **confirmar** suspeitas (seção 5, itens F-02 e F-03).
- Comparação com as issues #1 a #11 e com os PRs #12 e #13.

**O que NÃO foi possível**
- **Não há aparelho.** Nenhuma afirmação sobre boot, display, toque, USB, bateria ou recuperação foi verificada em hardware.
- **Não há `zig`**, então `build.sh` não rodou e o binário ARM64 não foi gerado. Uma checagem de sintaxe com `clang --target=aarch64` falha por falta de sysroot, então a compilação ARM64 também é `NÃO VERIFICADO`.
- **Não há `STOCK_BOOT`** (boot.img do aparelho), então `mkboot.py`/`mkramdisk.py`/`pack-test-image.sh` não rodaram de ponta a ponta.
- Não li `init/stb_truetype.h` (biblioteca de terceiros) nem os binários de fonte.

## 2. Níveis de evidência usados

| Rótulo | Significa |
|---|---|
| **Testado no host** | Existe teste automático que roda no computador e passou nesta auditoria. |
| **Lido no código** | Confirmado lendo o arquivo e a linha, sem executar. |
| **Reproduzido** | Confirmado por um experimento que eu rodei. |
| **Relatado pelo autor** | Consta em dev log, README ou mensagem, sem log ou captura no repositório. |
| **NÃO VERIFICADO** | Sem evidência. Nada deve ser assumido. |

## 3. Estado do repositório

- 8 commits na `main` (3 originais + 5 do PR #12). Tamanho: cerca de 7.200 linhas de C em `init/` (sem `stb_truetype.h` e sem testes).
- CI (`.github/workflows/ci.yml`): última execução na `main` **verde** (run #3, commit `86cda6f`): Linux (testes, ASan/UBSan, lint, fuzz curto) e macOS (testes e lint, com 2 testes e parte do lint pulados por usarem headers do Linux).
- Não existe `docs/` na `main`, nem `ROADMAP.md`, nem templates de issue/PR, nem pasta de evidências de hardware.
- 11 issues abertas (#1 a #11), todas de funcionalidade futura. **Nenhuma** trata de boot, recuperação, display, toque, USB ou documentação do estado real.

## 4. Tabela-resumo

Prioridades: P0 bloqueador, P1 alta, P2 média, P3 evolução. "Estado" nunca é maior do que a evidência.

| Área | Estado verificado | Evidência | Risco | Próxima ação |
|---|---|---|---|---|
| Boot/init | Código presente e coerente (ordem de montagem, estágios, nunca reinicia sozinho). **Boot no aparelho: NÃO VERIFICADO aqui; relatado pelo autor** (dev logs #1 e #2). | `init/main.c:937-1180`; README | **Alto** | Provar com log e foto; documentar a árvore de falhas (N01) |
| Display | Código presente (detecção de fb, ordem de canais BGR, barras de teste). Desenho testado no host (28 checks). **No aparelho: NÃO VERIFICADO aqui.** | `init/main.c:140-168`, `init/bootdiag.c` `bd_fb_*`, `init/tests/test_bootdiag_fb.c` | Alto | Arquivar evidência (N08) |
| Toque | Núcleo de eventos testado no host (21 checks). Descoberta do nó e "kick" de firmware no `main.c`. **No aparelho: NÃO VERIFICADO aqui.** | `init/touchcore.h`, `init/main.c:263-700`, `init/tests/test_touchcore.c` | Alto | Arquivar evidência (N09) |
| USB/recovery | Comando `REBOOT-BOOTLOADER` testado no host (16 checks + fuzz). Gadget ACM configurado no `main.c`, **sem teste de host**. README admite que a volta A/B e o comando serial **não foram provados**. | `init/devcmd.h`, `init/main.c:103-126`, README | Alto | Especificar o protocolo (N10); provar A/B (N02) |
| Persistência | `store.c` com dois slots, marcador PENDING e CRC, **testado no host com 171 checks** inclusive queda de energia em cada passo. No aparelho: NÃO VERIFICADO. Sem cifra e sem autenticação. | `init/store.c`, `init/tests/test_store.c` | Alto | Auditar o carregamento (N15); cifra/rollback no #9 |
| Segurança | PIN: SHA-256 iterado (50.000), salt, PIN cortado em 16 bytes, comparação em tempo constante. 7 checks no host. **PR #13 troca por scrypt** (não mesclado). O contador de erros é gravado com o estado a cada tentativa **só quando a área de estado está armada**, e **depois** da conferência (`init/ui.c:228-229`); em imagem somente leitura fica na memória. | `init/sec.c:35-52` (na `main`), `tests/sec_test.c` | Alto | Mesclar #13; achados F-04, F-06 |
| Motor financeiro | Estados, idempotência, tetos e expiração de cotação **testados no host (40 checks) e por fuzzing**. Invariantes só em parte formalizadas. Autenticação decidida pelo chamador (F-06). | `init/money.c`, `tests/money_test.c`, `tests/fuzz/fuzz_money.c` | Alto | Especificar invariantes (N11); fronteira de auth (N07) |
| Build/CI | Makefile e CI verdes. **Build ARM64 e empacotamento: NÃO VERIFICADOS aqui.** Sem hashes publicados dos artefatos, sem versões fixadas. | `Makefile`, `.github/workflows/ci.yml`, `build.sh` | Médio | Build reproduzível (N18) |
| Documentação | README bilíngue e honesto sobre DEMO. Mas: referências a ferramentas e caminhos que não existem, sem guia de recuperação, sem roadmap. | README; seção 5, F-07 | Médio | N13, N16, N20 |

## 5. Defeitos confirmados

Cada item diz **como foi confirmado**. Nenhum depende de hardware.

### F-01. `tools/test_boot_b.sh` pode sobrescrever o slot ativo
- **Evidência (lido no código):** `tools/test_boot_b.sh:10-14`. Espera o fastboot **sem limite de tempo** (`until ... sleep 1`), imprime o slot atual mas **não o confere**, e roda `fastboot flash boot_b` seguido de `set_active b`, **sem pedir confirmação**.
- **Cenário:** depois da primeira instalação o slot `b` fica ativo. Rodar o script de novo regrava o slot em uso e remove o caminho de volta.
- **Contraste:** `install.sh` faz o certo (escolhe o slot inativo, recusa o alvo igual ao original, pede frase digitada). O script antigo continua em `tools/` e não é citado no README.
- **Prioridade:** P1.

### F-02. `plat_boot_prop` lê a cmdline pelo primeiro trecho parecido, não pela chave exata
- **Evidência (reproduzido):** `init/main.c:858-865`. Copiei a função num programa de teste (`/tmp`, fora do repo) e rodei:
  - `bankphone.statehash=... bankphone.state=expdb` → `prop("bankphone.state")` devolve **vazio**, porque o primeiro `strstr` acha `bankphone.statehash` e a checagem do `=` falha. A persistência é ignorada em silêncio. Falha para o lado seguro (não grava), mas é um defeito.
  - `xbankphone.ro=1` e `androidboot.bankphone.ro=1` são lidos como `bankphone.ro=1`.
- **Por que importa:** `bankphone.ro` é a trava que impede qualquer gravação. Hoje nenhuma chave existente é prefixo de `ro`, então não há falha, mas a trava depende de uma leitura frouxa.
- **Prioridade:** P2.

### F-03. `store_open()` vaza um descritor de arquivo se for chamado duas vezes
- **Evidência (reproduzido):** programa de teste com o backend de arquivo: depois de duas aberturas bem-sucedidas, `fds +2`; depois de `store_close()`, ainda `fds +1`. `init/store.c:228` e `:240` atribuem `sfd = open(...)` sem fechar o anterior.
- **Quem chama duas vezes:** `init/main.c:1040` (modo diagnóstico) e depois `init/main.c:1163` (retentativa quando a UI finalmente sobe). Também ignora o retorno de `pin_deserialize`/`m_deserialize` (`init/main.c:907`).
- **Prioridade:** P2.

### F-04. `plat_random` cai para bytes previsíveis sem avisar
- **Evidência (lido no código):** `init/main.c:848`. Se `/dev/urandom` não abrir, preenche com `boot_mono() * 31 + i * 17`. Não confere leitura curta (`read` com retorno ignorado). Essa função gera o **salt do PIN** (`init/ui.c:219-220`).
- **Observação:** após o `devtmpfs` montado, `/dev/urandom` normalmente existe; o risco é o fallback silencioso, não o caminho normal.
- **Prioridade:** P1 (segurança).

### F-05. O relatório de boot grava na partição `expdb` por padrão, protegido só por lista de nomes
- **Evidência (lido no código):** `init/bootdiag.c:331-337` usa `"expdb"` quando nenhuma partição é dada; a proteção é a lista de bloqueio `g_deny` (`:242-258`), por nome. Se `BLKGETSIZE64` falhar, segue **sem checar o tamanho** (`:363`). `bd_flush()` (`:398`) faz `pwrite` de até 64 KiB mais `fsync` e é chamada em **34 pontos** do código.
- **Contraste:** o `store` exige o **hash do backup** da região antes da primeira escrita (`init/store.h`, regra R3). O relatório não exige nada parecido.
- **Mitigação existente:** a imagem de teste (`bankphone.ro=1`) não abre o relatório (`init/main.c:961-967`). O instalador só grava essa imagem. O risco aparece com a imagem **não somente leitura** (`work/bankphone-os.img`, gerada por `./build.sh`).
- **O que não sei:** quanto desgaste real isso causa na eMMC, e se `expdb` tem conteúdo útil na região usada (`BD_BASE` = 8 MiB). `NÃO VERIFICADO`.
- **Prioridade:** P1 (segurança do aparelho).

### F-06. A política de autenticação é decidida fora do motor financeiro
- **Evidência (lido no código):** `m_authorize(Tx *t, Level got, int64_t now)` (`init/money.c:130`) aceita o nível "alcançado" como parâmetro. Na UI, `ACT_CONFIRM2` (`init/ui.c:326-334`) chama `m_authorize(t, m_required(t), ...)`, isto é, passa **o próprio nível exigido como se tivesse sido alcançado**. Que o PIN foi digitado antes é garantido pelo **fluxo de telas**, não pelo motor.
- **Consequência:** qualquer caminho futuro que chame `m_authorize` (um comando serial, uma tela nova, um teste) consegue autorizar sem PIN. Também liga com a issue #4 (tela de confirmação do sistema).
- **Prioridade:** P1 (financeiro/segurança).

### F-07. Documentação e scripts apontam para coisas que não existem
- **Evidência (reproduzido com `grep`):**
  - `bankphonectl` e `python3 -m bankphone_host.cli` (em `scripts/state-region-hash.sh:24-25` e `scripts/pack-test-image.sh:46`): ferramentas que **não estão no repositório**.
  - `src/init/...` em vários comentários e em `scripts/state-region-hash.sh:71`: o código hoje está em `init/`.
  - `init/bootdiag.c:3`: "ver `src/init/README-bootdiag.md`", arquivo inexistente.
- **Prioridade:** P2.

### F-08. O relatório e a serial podem expor identificadores do aparelho
- **Evidência (lido no código):** `init/bootdiag.c:913` despeja `/proc/cmdline` inteiro, e o inventário também despeja `/proc/mounts` e `/proc/partitions` (`:965`). O relatório vai para `expdb` e é despejado na serial USB quando o host abre a porta.
- **O que não sei:** se a cmdline deste aparelho contém número de série ou outro identificador. `NÃO VERIFICADO`; é preciso capturar um relatório real e conferir.
- **Mitigação existente:** os logs **não** imprimem PIN nem hash (conferido com `grep` em `ui.c`, `sheets.c`, `screens.c`, `money.c`, `sec.c`).
- **Prioridade:** P2.

### F-09. O tratador de falha fatal não é seguro para sinais e trava para sempre
- **Evidência (lido no código):** `init/main.c:921-934` chama `bd_flush()`, que usa `malloc` (`init/bootdiag.c:408`), dentro do tratador de `SIGSEGV`/`SIGBUS`/`SIGILL`/`SIGFPE`/`SIGABRT`. Se a falha ocorreu dentro do `malloc`, o tratador pode travar. Depois faz `for (;;) pause();`.
- **Por desenho** o sistema nunca reinicia sozinho (`init/main.c:5-14`): é uma escolha para preservar o diagnóstico, mas deixa o aparelho preso até alguém segurar as teclas.
- **Prioridade:** P2.

### F-10. Constantes de demonstração duplicadas
- **Evidência (lido no código):** a cotação `1 USDC = R$ 5,60` aparece em `init/money.c:7` (`RATE 560`), em `init/screens.c:39` (`/ 560`) e `:93`, e em texto fixo em `init/sheets.c:121`. A tarifa `R$ 3,50` está em `init/money.c:8` e no texto de `init/sheets.c:121`.
- **Observação positiva:** a interface rotula DEMO em todas as telas e o comprovante diz `Hash on-chain: NENHUM (DEMO)` (`init/sheets.c:267`).
- **Prioridade:** P1 (risco de divergência entre o valor mostrado e o calculado).

### F-11. Itens menores
- `init/main.c` gera 11 avisos de truncamento de `snprintf` em `-O2` (linhas 86, 88, 121, 731-736, 764); `init/gfx.c` gera 4 de indentação enganosa. Nenhum é prova de defeito, mas truncar um nome de nó pode apontar para o caminho errado.
- `install.sh --dry-run` não grava nada, mas **reinicia o aparelho no bootloader** e, no modo Android, puxa o firmware do toque. É necessário para o teste e está dito no próprio resumo, mas o README diz "faz tudo, menos gravar".

## 6. Hipóteses ainda não verificadas

Nenhuma destas deve virar issue de bug confirmado.

| ID | Hipótese | Como verificar (precisa do aparelho) |
|---|---|---|
| H-1 | O **watchdog de hardware** da MediaTek pode reiniciar o aparelho se ninguém o alimentar. O código só **lista** o watchdog (`init/bootdiag.c:974-1000`) e comenta que não deve ser aberto sem alimentar. Se estiver armado pelo bootloader, o PID 1 atual não o alimenta. | Ler `/sys/class/watchdog/*/state` e `timeout` no log do relatório e comparar com o tempo até um reinício espontâneo. |
| H-2 | O **fallback automático A/B** (volta ao slot original se o novo não sobe) **pode não existir** neste bootloader. O README admite que não é garantido. | Gravar uma imagem propositalmente inválida só no slot inativo e observar (com plano de recuperação pronto). |
| H-3 | O firmware do toque pode precisar de `/vendor/firmware` no ramdisk, mas o caminho de busca do kernel pode ser outro. O código tenta 7 caminhos e loga qual existe. | Ler o log `toque: ... AUSENTE/presente` e `/proc/nvt_fw_version` no aparelho. |
| H-4 | `cpu_boost()` força o governador `performance` com frequência mínima igual à máxima e liga todos os núcleos (`init/main.c:721-742`). Pode afetar temperatura e bateria. | Medir temperatura/bateria em uso contínuo com e sem `bankphone.cpuboost=0`. |
| H-5 | O tempo do KDF (PR #13) no X669C pode passar de 1 s. | Rodar `make bench-kdf` compilado para ARM64. |
| H-6 | `vbmeta`/AVB pode exigir ajuste para a imagem de boot ser aceita. O instalador só grava `boot_<slot>`. | Observar o comportamento do bootloader com a imagem gravada. |
| H-7 | `plat_now()` usa o relógio de parede se for válido e **o tempo desde o boot** caso contrário (`init/main.c:846-847`). Se a base mudar entre boots, a janela diária de Pix e o bloqueio por tentativas (`locked_until`, gravado) podem ser avaliados na base errada. | Teste de host que simule a troca de base (N11) e, no aparelho, ver se o relógio de parede é válido sem rede. |

## 7. Pontos fortes (para não reescrever o que funciona)

- **Diagnóstico de boot:** número de estágio pintado direto no framebuffer, sem fonte, e relatório persistente (quando habilitado).
- **Modo somente leitura** (`bankphone.ro=1`) corta relatório, store e BCB de uma vez.
- **`store.c`** tem um protocolo de escrita bem pensado e bem testado, e recusa a primeira escrita sem o hash do backup.
- **HardwareService** (`init/hw.c`) descobre os nós em `/sys` e mostra `UNKNOWN`/`UNAVAILABLE` com motivo, em vez de inventar valores.
- **`install.sh`**: só aceita o X669C, só grava no slot inativo, exige frase digitada, tem `--dry-run` e `--restore`.
- **Interface honesta:** selo DEMO em todas as telas, "NOT IMPLEMENTED" onde falta.

## 8. Testes executados nesta auditoria (resultados reais)

Comandos rodados em `main` (`86cda6f`), Linux x86-64, `cc` (gcc):

| Comando | Resultado |
|---|---|
| `make test` | **283 verificações, TODOS OK** (money 40, sec 7, devcmd 16, store 171, touchcore 21, bootdiag_fb 28) |
| `make test-asan` | 283 verificações, TODOS OK |
| `make lint` | OK |
| `make lint-strict` | 7 avisos informativos (`money.c` 2, `sec.c` 5) |
| `make fuzz FUZZ_TIME=5` | `devcmd` 840 mil execuções e `money` 502 mil execuções, sem falha |
| Compilação `-O2 -Wall -Wextra` de cada arquivo de `init/` | 0 avisos, exceto `main.c` 11 e `gfx.c` 4 (F-11) |
| `clang --target=aarch64-linux-gnu -fsyntax-only init/main.c` | **falhou por falta de sysroot** (não prova nada sobre o código) |
| Experimentos F-02 e F-03 | resultados descritos na seção 5 |

**Não executado:** `build.sh`, `install.sh` (nem `--dry-run`), `pack-test-image.sh`, qualquer comando `adb`/`fastboot`, qualquer teste em aparelho.

## 9. O que exige o Infinix Hot 30i fisicamente

1. Confirmar o **boot repetido** (várias vezes, a frio) e capturar o relatório e as fotos de estágio.
2. Ler `/sys/class/watchdog/*` e fechar a hipótese H-1.
3. Provar (ou refutar) o **fallback A/B** e a recuperação documentada (H-2).
4. Validar **display**: resolução, stride, ordem de canais, barras (N08).
5. Validar **toque**: nó correto, coordenadas, firmware (N09).
6. Validar o **comando serial** do instalador e o canal de diagnóstico (N10).
7. Medir o **custo do KDF** e o **desempenho** (N, H-5).
8. Capturar um relatório real para checar **identificadores** (F-08).
9. Medir temperatura/bateria com e sem `cpu_boost` (H-4).

## 10. Riscos de segurança (resumo honesto)

- **Sem TEE, sem verified boot e com bootloader destravado**, quem tem o aparelho consegue copiar o armazenamento e atacar o PIN offline. PIN de 6 dígitos não resiste a isso com nenhum KDF (detalhes em `docs/THREAT_MODEL.md` no PR #13).
- O contador de tentativas de PIN é gravado a cada tentativa **quando a área de estado está armada** (`plat_save()` logo depois de `pin_check()`, `init/ui.c:228-229`), mas **não** em imagem somente leitura, onde reiniciar o zera. Mesmo armado, é gravado **depois** da conferência, e o estado pode ser restaurado por quem edita o armazenamento (#9).
- O estado salvo tem **CRC, não autenticação**: detecta corrupção acidental, não adulteração (#9).
- O motor financeiro **não impõe** a política de autenticação sozinho (F-06).
- O relatório de boot grava em partição bruta por padrão na imagem completa (F-05).
- **Nenhuma** afirmação de que o sistema é seguro para dinheiro real é feita ou deve ser feita.

## 11. Mapeamento com as issues e PRs existentes

| Item da auditoria | Existente | Decisão |
|---|---|---|
| KDF do PIN e modelo de ameaças | #2, PR #13 | **Reaproveitar.** Não abrir duplicata. |
| Estado cifrado, rollback, relógio monotônico | #9 | **Reaproveitar** (adicionar rótulo de prioridade). |
| Persistência: auditoria do carregamento | #9 não cobre | **Nova** (N15). |
| CI, sanitizers, fuzzing | #10 (≈70% feita) | **Reaproveitar** para o que resta. |
| Atualização assinada e A/B | #8 | **Reaproveitar**; recuperação testada é **nova** (N02). |
| Tela de confirmação do sistema | #4 | **Reaproveitar**; fronteira de auth no motor é **nova** (N07). |
| Integração financeira futura | #11 | **Reaproveitar** (já é a discussão de arquitetura). |
| Carteira, PIN de coação, P2P, recibos, cofre | #1, #3, #5, #6, #7 | **Reaproveitar**, prioridade P3. |

As issues novas propostas estão em `docs/issues/` (prontas para publicar) e a ordem de execução em `docs/IMPLEMENTATION_PLAN.md`.
