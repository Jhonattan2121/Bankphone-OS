# [P2] Política do swap acima de R$ 5.000: limite, cotação, espera e revalidação (decisão de produto)

**RASCUNHO. Não publicado.** Gerado na rodada 4; só vira issue se o mantenedor decidir.
**Labels sugeridos:** `priority:P2`, `area:finance`, `type:decision`, `needs:product-decision`
**Tipo:** decisão de produto + especificação + testes de fronteira. **Não é classificado como bug.**
**Precisa do Infinix Hot 30i:** não
**EN:** Product decision: what a swap above R$ 5,000 should do. Today it can never complete. The fix is a policy choice, not a longer quote lifetime.

## O que o código faz hoje (evidência [A] código lido e [B] teste de host na `main` `a384698`)

| Fato | Onde |
|---|---|
| Faixas de autorização pelo valor em BRL: ≤ R$ 500 → `L_PIN`; ≤ R$ 5.000 → `L_CONFIRM`; acima → `L_COOLDOWN` | `m_required`, `init/money.c:191`; política em `:76` (`pin_max=50000`, `confirm_max=500000`) |
| `L_COOLDOWN` exige **PIN + confirmação explícita + espera de 30 s** contados da **criação** da transação | `m_authorize`, `init/money.c:201`; `cooldown_s = 30` em `:76` |
| A cotação de um swap vale **20 s** a partir do `m_quote` | `QUOTE_TTL`, `init/money.c:10`; `pend_set`/`pend_get` |
| `m_authorize` confere a espera **antes** da validade da cotação | ordem em `init/money.c:201-202` |
| Tetos por transação (R$ 2.000) e diário (R$ 5.000) valem **só para Pix de saída** | `m_prepare_pix`, `:130-131`; `m_prepare_swap` **não** os usa |
| O único teto do swap é `MAX_SWAP_BRL` (R$ 1 bi), um limite **de segurança aritmética**, não de política | `init/money.c` (rodada 3, #24) |

**Resultado reproduzido (rodada 3, dois verificadores independentes):** um swap acima de R$ 5.000 devolve "ESPERA: aguarde antes de autorizar." até 29 s e **"Quote expired. Nothing was moved." a partir de 30 s**. Nada é movido (falha para o lado seguro), mas o degrau mais alto **nunca completa**.

## Por que não é só "aumentar a validade da cotação"
Uma cotação que dura mais que a espera deixa o **preço parado** por 30 s ou mais, a favor de quem a pediu. Com uma fonte de preço real (#11) isso é exposição de câmbio. Estender o prazo "para o fluxo passar" é exatamente a mudança que esta issue **não** quer fazer sem decidir a regra.

## Lacuna de política que a análise revelou (decidir junto)
Como `m_prepare_swap` não tem teto por transação nem diário, e a espera só vale acima de R$ 5.000, **hoje nada impede muitos swaps de até R$ 5.000 cada** (com PIN + confirmação) em sequência. O degrau "acima de R$ 5.000" é inalcançável, mas a **soma** não é limitada. Isto precisa de uma decisão: o valor total por dia em swap deve ter teto?

## Opções (nenhuma implementada)

| Opção | O que faz | Prós | Contras |
|---|---|---|---|
| **A. Cotar de novo ao fim da espera** | A espera de 30 s corre sobre a **intenção** (valor e sentido). Terminada a espera, o sistema gera uma cotação **nova**, mostra o preço novo e exige confirmação dentro da validade dela | Preço sempre fresco; mantém a espera como fricção | Duas confirmações; o preço pode mudar entre a intenção e a execução (tem que ser dito na tela) |
| **B. Teto duro por swap = R$ 5.000** (e diário) | Acima disso o motor recusa com mensagem clara | Simples, sem beco sem saída, sem preço parado | Remove o degrau de valores altos; precisa de caminho (outra função) se eles forem desejados |
| **C. Espera como "cofre" (#7)** | Valores acima do teto viram um pedido com **janela de cancelamento**, não um swap imediato | Alinha com a issue #7; mais seguro | Maior; fora do escopo de um ajuste |
| **D. Validade maior só neste degrau** | Mantém um único fluxo | Pequeno | **Não recomendado**: preço parado; é a mudança que se quer evitar |

**Recomendação do autor do rascunho:** **B agora** (teto por swap e diário, sem beco sem saída, fácil de testar) e **A** quando houver fonte de preço real; **C** como evolução. É uma recomendação, **a decisão é do mantenedor**.

## Regras que valem em qualquer opção
1. **Revalidar no momento de autorizar**: preço (o motor já recalcula a cotação, #24), teto por swap, teto diário e saldo. Nada decidido na criação vale cegamente na execução.
2. **Expirou = nada se move**: a transação vai para `EXPIRED`, a concessão do PIN é consumida e a mensagem diz "nada foi movido".
3. **Nunca recotar em silêncio a um preço pior**: se o preço mudou, a pessoa vê o novo e confirma de novo.
4. **Janela de cancelamento** visível durante a espera.
5. **Relógio:** a espera e a validade usam o relógio da plataforma; com relógio inválido a operação é recusada (já é assim, #23). Interação com os limites diários depende da issue #9 (`docs/review/ISSUE9_CLOCK_AND_ARM64_CI.md`).

## Testes de fronteira (esperado depende da opção escolhida; a tabela fixa as **entradas**)

| Entrada | Valores | O que checar |
|---|---|---|
| Valor do swap (centavos) | 50.000, 50.001, 500.000, 500.001, 500.001 + taxa, `MAX_SWAP_BRL`, `MAX_SWAP_BRL + 1`, 0, negativo | degrau de autorização correto e recusa dos inválidos |
| Tempo desde a criação | 0, 29, 30, 31 s | a espera termina exatamente aos 30 s |
| Idade da cotação | 19, 20, 21 s | expira exatamente após 20 s |
| Concessão do PIN | 119, 120, 121 s (`AUTH_TTL_S`) | concessão vence e é de uso único |
| Relógio | `-1`, `0`, `NOW_MAX`, `NOW_MAX + 1`, relógio que volta | recusa e nada muda |
| Cotação adulterada | `from_amt`, `to_amt`, `rate`, `fee_brl`, `expires` | "Quote invalid. Nothing was moved." |
| Somas | N swaps de R$ 5.000 no mesmo dia | o teto diário (se escolhido) vale |
| Tabela de pendentes | 8, 9 swaps abandonados | "Too many swaps waiting" e vaga devolvida |

**Critério de aceite (a ajustar pela opção):** um swap acima do degrau mais baixo **ou completa em um tempo definido e testado, ou é recusado com mensagem clara** (nunca fica num estado que não pode terminar); os limites e o relógio têm teste de fronteira; nenhum limite sobe só para o fluxo passar.
