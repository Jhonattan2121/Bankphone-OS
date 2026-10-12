# Invariantes do motor financeiro e matriz de testes / Money engine invariants and test matrix

**Tudo é DEMO / TESTNET. / Everything is DEMO / TESTNET.** Nenhuma chamada financeira real existe neste código (ver I-12).

**EN summary.** This matrix lists every rule the money engine (`init/money.c`) must keep, where the code enforces it, which test
proves it, and what is still open. It was built from reading the code, from an independent sweep of every arithmetic site (five
finders, each finding re-checked by two skeptics that had to reproduce it), and from reproductions run **on commit `32932dd`** before any
fix. The rows marked **FIXED (branch)** have a fix on a branch that is **not merged yet**; on `main` they are still defects.
Everything is host evidence (Linux x86_64 container). Nothing was verified on the Infinix Hot 30i X669C.

Níveis de evidência / evidence levels: **[A]** lido no código · **[B]** teste ou reprodução no computador · **[C]** aparelho (nenhum) · **[D]** planejado.

## 1. Como cada defeito foi estabelecido / how each defect was established

Todo defeito abaixo foi **reproduzido em `32932dd`** com um programa que usa a API pública do motor ou o carregador, com UBSan (`-fsanitize=undefined
-fno-sanitize-recover=undefined`) ou comparando o resultado. A reprodução vem **antes** do teste de regressão, e o teste de regressão **falha no código
antigo** (verificado nos casos marcados "mutação"). Onde um achado dos finders independentes não foi reproduzido por mim, a coluna diz "relatado".

## 2. Matriz

| ID | Invariante | Onde o código a impõe | Prova (teste) | Estado |
|---|---|---|---|---|
| **I-01** | O saldo é **derivado** só das transações `CONFIRMED` e nunca fica negativo | `m_balance`; o carregador recusa saldo final negativo | `money_test` (saldos), `fuzz_money` (`check_invariants`) | **Verificado [B]** |
| **I-02** | Idempotência: a mesma chave não cria duas transações; o carregador recusa chave repetida | `add()` / `find_idem`; `m_deserialize` | `money_test` ("reexecutar não duplica"), fuzz | **Verificado [B]** |
| **I-03** | Máquina de estados: um estado final (`CONFIRMED`, `FAILED`, `CANCELLED`, `EXPIRED`) nunca volta | `allowed()`, `advance()` | `fuzz_money` (estado final não muda) | **Verificado [B]** |
| **I-04** | Teto por Pix e teto diário valem | `m_prepare_pix` | `money_test` (limites), `fuzz_money` | **Verificado [B]** |
| **I-05** | Todo valor de entrada é `> 0` e `<= MAX_AMOUNT` (1e15) | `m_receive_pix`; **faltava** em `m_prepare_pix` | `money_test` "pix de MAX_AMOUNT + 1" | **FIXED (`fix/money-engine-clock-range`)** |
| **I-06** | **Nenhum produto ou soma sai do `int64`** (overflow com sinal é comportamento indefinido em C) | `m_quote`, `m_total_brl`, `brl_value`; `_Static_assert` | UBSan: `money_test`, `fuzz_money` alargado | **FIXED (`fix/money-engine-overflow-limits`)**; em `main` **não** |
| **I-07** | **Fechamento:** tudo que o motor grava, o próprio carregador aceita de volta (senão o livro-razão inteiro some no boot seguinte) | faixas do carregador × saídas do motor | ida e volta no fim do `fuzz_money`; `money_test` | **FIXED (3 branches de N11)**; em `main` **não** |
| **I-08** | A autorização exige uma concessão emitida pelo PIN, ligada a uma transação, de uso único e com validade | `auth_*` em `sec.c`, `m_authorize` | `money_test`, `fuzz_money` | **Verificado [B]** (N07, já na `main`) |
| **I-09** | A cotação de um swap só vale dentro da validade, e **o motor recalcula a cotação** em vez de confiar na do chamador | `swap_calc`, `m_prepare_swap` | 13 cotações forjadas; fuzz com mutação | **FIXED (`fix/money-engine-overflow-limits`)** |
| **I-10** | O motor só aceita um relógio que ele sabe gravar: `0 <= now <= MAX_TIME - validade da cotação` | `now_ok()` em cada entrada | 8 relógios inválidos × 6 verificações; fuzz | **FIXED (`fix/money-engine-clock-range`)** |
| **I-11** | A tabela de cotações pendentes tem limite conhecido, avisa quando enche, é zerada com o livro-razão e devolve toda vaga | `pend_*`, `m_init` | `money_test` (10 verificações, 2 mutações) | **FIXED (`fix/money-swap-pending-table`)** |
| **I-12** | **Separação DEMO:** nenhuma chamada financeira real; tudo rotulado DEMO | `grep` em `init/`: **nenhum** `socket`, `connect`, `getaddrinfo` ou URL de serviço (as únicas ocorrências de "bcb" são o *Bootloader Control Block* da partição `misc`, não banco) | busca textual [A]; comprovante `Hash on-chain: NENHUM (DEMO)` em `sheets.c` | **Verificado [A]** |
| **I-13** | O modo emergência bloqueia preparar e falha a autorização, sem deixar a concessão viva | `m_prepare_*`, `m_authorize` | `money_test` | **Verificado [B]** |
| **I-14** | Transação interrompida nunca fica "viva" depois de um reinício: `AWAITING_AUTH`, `QUOTED` e `CREATED` viram `EXPIRED` ao carregar | `m_deserialize` | `fuzz_money` (estado aceito nunca tem esses estados) | **Verificado [B]** |

## 3. Defeitos reproduzidos em `32932dd` (resumo com a evidência)

| # | Defeito | Entrada concreta | Resultado observado | Classe |
|---|---|---|---|---|
| D-1 | `m_quote` BRL→USDC estoura | `m_quote(A_BRL, A_USDC, 9223372037205, …)` | UBSan: `signed integer overflow: 9223372036855 * 1000000`; sem sanitizer vira `to_amt` negativo | I-06 |
| D-2 | `m_quote` USDC→BRL estoura | `m_quote(A_USDC, A_BRL, 16470307208669243, …)` | UBSan: `… * 560` | I-06 |
| D-3 | `m_total_brl` estoura num estado **aceito** pelo carregador | 17 linhas de financiamento de 1e15 micro-USDC | UBSan: `17000000000000000 * 560` | I-06 |
| D-4 | O motor cria um swap que o carregador recusa | `m_quote(A_BRL, A_USDC, 600000000000)` → saída `1071428570803571` (> 1e15) | `m_deserialize` recusa com "linha 5: valor fora da faixa": **o livro-razão some no reboot** | I-07 |
| D-5 | `m_prepare_swap` confia na `Quote` do chamador | `from_amt` negativo; `to_amt` = 1e12; validade de 1 ano; BRL→BRL | todas **AUTORIZADAS**; a primeira cria R$ 1.000 + 100 micro-USDC do nada | I-09 |
| D-6 | Um `now` fora de `[0, 1e11]` é aceito e gravado | `m_receive_pix(5000, "x", 100000000001)` | o carregador recusa a linha ("data fora da faixa") → ledger vazio; e `now ± constante` estoura nos extremos | I-10 |
| D-7 | `m_prepare_pix` sem teto próprio | 4 × `m_receive_pix(1e15)`, política gigante, `m_prepare_pix(2.5e15)` | tx com `from_amt` 2,5e15 que o carregador recusa | I-05/I-07 |
| D-8 | O 9º swap pendente "expira" na hora, sem motivo | 9 × `m_quote`+`m_prepare_swap`, autorizar o 9º | `Quote expired. Nothing was moved.` | I-11 |
| D-9 | Depois de 8 swaps abandonados e `m_init`, todo swap novo falha | idem, com `now` diferente | `Quote expired` até reiniciar o processo | I-11 |

### 3.1 Alcançabilidade: quem consegue disparar cada defeito / reachability

Cinco varreduras independentes (valores, fechamento motor→carregador, tempo, tamanhos, tela) levantaram candidatos; cada um foi entregue a dois
verificadores com a ordem de **refutar** (um reproduz o defeito no código da `main`, o outro procura um caminho real de entrada). O resultado muda a
**prioridade**, não o fato de o defeito existir:

| Defeito | Reproduzido | Alcançável por entrada real | Observação |
|---|---|---|---|
| D-3 `m_total_brl` e `approx_usdc` (estado aceito pelo carregador) | sim, 2/2 | **sim:** um arquivo de estado com CRC válido (o CRC não autentica) | O mais sério dos aritméticos: o estado passa pelo carregador e estoura na tela inicial |
| D-1 `m_quote` BRL→USDC, D-2 USDC→BRL | sim | **só por chamada direta à API** (a UI limita os dígitos digitáveis) | Corrigido porque a API do motor é pública e o próximo chamador não teria o limite da UI |
| D-4 saída do swap recusada pelo carregador | sim | só por chamada direta (um verificador reproduziu, o outro não achou caminho pela UI) | Idem |
| D-5 `m_prepare_swap` confia na cotação do chamador | sim | só por chamada direta; a UI monta a cotação a partir do `m_quote` | Defesa em profundidade, mas é a fronteira que protege o saldo |
| D-6 relógio fora da faixa | sim | **em parte:** o relógio real é limitado pela plataforma, mas o domínio muda (relógio/uptime) | Ver O-2 |
| D-7 Pix sem teto próprio | sim | só com a política elevada por código (hoje só os testes alteram) | Ver O-7 |
| D-8, D-9 tabela de cotações pendentes | sim, 2/2 | **sim:** 9 swaps iniciados e abandonados pela UI | Reproduzido pelo fluxo normal |
| O-1 espera de 30 s × validade de 20 s | sim, 2/2 | **sim:** fluxo normal de um swap acima de R$ 5.000 | Não corrigido (decisão de produto) |
| Sobrecarga da tabela de toques (`MAX_HITS`) e `approx_usdc` na UI | sim, 2/2 | **sim:** cerca de 215 entradas simuladas, ou estado com 17 financiamentos | Corrigido na branch de UI |
| Truncamento `int` do valor digitado em USDC | sim, 2/2 | **sim:** 6 a 9 dígitos na folha de swap em USDC | Corrigido na branch de UI |
| PIN: contador `int` gravado perto de `INT_MAX` | sim, 2/2 | **sim, com escrita no estado** (CRC válido, não MAC) | Corrigido na branch do PIN |
| PIN: prazo absoluto entre domínios de relógio | sim, 2/2 | **sim:** sem RTC e com persistência | Corrigido na branch do PIN |
| `now` em extremos do `int64` em `pin_check`, `pin_locked_s`, idade da concessão, validade da cotação, janela diária, espera | sim (só em chamada direta) | **não** por entrada real: exige `plat_now()` fora de faixa | Tornados inofensivos por validação do relógio; sem alegar que eram exploráveis |
| Cmdline maior que o buffer de 4096 bytes derruba `bankphone.ro=1` | sim (só com cmdline ≥ 4097 bytes) | **não por caminho real:** `mkboot.py` recusa cmdline ≥ 1536 bytes, o kernel arm64 limita a 2048, e quem injeta mais que isso já controla a cmdline | Correção mantida como **defesa em profundidade** (falhar aberto numa trava de escrita é o lado errado de errar); **não** é vulnerabilidade explorável hoje |
| Cotação aceita com o relógio andando para trás; Pix datado no futuro na janela | sim | parcial (domínio do relógio) | Ligado a O-2 e O-3; a #9 |

Três candidatos foram **rebaixados** (reproduzem só fora do alcance de qualquer entrada real): a soma diária com política acima de ≈ 4,6e18 (só com código
externo que altere `M.pol`), o agregado de swaps acima de 1e15 (exige chamada direta com valor que a UI não permite) e os campos forjados da cotação
(inalcançável pela UI). Eles continuam cobertos por teste porque a API é pública, mas **não** são tratados como vulnerabilidade explorável.
O workflow terminou (85 agentes, 0 erros); os vereditos acima são os finais.

## 4. Achados que **não** estão corrigidos, com a razão / findings NOT fixed, and why

| ID | Achado | Por que não foi corrigido aqui | Próximo passo |
|---|---|---|---|
| **O-1** | **Swap acima de R$ 5.000 nunca é autorizado:** a espera (cooldown) é 30 s, a validade da cotação é 20 s. Reproduzido por mim e por dois finders: até 29 s devolve "ESPERA", aos 30 s devolve "Quote expired". Falha para o lado seguro, mas é um beco sem saída | É decisão de **produto**: renovar a cotação ao fim da espera, aumentar a validade só para esse nível (preço parado por mais tempo), ou contar a espera de outro jeito. Corrigir sem escolher muda a regra financeira | Issue própria com as 3 opções e o teste de aceite "um swap de R$ 6.000 completa em `created + 31`" |
| **O-2** | **Teto diário contornável por troca de domínio do relógio:** o `plat_now()` alterna entre relógio real (≈ 1,7e9) e uptime (pequeno). Pix de 500000 centavos gastos em `now=500..520` deixam de contar quando o relógio passa a ser o real. Reproduzido pelo finder | Exige gravar **o domínio do relógio** com cada linha (ou um id de boot) e um relógio monotônico: é a issue **#9**. Um remendo local esconderia o problema | Registrar na #9 como critério |
| **O-3** | **Um Pix datado no futuro conta na janela de 24 h até o relógio alcançá-lo** (o relógio corrigido para trás bloqueia por anos) | Mesmo motivo de O-2 | #9 |
| **O-4** | **Ordem "executar na memória, depois gravar":** `m_authorize` confirma na memória e o `plat_save()` vem depois. Uma queda no meio perde a transação (o estado em disco volta ao anterior) | Para o provedor **MOCK** é inofensivo (nenhum dinheiro se move). Para um provedor **real** a ordem teria de ser *gravar a intenção, depois executar*, com chave de idempotência | Requisito para qualquer integração real (issue #11): documentado, não implementado |
| **O-5** | O valor em USDC é guardado com 6 casas mas a UI mostra 2: a tela e o ledger "não fecham" por frações de centavo (ex.: mostra `$ 17.23`, o ledger tem `17.232142`) | Decisão de produto: mostrar 6 casas ou arredondar a saída do swap para 2 casas | Issue de produto |
| **O-6** | Total acima de ≈ R$ 10 bi é cortado na tela (os dígitos passam da borda do cartão) | Limite de interface; o motor guarda certo | Reduzir a fonte conforme a largura, ou limitar o total exibido |
| **O-7** | Política (`M.pol`) é gravável por qualquer um e não tem teto próprio | Só os testes a alteram; com o teto de `MAX_AMOUNT` no `m_prepare_pix` o risco de soma estourar some | Um setter validado quando a política puder ser configurada |
| **O-8** | (relatado por um finder; **não reproduzi**) a UI volta da tela de revisão sem cancelar e deixa a transação em `AWAITING_AUTH` | Com a devolução das vagas vencidas (I-11) não trava mais, mas a linha fica na lista até o reboot | Cancelar ao voltar da tela de revisão |
| **O-9** | Texto de "quando" com valor negativo: um registro criado em outro boot sem relógio (carimbo de uptime, p. ex. 5000 s) é mostrado como `há -82 min` na aba Dinheiro e `em 83min de uptime` na Atividade; com relógio válido vira uma data de 1969. Reproduzido e alcançável (aparelho sem RTC, com persistência, após reinício) | Cosmético, sem overflow; a causa é a mesma da O-2 (o registro não diz em qual domínio de relógio foi criado) | Tratar junto com a #9; enquanto isso, mostrar "antes deste boot" quando `created > agora` |

## 5. Evidência de teste (computador)

| Alvo | Verificações antes → depois |
|---|---|
| `money_test` | 93 (`main`) → 124 (overflow) → 217 (relógio) → 228 (tabela de pendentes) |
| `sec_test` | 7 → 27 (bloqueio do PIN e relógio) |
| `cmdline_test` | 36 → 59 |
| `test_store` | 171 → 257 |
| `ui_test` (novo) | 0 → 40 |
| `image_tools_test` (novo) | 0 → 35 |
| Conjunto das 8 branches sobre a `main` | **779** verificações em `make test` e `make test-asan`, sem conflito de merge |

Fuzz (30 s por alvo, semente fixa): `fuzz_money` agora sorteia valores de fronteira e extremos do `int64`, relógios inválidos e cotações forjadas.
**Mutação:** com a correção removida, o fuzz aborta em segundos para o overflow, para a cotação forjada e para o relógio inválido.

## 6. O que continua sem prova

Nada disso rodou no aparelho. O fluxo de toques (`ui.c`) agora tem teste de host (`ui_test`), mas **não** foi exercitado no X669C.
Os testes de UBSan e ASan são de um container x86_64; **não** há prova em ARM64.
