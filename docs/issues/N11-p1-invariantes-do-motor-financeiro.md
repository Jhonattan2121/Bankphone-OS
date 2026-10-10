# [P1] Formalizar estados, invariantes, arredondamento e limites do motor financeiro (simulação)

**Labels sugeridos:** `priority:P1`, `area:finance`, `area:testing`, `type:feature`
**Tipo:** especificação + testes que faltam
**Precisa do Infinix Hot 30i:** não
**EN:** Formalize the money engine's states, invariants, rounding and limits (simulation only), and add the missing tests.

## Contexto
O motor financeiro (`init/money.c`) precisa ser confiável **como simulação** antes de qualquer integração com dinheiro real (#11). Hoje as regras existem no código e em testes, mas não estão escritas num só lugar, e algumas não são verificadas por teste.

## Estado atual
- Valores em centavos inteiros (BRL) e 1e-6 (USDC), sem ponto flutuante (`init/money.c:10`).
- Máquina de estados com transições permitidas (`init/money.c:14-26`): `CREATED`, `QUOTED`, `AWAITING_AUTH`, `AUTHORIZED`, `SIGNED`, `BROADCASTING`, `PENDING`, `CONFIRMED`, `FAILED`, `CANCELLED`, `EXPIRED`. Os estados finais não voltam.
- Saldo é **derivado** das transações `CONFIRMED` (`m_balance`, `init/money.c:48-52`).
- Idempotência por chave (`find_idem`, `add`).
- Tetos por Pix e diário, cotação com validade de 20 s, faixas de autenticação (`m_prepare_pix`, `m_required`).
- Cobertos por teste/fuzz: saldo nunca negativo, tetos, estado final não muda, salvar e recarregar mantém saldos (`tests/fuzz/fuzz_money.c`), 40 verificações em `tests/money_test.c`.
- **Não verificados:** chave de idempotência nunca aplicada duas vezes; cotação com mais de 20 s nunca aceita; ausência de estouro em `valor × taxa` (o fuzzer limita os valores ao que `parse_cents` devolve).

## Problema
1. As regras não estão documentadas num lugar só.
2. `m_receive_pix` aceita qualquer valor (inclusive zero ou negativo) porque confia no chamador (`parse_cents`).
3. A conta de troca trunca (divisão inteira) sem regra de arredondamento escrita (`init/money.c:91-100`).
4. O livro-razão tem capacidade fixa de 400 transações (`MAX_TX`) e responde "Ledger full." sem política de arquivamento.
5. `m_deserialize` não valida moeda, sinal nem faixa dos valores carregados (ver N15).
6. O tempo vem de `plat_now()`, que usa o relógio de parede se válido e **o tempo desde o boot** caso contrário (`init/main.c:846-847`). Se a base mudar entre boots, a janela diária (`created ≥ now − 86400`) e os prazos podem ser avaliados na base errada. **Hipótese H-7, não verificada.**

## Objetivo
`docs/MONEY_ENGINE.md` com estados, transições, invariantes, arredondamento e limites, e testes que verifiquem **cada invariante escrita**.

## Tarefas
- [ ] Documentar operações (receber, enviar, trocar, cancelar, expirar) e o diagrama de estados.
- [ ] Escrever as invariantes, uma por linha, cada uma com o teste que a verifica.
- [ ] Definir a regra de arredondamento da troca (para baixo, a favor de quem) e testar com valores de fronteira.
- [ ] Validar entradas no próprio motor: valor > 0 e abaixo de um máximo, moeda válida.
- [ ] Tratar overflow/underflow com aritmética conferida (`__builtin_*_overflow` ou equivalente) em `valor × taxa`.
- [ ] Testes de fronteira: 0, 1, máximo, máximo + 1, negativo, taxa maior que o valor.
- [ ] Estender o fuzzer para: idempotência, expiração da cotação e estouro **sem** limitar a faixa de entrada.
- [ ] Decidir e testar o comportamento com o livro cheio (rejeitar com mensagem clara, ou arquivar).
- [ ] Investigar H-7: simular a troca de base de tempo entre boots e escrever um teste que mostre o efeito.
- [ ] Separar, se fizer sentido, cálculo de taxas, autorização, persistência e interface (sem refatoração grande).

## Critérios de aceite
- [ ] Cada invariante documentada tem um teste ou uma checagem no fuzzer.
- [ ] Operações inválidas são rejeitadas **dentro do motor**.
- [ ] Os testes de fronteira passam sob ASan/UBSan.
- [ ] O documento diz com clareza que a simulação **nunca** representa uma transação em blockchain ou em rede Pix real.

## Testes
Unitários (host), fuzzing do motor (sem limitar entradas), `make test-asan`.

## Riscos
Mudar o arredondamento muda saldos de demonstração; avisar no PR. Cuidado para não quebrar o formato do estado salvo (N15).

## Dependências
N07 (fronteira de autenticação) e N15 (validação do estado carregado); #10 (fuzzing, parte restante).

## Evidências
`init/money.c:10-26`, `:48-52`, `:91-100`, `:128-146`, `init/main.c:846-847`, `tests/fuzz/fuzz_money.c`, `docs/PROJECT_AUDIT.md`.

## Fora do escopo
Qualquer integração externa (#11), carteira (#1) e rede.

## Definição de concluído
Documento publicado, invariantes verificadas por teste, hipótese H-7 confirmada ou descartada.
