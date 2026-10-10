# [P1] A política de autenticação vive na UI: o motor financeiro aceita o nível "alcançado" do chamador

**Labels sugeridos:** `priority:P1`, `area:finance`, `area:security`, `type:bug`
**Tipo:** risco de projeto confirmado no código (F-06)
**Precisa do Infinix Hot 30i:** não
**EN:** The authentication policy lives in the UI; the money engine accepts the caller's "achieved level".

## Contexto
O motor decide o nível exigido por valor (PIN, PIN mais confirmação, ou mais espera de 30 s). Quem garante que a pessoa **realmente** passou por esse nível deveria ser o sistema, não a ordem das telas.

## Estado atual
- `m_authorize(Tx *t, Level got, int64_t now)` (`init/money.c:130`) recebe `got` do chamador e só compara com `m_required(t)` (`:128`).
- Na UI, `ACT_CONFIRM2` (`init/ui.c:326-334`) chama `m_authorize(t, m_required(t), ...)`: passa **o próprio nível exigido como se tivesse sido alcançado**.
- O PIN ter sido digitado antes é garantido pelo fluxo de telas (`SH_PIN` → `SH_CONFIRM2`), não pelo motor.
- Teste atual (`tests/money_test.c`) chama `m_authorize` direto com o nível desejado.

## Problema
Qualquer outro chamador (um comando serial futuro, uma tela nova, um teste) autoriza uma transação **sem PIN** passando o nível que quiser. A fronteira de segurança está no lugar errado.

## Objetivo
O motor só autoriza se tiver **recebido uma prova** de autenticação criada pelo módulo de segurança (por exemplo, um token de uso único e curto, emitido por `pin_check` com sucesso), e não um número que o chamador escolhe.

## Tarefas
- [ ] Definir o contrato: `sec` emite uma "concessão" (nível, hora, uso único, ligada ao id da transação); o motor exige a concessão válida.
- [ ] Mudar `m_authorize` para receber a concessão (ou consultá-la no módulo `sec`) e **rejeitar** quando faltar, expirar, já tiver sido usada ou for de nível menor.
- [ ] Atualizar `ui.c` para solicitar a concessão após o PIN e passar a concessão correta; remover o `m_required(t)` como "alcançado".
- [ ] Documentar o contrato em `docs/MONEY_ENGINE.md` (ver N11).
- [ ] Testes: autorizar sem concessão falha; concessão expirada falha; concessão reutilizada falha; nível menor falha; fluxo normal passa.
- [ ] Acrescentar essas regras às invariantes do fuzzer (`tests/fuzz/fuzz_money.c`).

## Critérios de aceite
- [ ] Nenhum caminho de código autoriza uma transação sem concessão emitida por `sec`.
- [ ] Os testes descritos existem e passam; o fuzzer não consegue autorizar sem concessão.
- [ ] Os tetos por Pix e diário e a espera de 30 s continuam valendo.

## Testes
Unitários (host), fuzzing do motor, `make test-asan`.

## Riscos
Mudar a interface do motor quebra os testes atuais; atualizá-los no mesmo PR. Cuidado para não enfraquecer a política existente.

## Dependências
Interage com #4 (tela de confirmação do sistema) e com o PR #13 (PIN com scrypt, que expõe `pin_check`). Pode seguir sem eles, mas combine a ordem para evitar conflito em `sec.c`.

## Evidências
`init/money.c:128-146`, `init/ui.c:326-334`, `tests/money_test.c`, `docs/PROJECT_AUDIT.md` (F-06).

## Fora do escopo
Biometria, TEE, e a tela de confirmação à prova de falsificação (#4).

## Definição de concluído
Motor impõe a autenticação sozinho, com testes e fuzzing, e a documentação do contrato publicada.
