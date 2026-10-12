# Textos prontos para o GitHub, rodada 3 / Ready-to-paste GitHub texts, round 3

**Tudo é DEMO / TESTNET. / Everything is DEMO / TESTNET.** Estes textos são para **você** copiar. Nada foi publicado, nenhum PR foi aberto, nenhuma issue foi fechada.
Cada texto diz o que foi provado no computador (x86_64) e o que **não** foi: nada rodou no X669C. Convenção do repositório: título em inglês e português separados por ` / `.

**Ordem de mesclagem:** 1 → 2 → 3 (empilhadas), depois 4, 5, 6, 7, 8 em qualquer ordem. Para 2 e 3: abra o PR depois que o anterior for mesclado (a diferença cai sozinha) ou aponte a base para a branch anterior.

---

## 1. `fix/money-engine-overflow-limits` (`dbee845`, base `main`)

**Título**
```
fix(money): keep every swap product inside int64 and recompute the quote in the engine / mantém todo produto do swap dentro de int64 e recalcula a cotação no motor (N11)
```
**Descrição**
```markdown
## Resumo / Summary
**EN:** `m_quote` multiplied the caller's amount by the rate without a limit (signed overflow, undefined behaviour in C), `m_total_brl` multiplied balances by 560, the engine could create a swap whose output the loader refuses (the whole ledger disappears at the next boot), and `m_prepare_swap` trusted the quote the caller built. Now the amount is checked against a functional limit **before** the multiplication (`swap_calc`), the products are split so they cannot overflow, and `m_prepare_swap` recomputes the quote.
**PT:** `m_quote` multiplicava o valor sem limite (estouro de inteiro com sinal, comportamento indefinido), `m_total_brl` multiplicava saldos por 560, o motor podia criar um swap cuja saída o carregador recusa (o livro-razão some no boot seguinte) e `m_prepare_swap` confiava na cotação montada pelo chamador. Agora o valor é checado contra um limite funcional **antes** de multiplicar, os produtos são divididos e a cotação é recalculada.

## Reprodução na main (`32932dd`, UBSan)
- `m_quote(A_BRL, A_USDC, 9223372037205, …)` → `signed integer overflow: 9223372036855 * 1000000`
- `m_quote(A_USDC, A_BRL, 16470307208669243, …)` → `… * 560`
- 17 financiamentos de 1e15 micro-USDC (aceitos pelo carregador) → `m_total_brl`: `17000000000000000 * 560`
- `m_quote(A_BRL, A_USDC, 600000000000)` → saída 1071428570803571 (> 1e15) → `m_deserialize` recusa
- `m_prepare_swap` com cotação forjada (`from_amt` negativo, `to_amt` inflado, validade de 1 ano, BRL→BRL) → todas autorizadas

## Evidência
- `money_test`: 93 → 124 verificações (limites, fechamento, 13 cotações forjadas); falham no código antigo.
- `fuzz_money` com valores de fronteira, extremos de `int64` e mutação de cotação; com a correção removida o fuzz aborta.
- `make test`, `make test-asan`, `make lint` passam.

## Alcance honesto
O estouro de `m_quote` é alcançável hoje **só por chamada direta à API** (a UI limita os dígitos); o de `m_total_brl` é alcançável por um estado com CRC válido (o CRC não autentica). Foi corrigido porque a API do motor é pública.

## Não verificado
Nada rodou no aparelho nem em ARM64. Sem mudança criptográfica.
```

## 2. `fix/money-engine-clock-range` (`aa94316`, base: o PR 1)

**Título**
```
fix(money): refuse a clock the engine cannot store and cap a Pix at the sanity limit / recusa relógio que o motor não sabe gravar e limita o Pix (N11)
```
**Descrição**
```markdown
## Resumo / Summary
**EN:** The engine accepted any `now`. A value outside 0..1e11 was stored and the loader then refused the line, losing the ledger; `now ± constant` overflowed at the extremes. `m_prepare_pix` also had no cap of its own. Every entry point now validates the clock first ("Clock invalid.") and refuses a Pix above `MAX_AMOUNT`.
**PT:** O motor aceitava qualquer `now`. Fora de 0..1e11 o valor era gravado e o carregador recusava a linha, perdendo o livro-razão; `now ± constante` estourava nos extremos. `m_prepare_pix` também não tinha teto próprio. Agora toda entrada valida o relógio e recusa Pix acima de `MAX_AMOUNT`.

## Reprodução na main
`m_receive_pix(5000, "x", 100000000001)` → o carregador recusa ("data fora da faixa"); 4 × `m_receive_pix(1e15)` + política gigante + `m_prepare_pix(2.5e15)` → tx que o carregador recusa.

## Evidência
`money_test` 124 → 217 (8 relógios inválidos × 6 verificações, Pix no teto); fuzz com relógios de borda; mutação conferida. `make test`, `test-asan`, `lint` passam.

## Não verificado / Fora do escopo
Não resolve o domínio do relógio (relógio real × uptime, O-2/O-3): isso depende da #9. Nada rodou no aparelho.
```

## 3. `fix/money-swap-pending-table` (`e7f776d`, base: o PR 2)

**Título**
```
fix(money): the pending-swap table reports when full, resets with the ledger and frees every slot / a tabela de cotações pendentes avisa quando enche, é zerada com o livro-razão e libera todas as vagas (N11)
```
**Descrição**
```markdown
## Resumo / Summary
**EN:** The static table holds 8 pending quotes. The 9th swap silently got no slot and "expired" at once; after 8 abandoned swaps and `m_init`, every new swap failed until the process restarted. Now a full table is reported, `m_init` clears it, and expired or finished slots are returned.
**PT:** A tabela estática guarda 8 cotações pendentes. O 9º swap ficava sem vaga e "expirava" na hora; depois de 8 abandonados e `m_init`, todo swap novo falhava até reiniciar. Agora a tabela cheia é avisada, o `m_init` a zera e as vagas vencidas ou concluídas voltam.

## Reprodução na main
9 × `m_quote` + `m_prepare_swap`, autorizar o 9º → `Quote expired. Nothing was moved.` (alcançável pelo fluxo normal da UI: dois verificadores reproduziram).

## Evidência
`money_test` 217 → 228 (10 verificações, 2 mutações). `make test`, `test-asan`, `lint` passam.

## Não corrigido
O-1: swap acima de R$ 5.000 nunca é autorizado porque a espera (30 s) é maior que a validade da cotação (20 s). É decisão de produto; fica para uma issue própria.
```

## 4. `fix/cmdline-truncation-fails-closed` (`9a68db0`)

**Título**
```
fix(init): the write lock fails closed when the cmdline is cut or unreadable / a trava de escrita falha fechada quando a cmdline é cortada ou ilegível (N14)
```
**Descrição**
```markdown
## Resumo / Summary
**EN:** `bankphone.ro=1` was looked up in a fixed 4096-byte buffer. With a `/proc/cmdline` of 4097 bytes or more, the token at the end was cut off and the system ran **writable** (reproduced with a file standing in for `/proc/cmdline`). A long state-partition name was silently truncated too. Now a truncated or unreadable cmdline keeps the image read-only, and a name that does not fit is refused. **Honest reach:** no real path produces such a cmdline today (`mkboot.py` refuses 1536 bytes or more, the arm64 kernel limits it to 2048), so this is defense in depth, not an exploitable hole; a write lock should fail closed anyway.
**PT:** `bankphone.ro=1` era procurado num buffer fixo de 4096 bytes. Com `/proc/cmdline` de 4097 bytes ou mais, o token do fim era cortado e o sistema subia **gravável**. **Alcance honesto:** hoje nenhum caminho real gera uma cmdline assim (o `mkboot.py` recusa 1536 bytes ou mais, o kernel arm64 limita a 2048); é defesa em profundidade, não brecha explorável. Um nome de partição de estado longo também era truncado em silêncio. Agora cmdline cortada ou ilegível mantém somente leitura, e nome que não cabe é recusado.

## Evidência
`cmdline_test` 36 → 59 (cena com 4097 bytes, leitura vazia, truncamento exato); `test_store` bloco [17] para nomes longos. `make test`, `test-asan`, `lint` passam.

## Não verificado
HW-19: cmdline longa no aparelho. Nenhuma gravação feita.
```

## 5. `fix/store-never-overwrite-only-good-slot` (`acdef27`)

**Título**
```
fix(store): a slot with a bad payload is not a good slot / um slot com payload ruim não é um slot bom
```
**Descrição**
```markdown
## Resumo / Summary
**EN:** `slot_ok` was set from the header alone. A slot with a good header and a corrupted payload counted as "good", so `store_save` could overwrite the only slot that was still readable. `slot_ok` now means header **and** payload CRC verified.
**PT:** `slot_ok` vinha só do cabeçalho. Um slot com cabeçalho bom e payload corrompido contava como bom e o `store_save` podia sobrescrever o único slot legível. Agora `slot_ok` significa cabeçalho **e** CRC do payload conferidos.

## Evidência
`test_store` 171 → 257 (bloco [15b], com injeção de falhas); reproduzido na `main` antes da correção. `make test`, `test-asan`, `lint` passam.

## Não verificado
HW-08 (queda de energia real). Nada rodou no aparelho.
```

## 6. `fix/pin-lockout-bounded-clock` (`9e74f13`)

**Título**
```
fix(sec): a saved PIN lockout can never outlast the longest real one / um bloqueio de PIN salvo nunca dura mais que o maior bloqueio real
```
**Descrição**
```markdown
## Resumo / Summary
**EN:** The lockout deadline is saved as an absolute time. When `plat_now()` switches between the wall clock (~1.7e9) and uptime (seconds), a deadline saved in one domain is read in the other and locks the phone for decades (55.8 years reproduced). The failed-PIN counter was a plain `int` incremented without a bound. Now the deadline is anchored once and capped (1920 s), an out-of-range clock returns the maximum (never zero), and the counter saturates.
**PT:** O prazo do bloqueio é salvo como tempo absoluto. Quando o `plat_now()` troca entre relógio real e uptime, o prazo salvo num domínio é lido no outro e trava o aparelho por décadas (55,8 anos reproduzidos). O contador de erros era um `int` sem limite. Agora o prazo é ancorado uma vez e limitado a 1920 s, relógio fora de faixa devolve o máximo e o contador satura.

## Evidência
`sec_test` 7 → 27; mutação conferida. `make test`, `test-asan`, `lint` passam.

## Atenção
Esta branch toca `init/sec.c`, como o #13. Vai conflitar com ele; resolver mesclando a `main` na branch. **Não** altera nada de criptografia. Nada rodou no aparelho (HW-15, HW-16).
```

## 7. `fix/ui-input-and-display` (`262cbdb`)

**Título**
```
fix(ui): USDC amounts no longer wrap, the tab bar survives a long ledger, the PIN sheet shows its lockout / valores em USDC não estouram, a barra de abas sobrevive a um livro-razão longo e a folha de PIN mostra o bloqueio
```
**Descrição**
```markdown
## Resumo / Summary
**EN:** (1) the typed USDC amount was an `int`: 6 to 9 digits wrapped; (2) `approx_usdc` overflowed with a large total; (3) the touch-hit table was too small: with ~215 ledger rows the tab bar and the gear stopped responding; (4) the PIN sheet did not show the lockout countdown; (5) a `stride=0` was dropped.
**PT:** (1) o valor digitado em USDC era `int`: 6 a 9 dígitos davam a volta; (2) `approx_usdc` estourava com total grande; (3) a tabela de toques era pequena: com ~215 linhas, a barra de abas e a engrenagem deixavam de responder; (4) a folha de PIN não mostrava a contagem do bloqueio; (5) `stride=0` era descartado.

## Evidência
`ui_test` (novo): 40 verificações dirigindo o `ui.c` real com plataforma falsa; os candidatos foram reproduzidos por dois verificadores independentes. `make test`, `test-asan`, `lint` passam.

## Não verificado
HW-17, HW-18. A tela real do X669C não foi vista. Se o #15 for mesclado antes, o `plat_random` do `ui_test` precisa de ajuste.
```

## 8. `test/image-tools` (`4744fae`)

**Título**
```
test: tests for mkboot.py and mkramdisk.py / testes para mkboot.py e mkramdisk.py
```
**Descrição**
```markdown
## Resumo / Summary
**EN:** The tools that build the image written to the phone had no test. 35 checks cover header fields, alignment, ramdisk packing and refusal of bad input. No production change.
**PT:** As ferramentas que montam a imagem gravada no aparelho não tinham teste. 35 verificações cobrem campos do cabeçalho, alinhamento, empacotamento do ramdisk e recusa de entrada inválida. Nenhuma mudança de produção.

## Não verificado
Gravar a imagem num aparelho continua sendo teste manual (HW-01).
```

---

## 9. Para as issues (**não** publicar nem fechar nada)

- **#9 (estado e relógio):** acrescentar como critério O-2 e O-3 e o resumo de `docs/STATE_INTEGRITY.md`. **Não fechar.**
- **#11 (integração real):** acrescentar O-4 (gravar a intenção antes de executar, com chave de idempotência). **Não fechar.**
- **Nova issue sugerida (O-1):** "Swap acima de R$ 5.000 nunca é autorizado (espera 30 s > validade 20 s)". Três opções: renovar a cotação ao fim da espera, validade maior só nesse nível, contar a espera de outro jeito. Aceite: um swap de R$ 6.000 completa em `created + 31`.
