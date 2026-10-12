# Textos prontos para o GitHub / Ready-to-paste GitHub texts

**Tudo é DEMO / TESTNET. / Everything is DEMO / TESTNET.**

Estes textos são para **você** copiar para o GitHub. Eles não foram publicados por ninguém. Os critérios marcados
como cumpridos foram conferidos contra o **diff real** de cada branch (commit citado). Onde um critério do pedido
**não** é cumprido literalmente, o texto diz isso.

Conteúdo / Contents: 1 · N04 · 2 · N14 · 3 · N15 · 4 · N05 (#15, base) · 5 · script de publicação · 6 · decisão
do PR #14 · 7 · correção do texto do PR #13 · 8 · atualização da issue #2 · 9 · atualização da issue #10.

Convenção: títulos em inglês e português separados por ` / `, como o resto do repositório. Sem rodapé de
assistente, sem trailer de coautoria.

---

## 1. N04 — script de boot A/B / A/B boot script

**Branch:** `fix/boot-b-script-slot-safety` · commit `537d1ab` · base `main` · 1 commit, 4 arquivos.

**Título / Title**
```
fix(tools): test_boot_b.sh must never flash the active slot / test_boot_b.sh nunca pode gravar no slot ativo (N04)
```

**Descrição / Description**
```markdown
## Summary / Resumo

**EN:** `tools/test_boot_b.sh` flashed `boot_b` and activated `b` without checking which slot was running. After the
first install `b` is the active slot, so running it again overwrote the system in use and removed the way back.
It now reads the active slot, flashes **only the inactive one**, refuses when it cannot tell, and asks for a typed
phrase. Finding F-01 of the audit.

**PT:** `tools/test_boot_b.sh` gravava `boot_b` e ativava `b` sem conferir qual slot estava em uso. Depois da
primeira instalação o slot ativo é o `b`; rodar de novo sobrescrevia o sistema em uso e removia o caminho de volta.
Agora o script lê o slot ativo, grava **só o inativo**, recusa quando não consegue saber e pede uma frase digitada.
Achado F-01 da auditoria.

## What changed / O que muda

**EN**
- Reads `slot-count`, `current-slot` and `unlocked`. Needs two slots and an unlocked bootloader. The target is the
  other slot; if the active slot cannot be read (empty, `c`, `_a`, anything but `a`/`b`) it **refuses** and writes nothing.
- Validates the image **before** anything is written: exists, is a regular file, is not empty, starts with `ANDROID!`.
  Records its SHA-256.
- `--dry-run` does everything except flash, activate and reboot.
- Typed confirmation: `FLASH boot_<target>`. The old phrase for the wrong slot is refused.
- `--wait SECONDS` (default 60): never waits forever for fastboot; non-numeric value is a usage error.
- A last guard right before the flash: the target must differ from the active slot.

**PT**
- Lê `slot-count`, `current-slot` e `unlocked`. Exige dois slots e bootloader destravado. O alvo é o outro slot; se
  o slot ativo não puder ser lido (vazio, `c`, `_a`, qualquer coisa fora de `a`/`b`) **recusa** e não grava nada.
- Valida a imagem **antes** de gravar: existe, é arquivo comum, não está vazia, começa com `ANDROID!`. Registra o SHA-256.
- `--dry-run` faz tudo, menos gravar, ativar e reiniciar.
- Confirmação digitada: `FLASH boot_<alvo>`. A frase antiga do slot errado é recusada.
- `--wait SEGUNDOS` (padrão 60): nunca espera para sempre pelo fastboot; valor não numérico é erro de uso.
- Uma última guarda logo antes de gravar: o alvo precisa ser diferente do slot ativo.

## Tests / Testes
`tests/scripts/boot_b_test.sh`: **20 verificações** com um `fastboot` **falso** no PATH. Cobre: slot `a` ativo → grava
só `boot_b`; slot `b` ativo → grava só `boot_a` e **nunca** `boot_b`; slot `b` com a frase antiga → recusa; slot vazio,
`c` e `_a` → recusa; sem 2 slots; bootloader travado; imagem ausente, vazia e sem cabeçalho; confirmação errada;
`--dry-run` sem gravar; timeout; opções inválidas; e uma checagem estática de que não há `flash boot_a/boot_b` fixo.
`make test` e `make lint` passam. **Nenhum teste automático toca um aparelho.**

## Limits / Limites
- Provado só contra um `fastboot` falso. O comportamento real do `fastboot getvar` no Infinix Hot 30i X669C
  (formato de `current-slot`, saída em stderr) **não foi verificado**; o script lê stdout e stderr, mas isso só
  se confirma no aparelho (HW-01 e HW-03 em `docs/review/HARDWARE_TESTS.md`).
- O timeout cobre a **espera pelo aparelho**, não a duração do `fastboot flash` em si.
- Gravar um slot não prova que o aparelho volta ao slot original numa falha de boot (hipótese H-2, issue N02).

Refs N04 (F-01). Tudo é DEMO/TESTNET.
```

**Critérios de aceite / Acceptance criteria** (conferidos contra o diff `537d1ab`)

| Critério pedido | Cumprido? | Onde |
|---|---|---|
| Identifica slot ativo e inativo | Sim | `getv current-slot` + `case` |
| Recusa quando não consegue determinar o estado | Sim | `die "não consegui ler o slot ativo..."`; testes do slot vazio, `c`, `_a` |
| Valida a imagem antes de gravar | Sim | linhas de validação antes do `fastboot flash` |
| `--dry-run`, confirmação explícita, timeout | Sim (timeout só da espera do aparelho) | `--dry-run`, `PHRASE`, `--wait` |
| Testes com `fastboot` falso; sem gravar em aparelho | Sim | `boot_b_test.sh` (20 verificações) |
| Funciona no aparelho real | **Não verificado** | HW-01, HW-03 |

---

## 2. N14 — parser exato da cmdline / exact cmdline parser

**Branch:** `fix/cmdline-exact-token-parser` · commit `153e963` · base `main` · 1 commit, 17 arquivos (inclui o corpus de fuzz).

**Título / Title**
```
fix(init): match kernel cmdline keys as whole tokens, with no shared buffer / ler a cmdline por token inteiro, sem buffer compartilhado (N14)
```

**Descrição / Description**
```markdown
## Summary / Resumo

**EN:** `plat_boot_prop` found a key with the first `strstr` hit, so it depended on the order of the parameters and
on prefixes, and it returned one shared static buffer, so reading three values in a row left three pointers aiming
at the same text. The switch `bankphone.ro=1` (no writes at all) is read through it. Both defects were reproduced
before the fix. Finding F-02 (plus an aliasing bug found while fixing it).

**PT:** `plat_boot_prop` achava a chave pelo primeiro `strstr`, então dependia da ordem dos parâmetros e de prefixos, e
devolvia um único buffer estático, de modo que ler três valores em seguida deixava três ponteiros para o mesmo texto.
A trava `bankphone.ro=1` (nenhuma gravação) é lida por aí. Os dois defeitos foram reproduzidos antes da correção.
Achado F-02 (mais um bug de aliasing achado durante a correção).

## What changed / O que muda

**EN**
- New `init/cmdline.h` (no I/O, testable on a PC): the command line is a list of tokens separated by space, tab
  or newline; `key=value` matches only as a **whole token**. `xbankphone.ro=1`, `androidboot.bankphone.ro=1`,
  `bankphone.rox=1` and `bankphone.statehash=…` do **not** match `bankphone.ro` / `bankphone.state`.
- The order of the parameters does not matter (before: `statehash` before `state` made `state` read as empty).
- `key` without `=` does not match; `key=` matches with an empty value; a repeated key: the **last** one wins (design
  decision, documented in the header).
- `cmdline_get` copies into a caller buffer, always NUL-terminated, returns 1 found / 0 absent / -1 truncated.
  Values are limited to 64 bytes; a longer value is reported as truncated, never overflows.
- `cmdline_prop` rotates through 8 buffers, so up to 7 earlier results stay valid. `load_state` copies the three
  values it needs into local buffers.

**PT**
- Novo `init/cmdline.h` (sem I/O, testável no computador): a cmdline é uma lista de tokens separados por espaço, tab
  ou quebra de linha; `chave=valor` só casa como **token inteiro**. `xbankphone.ro=1`, `androidboot.bankphone.ro=1`,
  `bankphone.rox=1` e `bankphone.statehash=…` **não** casam com `bankphone.ro` / `bankphone.state`.
- A ordem dos parâmetros não importa (antes: `statehash` antes de `state` fazia `state` ler vazio).
- `chave` sem `=` não casa; `chave=` casa com valor vazio; chave repetida: vale a **última** (decisão de projeto,
  documentada no cabeçalho).
- `cmdline_get` copia para um buffer do chamador, sempre com NUL, devolve 1 achou / 0 ausente / -1 cortado.
  Valores têm no máximo 64 bytes; um valor maior é sinalizado como cortado, nunca estoura.
- `cmdline_prop` gira entre 8 buffers, então até 7 resultados anteriores continuam válidos. O `load_state` copia os
  três valores de que precisa para buffers locais.

## Tests / Testes
- `tests/cmdline_test.c`: **36 verificações** — token inteiro, prefixos, as duas ordens, `=` dentro do valor, separadores
  (espaço, tab, quebra de linha, vários), chave repetida, valor vazio, buffers de 1, 2 e exato, `n = 0`, ponteiros
  nulos, chave vazia/com `=`/com espaço, cmdline de ~4 KiB, e que três leituras seguidas **não** apontam para o mesmo buffer.
- `tests/fuzz/fuzz_cmdline.c` com corpus. `make test`, `make test-asan` e `make lint` passam.
- Os dois bugs originais foram **reproduzidos com a função antiga** antes da correção (relatado em `docs/PROJECT_AUDIT.md`, F-02). Os testes novos exercitam o parser novo (`cmdline.h`); a função antiga não é mais compilada nos testes.

## Limits / Limites
- Host apenas. A cmdline real do aparelho (formato, tamanho, o que o bootloader acrescenta) **não foi vista**; HW-09.
- "Última ocorrência vale" é uma escolha; o kernel Linux costuma deixar a última sobrescrever, mas **não verifiquei a
  regra exata** para estes parâmetros. Quem controla a cmdline já controla o boot; isto não é uma fronteira de segurança.
- Aspas não são interpretadas.

Refs N14 (F-02). Tudo é DEMO/TESTNET.
```

**Critérios / criteria** (conferidos contra o diff `153e963` e o cabeçalho `cmdline.h`)

| Critério pedido | Cumprido? |
|---|---|
| Correspondência por token completo, sem substring | Sim (testes de prefixo e sufixo) |
| Limites de buffer | Sim (1, 2, exato, `n=0`, 64 bytes, 4 KiB) |
| Tokens repetidos | Sim, com regra definida (última vence) |
| Valores ausentes | Sim (sem `=` não casa; `chave=` é vazio) |
| Independe da ordem | Sim (teste das duas ordens; fuzz) |
| Regressão dos casos que reproduziam o bug | Sim, como casos do parser novo (`statehash` antes de `state`; três ponteiros distintos). O bug foi reproduzido com a função antiga, mas esse teste contra a função antiga não está no repositório |
| Provado com a cmdline real do aparelho | **Não verificado** |

---

## 3. N15 — carga validada e idempotente do estado / validated, idempotent state load

**Branch:** `fix/state-load-validated-idempotent` · commits `7b022b2` e `6f76322` · base `main` · 2 commits.

**Título / Title**
```
fix(state): validate the saved state all-or-nothing, close the fd on reopen, refuse Pix <= 0 / validar o estado gravado por inteiro, fechar o descritor ao reabrir, recusar Pix <= 0 (N15)
```

**Descrição / Description**
```markdown
## Summary / Resumo

**EN:** The loader ignored the result of `m_deserialize`/`pin_deserialize` and accepted fields that make no sense
(negative amounts, unknown states, duplicate ids, 401 transactions); `store_open` leaked one file descriptor per
reopen (`main.c` reopens the area after `ui_init`); and `m_receive_pix` accepted zero and negative amounts, which the
new strict loader would then refuse on the next boot (found by the fuzzer). Findings F-03 and part of N15.

**PT:** O carregador ignorava o resultado de `m_deserialize`/`pin_deserialize` e aceitava campos sem sentido
(valores negativos, estados desconhecidos, ids repetidos, 401 transações); o `store_open` vazava um descritor por
reabertura (o `main.c` reabre a área depois do `ui_init`); e o `m_receive_pix` aceitava valor zero e negativo, que o
carregador estrito recusaria no próximo boot (achado pelo fuzzer). Achado F-03 e parte da N15.

## What changed / O que muda

**EN**
- `m_deserialize` is **strict and all-or-nothing**: header `V1` required exactly once and first; every `T` line has
  13 fields; type, state, currencies, amounts (0..1e15), time (0..1e11), text lengths and the currency/amount
  coherence are checked; repeated idempotency keys, more than 400 transactions and a negative final balance are
  refused. On **any** error the ledger is reset to empty and `m_deserialize_error()` says which line and why.
- `load_state` now checks both results: if the PIN line or the ledger is refused, the PIN is restored to what it was
  and the reason goes to the boot report (before, failures were silent).
- `store_open` closes the previous descriptor first (reopening is expected).
- `m_receive_pix` refuses amounts `<= 0` and above the sanity cap.
- The saved text is sanitised when written, so a counterpart name with `|` or a newline cannot break the format.

**PT**
- `m_deserialize` é **estrito e tudo-ou-nada**: cabeçalho `V1` exatamente uma vez e primeiro; toda linha `T` tem 13
  campos; tipo, estado, moedas, valores (0..1e15), hora (0..1e11), tamanho dos textos e a coerência moeda/valor são
  conferidos; chave de idempotência repetida, mais de 400 transações e saldo final negativo são recusados. Em
  **qualquer** erro o livro-razão volta a vazio e `m_deserialize_error()` diz a linha e o motivo.
- O `load_state` agora confere os dois resultados: se a linha do PIN ou o livro-razão for recusado, o PIN volta ao que
  era e o motivo vai para o relatório de boot (antes, a falha era silenciosa).
- `store_open` fecha o descritor anterior primeiro (a reabertura é esperada).
- `m_receive_pix` recusa valor `<= 0` e acima do teto de sanidade.
- O texto gravado é higienizado na escrita; um nome com `|` ou quebra de linha não quebra mais o formato.

## Tests / Testes
- `tests/money_test.c`: **69 verificações** (eram 40): estados inválidos recusados por inteiro com motivo (7 formas),
  carregar duas vezes dá o mesmo resultado, **estado cortado em cada byte** é aceito inteiro ou recusado vazio,
  `m_receive_pix` com 0, negativo e acima do teto.
- `init/tests/test_store.c`: novo caso — 20 aberturas seguidas **sem fechar** não mudam o número de descritores.
  O teste **falha** sem a correção (4 antes, 24 depois) e passa com ela.
- `tests/fuzz/fuzz_money.c`: no modo "estado" confere os invariantes depois do `m_deserialize`. `fuzz_money`: 2,3 milhões
  de execuções em 30 s sem falha. `make test`, `make test-asan`, `make lint` passam.

## Limits / Limites
- "Tudo-ou-nada" aqui quer dizer: **nunca fica estado parcial**. A leitura constrói o resultado dentro de `M` e, se
  falhar, zera `M`; não guarda o estado anterior da memória (ele já foi zerado pelo `ui_init`). Não é transação atômica
  para outra thread; o sistema é de uma thread.
- O arquivo de estado **não é autenticado** (o CRC só pega acidente): validar campos não impede quem escreve no
  armazenamento de gravar um estado coerente e falso (issue #9).
- Não implementei "carregar uma vez só": o `ui_init` zera `M` e `PIN`, então o segundo carregamento do `main.c` é
  necessário. A correção foi o descritor, não a contagem de carregamentos.
- Host apenas; nada rodou no aparelho (HW-08).

Refs N15 (F-03). Tudo é DEMO/TESTNET.
```

**Critérios / criteria** (conferidos contra os diffs `7b022b2`, `6f76322`)

| Critério pedido | Cumprido? | Observação |
|---|---|---|
| Validar todos os campos antes de modificar o estado ativo | **Em parte** | A validação é completa, mas a leitura escreve dentro de `M` e zera em caso de erro. Resultado observável: nunca fica estado parcial. Não é "validar antes e só então aplicar" |
| Falhas de leitura não deixam estado parcial | Sim | `des_fail` chama `m_init`; `load_state` restaura o PIN; teste dos cortes byte a byte |
| Fechar corretamente o descritor ao reabrir | Sim | teste que falha sem a correção |
| Recusar valor `<= 0` | Sim | `m_receive_pix` (entrada) e `m_deserialize` (estado). **Obs.:** `m_prepare_pix`/`m_quote` já recusavam; só o `m_receive_pix` estava aberto |
| Testar arquivo truncado, corrompido, inválido, reaberturas repetidas | Sim | cortes em cada byte (host), 7 formas inválidas, 20 reaberturas, fuzz |
| Carregamento idempotente | Sim, para o **resultado** (mesmo estado ao carregar duas vezes) | "Carregar uma vez só" **não** é feito (ver Limits) |
| Documentar o formato do estado (`docs/STATE_FORMAT.md`, tarefa do rascunho N15) | **Não feito** | Fica para uma próxima etapa; a validação está descrita nos comentários de `money.c` |
| Validado no aparelho | **Não verificado** | HW-08 |

---

## 4. N05 — PR #15 (já aberto) / PR #15 (already open)

**Branch:** `fix/entropy-no-weak-fallback` · commit `cf94032` · **empilhado sobre `sec/scrypt-pin-kdf` (PR #13)**.

**Ações pendentes sobre o #15:**
1. No GitHub, trocar a **base** do PR de `main` para `sec/scrypt-pin-kdf` (hoje mostra 5 commits e 23 arquivos porque
   inclui o #13). Ou esperar o merge do #13 e deixar o GitHub recalcular.
2. O #13 recebeu dois commits novos depois do #15 (`0255d37`, `60935bc`). A branch do #15 **ainda não os contém**.
   Para trazê-los sem reescrever histórico: `git checkout fix/entropy-no-weak-fallback && git merge origin/sec/scrypt-pin-kdf`
   e `git push`. **Eu não fiz isso**: a tentativa foi bloqueada pelo ambiente e não contornei; faça você ou autorize.

O título e a descrição bilíngues já estão no PR. Critério de aceite resumido: sem aleatoriedade segura o PIN **não** é
criado, e o código não tem plano B previsível (`entropy.h`, 9 verificações). **Não testado:** o caminho de `ui.c` que
recusa criar o PIN, e o bloqueio de `getrandom()` no boot real (HW-13).

---

## 5. Script de publicação de issues / issue publishing script

**Branch:** `tools/publish-issues-script` · commits `1a37994`, `116a140` · empilhada sobre N04.

**Título / Title**
```
feat(tools): script to publish the issue drafts from the maintainer's account / script para publicar os rascunhos de issue pela conta do mantenedor
```

**Descrição / Description**
```markdown
## Summary / Resumo

**EN:** `scripts/publish-issues.sh` publishes the drafts in `docs/issues/` (N01–N20) with the `gh` CLI, from the
maintainer's own account. It is a dry run by default; real publishing needs the typed phrase `PUBLISH <n> ISSUES`.

**PT:** `scripts/publish-issues.sh` publica os rascunhos de `docs/issues/` (N01–N20) com o `gh`, pela conta do próprio
mantenedor. É dry-run por padrão; a publicação real exige a frase digitada `PUBLISH <n> ISSUES`.

## Behaviour / Comportamento
**EN**
- Duplicates: by a hidden marker `<!-- draft-id: N05 -->` that the script adds to each issue it creates, by exact title,
  and by a normalised title (ignores case, spaces, punctuation and the `[P1]` prefix). Running it twice creates nothing.
- Refuses a draft that has an empty title or body, a title over 256 characters, a token-like string, or a mention of an
  AI tool or assistant.
- Title and body go to `gh` as one argument and as a file, never through the shell: Markdown and special characters
  arrive unchanged (tested with quotes, `$`, backticks, `|`, `<tag>`).
- Stops at the first failed creation (no blind retries, which could duplicate), waits `--delay` seconds between
  creations, and prints what was created on Ctrl-C. It never prints environment variables or tokens.
- Labels only with `--with-labels`, and only the ones that already exist in the repository.

**PT**
- Duplicatas: por um marcador invisível `<!-- draft-id: N05 -->` que o script põe em cada issue que cria, pelo título
  exato e pelo título normalizado (ignora caixa, espaços, pontuação e o prefixo `[P1]`). Rodar duas vezes não cria nada.
- Recusa rascunho com título ou corpo vazio, título com mais de 256 caracteres, texto com cara de token ou menção a
  ferramenta/assistente de IA.
- Título e corpo vão para o `gh` como um argumento e como arquivo, nunca pelo shell: Markdown e caracteres especiais
  chegam sem alteração (testado com aspas, `$`, crases, `|`, `<tag>`).
- Para na primeira falha ao criar (sem repetir às cegas, o que poderia duplicar), espera `--delay` segundos entre as
  criações e mostra o que foi criado no Ctrl-C. Nunca imprime variáveis de ambiente nem tokens.
- Rótulos só com `--with-labels`, e só os que já existem no repositório.

## Tests / Testes
`tests/scripts/publish_issues_test.sh`: **31 verificações** com um `gh` **falso**. `make test` e `make lint` passam.

## Limits / Limites
- **Nunca rodou contra o GitHub de verdade.** Os testes provam o comportamento contra o falso. Antes do primeiro uso
  real: rodar sem `--publish` e conferir a lista; publicar **uma** (`--only N05`) e olhar o resultado.
- O limite de taxa da API e o comportamento do Ctrl-C **no meio** de uma chamada ao `gh` não foram exercitados.
- A detecção por marcador só reconhece issues criadas por este script; issues antigas são comparadas só por título.
- Só deve ser usado depois do merge do #14 (é ele que traz `docs/issues/`) e da revisão dos rascunhos.

Tudo é DEMO/TESTNET.
```

---

## 6. Decisão do PR #14 / PR #14 decision

Conferido contra o diff completo (`cd02a59` + base `86cda6f`): 24 arquivos, **só documentação** (`ROADMAP.md`,
`docs/PROJECT_AUDIT.md`, `docs/IMPLEMENTATION_PLAN.md`, `docs/issues/*`). CI `linux` e `macos` verdes.

| Pergunta | Resposta | Como foi conferido |
|---|---|---|
| Documentação coerente com o código atual? | **Em geral sim**, com **duas correções** (abaixo) | Achados F-01…F-11 conferidos com os mesmos números de linha contra `86cda6f`; F-02, F-03 e F-04 já foram **reproduzidos** e corrigidos em PRs separados |
| N01–N20 são propostas, não issues publicadas? | **Sim.** Nenhuma foi publicada | Listagem de issues do repositório: só #1–#11 existem |
| Duplicam as issues #1–#11? | **Não** | Cada N foi comparada com os títulos de #1–#11; o `docs/issues/README.md` lista as relações (#2, #8, #9, #10, #11, #4 reaproveitadas) |
| Critérios de aceite verificáveis? | **Sim**, com ressalva | As 20 têm "Critérios de aceite" e 8 a 14 tarefas em caixas; os de hardware dependem de evidência arquivada |
| O roadmap promete algo de hardware sem teste? | **Não** | Marcos por critério, sem datas; boot, display, toque e recuperação estão `NÃO VERIFICADO`; "Fora do roteiro" lista Pix real etc. |
| Há menção a ferramenta de IA nos arquivos? | **Não** nos documentos do PR | busca textual (o `LICENSE` pré-existente tem a palavra "Generated" em contexto jurídico, não relacionada) |

**Duas correções que ficam para o PR de manutenção (`docs/github-maintenance-review`):**
1. **N15 (rascunho):** dizia que `load_state()` deve ficar idempotente "guardando já carregado". Isso estava errado: o
   `ui_init` zera `M` e `PIN`, então o segundo carregamento é necessário. O rascunho foi corrigido neste ramo.
2. **Estado "ainda não corrigido":** o roadmap e a auditoria descrevem o repositório em `86cda6f`. N04, N05, N14 e N15
   já têm correção em PRs/branches separados; o PR de manutenção registra isso sem alterar o que a auditoria viu.

**Recomendação / Recommendation: `APPROVE` para fazer o merge**, depois de você ler o PR #14 na íntegra, porque é só
documentação, o CI está verde e as afirmações têm nível de evidência explícito. **Não faço o merge.**

**Texto de revisão / Review text (para colar)**
```markdown
**EN:** Reviewed the full diff (24 files, documentation only). The audit separates what was read in the code, what was
tested on the host, and what is NOT VERIFIED on the phone, and the roadmap promises no hardware behaviour. N01–N20
are drafts, not published issues, and none duplicates #1–#11. Two statements need correcting (the N15 "load once"
idea; the audit describes commit 86cda6f, so N04/N05/N14/N15 now have fixes elsewhere) — both are handled in the
follow-up maintenance PR. Recommend merge. CI is green on Linux and macOS.

**PT:** Revisei o diff completo (24 arquivos, só documentação). A auditoria separa o que foi lido no código, o que foi
testado no computador e o que está NÃO VERIFICADO no aparelho, e o roadmap não promete comportamento de hardware.
N01–N20 são rascunhos, não issues publicadas, e nenhuma duplica #1–#11. Duas afirmações precisam de correção (a ideia
de "carregar uma vez" da N15; a auditoria descreve o commit 86cda6f, então N04/N05/N14/N15 já têm correção em outros
PRs) — as duas estão tratadas no PR de manutenção seguinte. Recomendo o merge. CI verde no Linux e no macOS.
```

---

## 7. Correção no texto do PR #13 / Fix to the PR #13 text

O texto atual do PR #13 tem uma frase **incorreta**: *"O contador de tentativas ainda não é persistente (#9)"* /
*"The failed-attempt counter is still not persistent (#9)"*. Pelo código ele **é** gravado com o estado a cada
tentativa quando a área de estado está armada (`plat_save()` logo depois de `pin_check()`); fica só em memória na
imagem somente leitura. O que falta é gravar **antes** da conferência e proteger contra rollback (#9).

Troque a linha de "Honest limits" por:
```markdown
- The failed-attempt counter is saved with the state after every try, but only when the state area is armed, and after the check (cutting power during the check can lose a try). The state file is not authenticated, so someone with storage access can reset it (#9).
```
e a versão em português por:
```markdown
- O contador de tentativas é gravado com o estado a cada tentativa, mas só quando a área de estado está armada, e depois da conferência (cortar a energia durante a conferência pode perder uma tentativa). O arquivo não é autenticado, então quem acessa o armazenamento pode zerá-lo (#9).
```

Acrescente também ao PR #13 (resultado da auditoria):
```markdown
## Audit follow-up / Seguimento da auditoria
**EN:** An audit of this PR found and fixed two issues, each with a regression test: the failed-try counter, `set`
and `locked_until` read from the (unauthenticated) state file were not validated (a counter of INT_MAX overflowed a
signed int; a negative one gave unlimited tries), and the typed PIN stayed in the UI buffers. The state key is derived
but **nothing uses it yet**: the saved state is not encrypted (#9). Not an independent review; not measured on the phone.
See `docs/review/PR13_CRYPTO_AUDIT.md`.

**PT:** Uma auditoria deste PR achou e corrigiu dois problemas, cada um com teste de regressão: o contador de erros, o
`set` e o `locked_until` lidos do arquivo de estado (não autenticado) não eram validados (contador INT_MAX estourava um
int com sinal; um negativo dava tentativas ilimitadas), e o PIN digitado ficava nos buffers da interface. A chave do
estado é derivada, mas **nada a usa ainda**: o estado gravado não é cifrado (#9). Não é revisão independente; não foi
medido no aparelho. Ver `docs/review/PR13_CRYPTO_AUDIT.md`.
```

---

## 8. Atualização da issue #2 / Update for issue #2 (não fechar / do not close)

```markdown
## Status update / Atualização de estado

**EN**
- PR #13 (scrypt PIN verifier + separate state key + threat model) is open, CI green on Linux and macOS.
- An audit of the PR found and fixed two defects with regression tests (counters read from the state file were not
  validated; the typed PIN stayed in UI buffers). Report: `docs/review/PR13_CRYPTO_AUDIT.md` (decision: request
  changes → fixed on the branch; needs an independent review).
- Host evidence: RFC 7914 / 4231 / 5869 vectors, 850 differential cases against OpenSSL, 76 checks in `sec_test`,
  ASan/UBSan, and a fuzz target for the PIN state parser (about 19.7M executions in 30 s, no failures).
- **Not done:** cost measured on the Infinix Hot 30i X669C; independent review; behaviour when going back to an older
  firmware (an old build refuses the new `P2` line and offers to create a new PIN, reproduced on the host); the
  saved state is not encrypted yet, so the derived state key protects nothing today (#9).
- Open until: PR #13 is merged **and** the cost is measured on the phone **and** the downgrade behaviour is decided
  (with #8/#9). A 6-digit PIN does not resist an offline attack with any KDF; that is documented, not solved.

**PT**
- O PR #13 (verificador scrypt do PIN + chave de estado separada + modelo de ameaças) está aberto, CI verde no Linux e no macOS.
- A auditoria do PR achou e corrigiu dois defeitos com teste de regressão (contadores lidos do estado não eram
  validados; o PIN digitado ficava nos buffers da interface). Relatório: `docs/review/PR13_CRYPTO_AUDIT.md`
  (decisão: pedir mudanças → corrigido na branch; falta revisão independente).
- Evidência no computador: vetores RFC 7914 / 4231 / 5869, 850 casos comparados com o OpenSSL, 76 verificações no
  `sec_test`, ASan/UBSan e um alvo de fuzz do parser do estado do PIN (cerca de 19,7 milhões de execuções em 30 s, sem falhas).
- **Falta:** medir o custo no Infinix Hot 30i X669C; revisão independente; comportamento ao voltar para um firmware
  antigo (um build antigo recusa a linha `P2` e oferece criar um PIN novo, reproduzido no computador); o estado
  gravado ainda não é cifrado, então a chave de estado derivada hoje não protege nada (#9).
- Fica aberta até: o PR #13 ser mesclado **e** o custo ser medido no aparelho **e** o comportamento no downgrade ser
  decidido (com #8/#9). PIN de 6 dígitos não resiste a ataque offline com nenhum KDF; está documentado, não resolvido.
```

---

## 9. Atualização da issue #10 / Update for issue #10 (não fechar / do not close)

Já existe um comentário seu/meu na #10 dizendo "≈ 70%". **Substitua por este**, que troca a estimativa por uma lista:

```markdown
## Status update / Atualização de estado

**EN:** "About 70%" was an estimate, not an acceptance criterion. Here is the exact list.

Done and verified (host / CI):
- [x] `Makefile` with `test`, `test-asan`, `lint`, `lint-strict`, `fuzz`; `CONTRIBUTING.md` explains how to run them.
- [x] GitHub Actions on Linux and macOS (macOS skips the Linux-only tests and lint of Linux-only files).
- [x] ASan + UBSan builds of every test.
- [x] Own fuzz driver with a fixed seed (reproducible), and an optional libFuzzer build; targets for the USB command
      parser and the money engine, plus the PIN state parser (#13) and the cmdline parser (N14 branch).
- [x] Bugs found by this work, fixed with regression tests (NUL after the command, out-of-bounds read on a lone `T`
      line, unset PIN written in a form it refused to read back, `memcpy(NULL, 0)`, fd leak, shared cmdline buffer).

Still missing — this is what must be done before closing:
- [ ] CI hardening: no dependency cache, actions pinned by tag (`@v4`) and not by commit SHA, fuzz runs only on Linux for 20 s, no artifacts or hashes published.
- [ ] ARM64 build of the phone binary in CI (N18). Today **only the host is built**; the ARM64 build is NOT VERIFIED here.
- [ ] Fuzz targets for the state store (`store.c`) and for the image tools (`mkboot.py`, `mkramdisk.py`).
- [ ] A scheduled (nightly) longer fuzz run, with the crashing inputs saved as corpus files.
- [ ] A coverage report, and a rule for the minimum.
- [ ] Decide what to do with the PR branches that stack on each other (merge order in `docs/GITHUB_MAINTENANCE_REVIEW.md`).

**PT:** "Cerca de 70%" era uma estimativa, não um critério de aceite. Aqui está a lista exata.

Feito e verificado (computador / CI):
- [x] `Makefile` com `test`, `test-asan`, `lint`, `lint-strict`, `fuzz`; o `CONTRIBUTING.md` explica como rodá-los.
- [x] GitHub Actions no Linux e no macOS (o macOS pula os testes só de Linux e o lint de arquivos só de Linux).
- [x] Builds com ASan + UBSan de todos os testes.
- [x] Driver de fuzz próprio com semente fixa (reproduzível) e build opcional com libFuzzer; alvos do parser do comando
      USB e do motor de dinheiro, mais o parser do estado do PIN (#13) e o parser da cmdline (branch N14).
- [x] Bugs achados por este trabalho, corrigidos com teste de regressão (NUL depois do comando, leitura fora do
      limite numa linha `T` sozinha, PIN não definido gravado de um jeito que não era lido de volta, `memcpy(NULL, 0)`,
      vazamento de descritor, buffer compartilhado da cmdline).

Ainda falta — é isto que precisa ser feito antes de fechar:
- [ ] Endurecer o CI: sem cache de dependências, actions fixadas por tag (`@v4`) e não por SHA de commit, fuzz só no Linux por 20 s, sem artefatos nem hashes publicados.
- [ ] Build ARM64 do binário do aparelho no CI (N18). Hoje **só o computador é compilado**; o build ARM64 é NÃO VERIFICADO aqui.
- [ ] Alvos de fuzz para o `store.c` e para as ferramentas de imagem (`mkboot.py`, `mkramdisk.py`).
- [ ] Fuzz agendado (noturno) mais longo, guardando as entradas que quebram como corpus.
- [ ] Relatório de cobertura e uma regra de mínimo.
- [ ] Decidir o que fazer com as branches de PR empilhadas (ordem de merge em `docs/GITHUB_MAINTENANCE_REVIEW.md`).
```

**Observação / Note:** a lista do CI foi conferida no `ci.yml` da `main` em `86cda6f`.
