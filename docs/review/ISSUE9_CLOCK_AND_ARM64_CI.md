# Plano técnico: relógio como fonte não confiável (issue #9) e CI ARM64 / Plan: untrusted clock (#9) and ARM64 CI

**Tudo é DEMO / TESTNET. / Everything is DEMO / TESTNET.** Referência: `main` `a384698`. Parte 1 é **plano**, nada implementado. Parte 2 tem **evidência de emulação** produzida nesta rodada, que **não** é evidência do aparelho.

**EN summary.** Part 1 inventories every use of time in the engine, shows which uses are already safe and which are not, and proposes one conservative rule (re-stamp every saved timestamp to "now" whenever the clock domain or the boot is not provably the same) plus a test matrix. It does **not** claim rollback protection: an attacker who restores an older state also restores its timestamps. Part 2 reports what was run for ARM64 (the real device build target cross-compiled, 779 checks under emulated aarch64), what that proves and does not, and proposes CI jobs.

---

# Parte 1. O relógio

## 1.1 Como o tempo é obtido hoje [A]
`init/main.c:907-909`: `plat_clock_valid()` é verdadeiro se `time(NULL) > 1700000000`; `plat_now()` devolve a hora real se válida, **senão o uptime** (`CLOCK_MONOTONIC`, segundos desde o boot). O mesmo número, `plat_now()`, tem **duas unidades diferentes** conforme o aparelho tenha ou não hora válida, e o domínio pode mudar entre um boot e outro, ou dentro do mesmo boot. Não se sabe **o que ajusta o relógio real neste aparelho** (sem Android, sem rede): é a medição **HW-15**.

## 1.2 Inventário dos usos do tempo

| Uso | Onde | Gravado? | Estado | Risco |
|---|---|---|---|---|
| Bloqueio do PIN (`locked_until`) | `sec.c` | **sim**, absoluto | **corrigido no #30** (ancorado uma vez, no máximo 1920 s; relógio inválido devolve o máximo) | restam: contagem do tempo restante após troca de domínio é conservadora (pode bloquear até 1920 s a mais), nunca a menos |
| Validade da cotação (20 s) | `money.c` `pend_*` | **não** (tabela estática; cotações pendentes viram `EXPIRED` ao carregar, I-14) | seguro entre boots; relógio que anda para trás **dentro do boot** deixa uma cotação valer além de 20 s (reproduzido, só por troca de domínio no mesmo boot; sem caminho real conhecido) | baixo |
| Espera de 30 s (`L_COOLDOWN`) | `money.c:201` | `created` é gravado, mas `AWAITING_AUTH` vira `EXPIRED` ao carregar | seguro entre boots | ver `docs/issues/S01-*` |
| Vida da concessão (120 s) | `sec.c` | **não** | relógio que volta apaga a concessão (`now < G.at`) | seguro |
| **Janela diária de Pix (86400 s)** | `money.c:131` (`m_pix_spent_since(now - 86400)`) | **sim** (`t->created`) | **NÃO corrigido** | **ver 1.3** |
| Texto "há N min" na tela | `screens.c` `when_str` | sim (`created`) | **NÃO corrigido** (O-9: "há -82 min", datas de 1969) | cosmético |

## 1.3 O problema que falta: a janela diária mistura unidades [B, reproduzido na rodada 3]
`created` é gravado na unidade do boot em que a transação nasceu e lido na unidade do boot atual.

| Gravado em | Lido em | Efeito na janela diária |
|---|---|---|
| hora real (≈ 1,7e9) | uptime (pequeno) | `created` parece estar no futuro: **conta como gasto "recente" por muito tempo** (fail-closed, negação de uso) |
| uptime (pequeno) | hora real (≈ 1,7e9) | `created < now - 86400`: **o gasto deixa de contar** (fail-**open**: contorna o teto) |
| uptime, boot anterior | uptime, boot novo | épocas diferentes: contagem errada nos dois sentidos |

O segundo caso é o que importa para segurança (O-2 e O-3 em `MONEY_INVARIANTS.md`). Ele exige que o aparelho passe de "sem hora válida" para "com hora válida" entre dois boots, o que **só se sabe se acontece** depois do HW-15.

## 1.4 Regra conservadora proposta (não implementada)

> **Todo carimbo de tempo gravado é re-carimbado para "agora" quando o domínio ou o boot não for provadamente o mesmo da gravação.**

Mecanismo mínimo, **sem mudar o formato de cada transação**:
1. O estado grava **um** campo de cabeçalho: o **domínio da última gravação** (`W` = hora real, `U` = uptime) e, para `U`, nada mais (cada boot em `U` é uma época nova).
2. Ao carregar, na camada da plataforma (o carregador `m_deserialize` continua sem relógio): se `domínio gravado == W` **e** `domínio atual == W`, os carimbos valem; **em qualquer outro caso** (`W→U`, `U→W`, `U→U`), todo `created` de Pix de saída recente vira `now` (a janela de 24 h **recomeça**).
3. Efeito: o erro só pode ser para o lado **seguro**. Uma troca de domínio estende a janela em até 24 h; **nunca** a encurta.
4. O que isso **não** faz: não impede alguém de forçar trocas de domínio repetidas para manter a janela eternamente cheia (negação de uso, e exige acesso físico), e **não** detecta rollback (quem restaura um estado antigo restaura também os carimbos e o domínio).

**Trade-off a decidir:** sem relógio confiável o teto diário vira "teto por boot estendido", pois nunca se prova que se passaram 24 h. Alternativa mais permissiva: aceitar o contador por boot em `U` (a janela só conta Pix do boot atual). **Não recomendo** para dinheiro; é decisão do mantenedor.

## 1.5 Matriz de testes (host; nada disto toca o aparelho)

| Cenário | Bloqueio PIN | Teto diário | Cotação (20 s) | Espera (30 s) | Concessão (120 s) | Texto "quando" |
|---|---|---|---|---|---|---|
| Relógio **avança** normal | ok | ok | expira em 20 s | termina em 30 s | vence em 120 s | correto |
| Relógio **volta** dentro do boot | não encurta | não encurta | não vale além de 20 s | não encurta | apaga a concessão | sem número negativo |
| **Reinício** com o mesmo domínio `W` | ok | mantém a janela | pendentes expiram | pendentes expiram | some | correto |
| Reinício `U`→`U` | ≤ 1920 s a mais | **janela recomeça** | expiram | expiram | some | "antes deste boot" |
| `W`→`U` e `U`→`W` | ≤ 1920 s a mais | **janela recomeça** | expiram | expiram | some | "antes deste boot" |
| Relógio **inválido** (`-1`, `INT64_MIN`, > `NOW_MAX`) | máximo, nada muda | recusa | recusa | recusa | recusa | sem estouro |
| Valores de **fronteira** (`NOW_MAX`, `NOW_MAX+1`, 24 h exatas, 23:59:59) | idem | exatamente 86400 s | 19/20/21 s | 29/30/31 s | 119/120/121 s | idem |
| **Rollback** do estado | **não detectado** | **não detectado** | — | — | — | — |

A última linha é de propósito: um teste deve **mostrar** que o rollback passa, para que ninguém afirme proteção que não existe.

## 1.6 Entrega sugerida (itens pequenos e independentes)
1. `docs`: critérios de aceite da #9 com esta matriz. **Sem código.**
2. Testes de host da matriz sobre o motor atual (devem **falhar** nas linhas `U→W`, mostrando O-2).
3. Campo de domínio no estado + re-carimbo na carga (camada da plataforma). Só depois do HW-15 e da decisão do 1.4.
4. O-9: texto "quando" tolerante a carimbo futuro ("antes deste boot").

---

# Parte 2. ARM64 e CI

## 2.1 O que foi executado nesta rodada (computador, ambiente emulado)

| O quê | Como | Resultado |
|---|---|---|
| Compilar o `init` **como o `build.sh` faz** | `zig cc -target aarch64-linux-musl -mcpu=cortex_a53 -static -O2 -Wall -Wextra` (zig 0.17.0 do pacote PyPI `ziglang`; o `build.sh` não fixa a versão) sobre `main` `a384698` | compila **sem avisos**; ELF `aarch64`, estático, 316 KiB |
| Rodar a suíte em aarch64 | `make test CC=<zig cc -target aarch64-linux-musl> CFLAGS=-static…` sob `qemu-aarch64-static` (qemu-user 8.2) | **779 verificações, 0 falhas** na `main`; **1707** no teste de reconciliação #13 + #15 + `main` |
| Diferença de ABI | programa de prova | `char` é **sem sinal** no aarch64 e **com sinal** no x86_64; `long`, `time_t`, `size_t`, `int` iguais (8, 8, 8, 4) |

## 2.2 O que isso prova e o que **não** prova

**Prova (nível B, emulado):** o código compila para o alvo do aparelho sem avisos; o comportamento testado é o mesmo com `char` sem sinal; os tamanhos de tipo não mudam em relação ao x86_64 (`time_t` de 64 bits, então `time_t tt = (time_t)created` não trunca).

**Não prova:**
- Nada sobre o **X669C** (Cortex-A53 real, kernel e DTB do fabricante, vendor firmware). `qemu-user` não emula o kernel, o framebuffer, o toque, o `/dev/urandom` do aparelho, nem **tempo** (o custo do scrypt aqui não vale para o aparelho: HW-07).
- Sanitizers: **não foram tentados** neste alvo (musl estático sob qemu-user); a suíte ASan/UBSan continua **só em x86_64**.
- Falhas de alinhamento ou de ordem de memória que dependem do kernel/CPU reais.
- O `main.c` não é executado pela suíte (ele só é **compilado** pelo lint e pelo cross-build).
- **Emulação não é teste no aparelho.** Os itens HW-01 a HW-21 continuam `NÃO VERIFICADO`.

## 2.3 CI proposto (**não adicionado ao repositório**; é uma proposta para revisão)

O CI atual (`.github/workflows/ci.yml`) tem `linux` (x86_64) e `macos`. Três jobs complementares, em ordem de valor:

```yaml
  linux-arm64:                 # A. testes nativos em arm64 (não emulado), em CPU diferente do Cortex-A53 do aparelho
    runs-on: ubuntu-24.04-arm  # verificar disponibilidade para este repositório na documentação do GitHub
    steps:
      - uses: actions/checkout@v4
      - run: make test
      - run: make test-asan    # sanitizers em arm64 nativo: algo que o qemu-user estático não dá
      - run: make lint

  device-cross-build:          # B. o init com a MESMA linha do build.sh, só para compilar (sem imagem, sem STOCK_BOOT)
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - run: python3 -m pip install ziglang==<VERSAO_FIXADA>
      - run: python3 -m ziglang cc -target aarch64-linux-musl -mcpu=cortex_a53 -static -O2 -Wall -Wextra -Werror -Wno-unused-function -Wno-unused-parameter -o /tmp/init init/main.c init/gfx.c init/money.c init/sec.c init/store.c init/ui.c init/screens.c init/sheets.c init/components.c init/icons.c init/hw.c init/bootdiag.c -lm

  tests-qemu-aarch64:          # C. (opcional) o que foi feito aqui: suíte no alvo musl estático sob qemu-user
    runs-on: ubuntu-latest
    steps: [checkout, instalar qemu-user-static e registrar o binfmt, instalar ziglang fixado, "make test CC=<zig cc -target aarch64-linux-musl> CFLAGS=-static…"]
```

- **A** é o de maior valor e o mais simples. **B** protege o caminho real do aparelho (a linha do `build.sh` é duplicada no CI: manter as duas iguais, ou extrair para um script chamado pelos dois). **C** só vale se A não estiver disponível.
- **Fixar a versão do `zig`** (hoje o `build.sh` usa o que estiver no `PATH`). A que usei aqui, 0.17.0, **não** é necessariamente a do autor; confirmar com ele.
- Nenhum job substitui a Parte "hardware". O texto do CI deve dizer **"arm64 (nativo)"** ou **"aarch64 (emulado)"**, nunca "testado no aparelho".

## 2.4 Decisões e bloqueadores desta parte
1. Aprovar (ou não) os jobs A e B no CI (muda `.github/workflows/ci.yml`, só com aprovação).
2. Confirmar a versão do `zig` do autor para fixá-la.
3. HW-20: confirmar que o aparelho roda mesmo o binário `aarch64`/musl estático de 64 bits (hoje é inferência).
