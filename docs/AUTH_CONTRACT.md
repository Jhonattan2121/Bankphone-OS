# Contrato de autorização do motor / Engine authorization contract

**Tudo é DEMO / TESTNET. / Everything is DEMO / TESTNET.** (N07, achado F-06 da auditoria)

## Português

**O que mudou.** Antes, `m_authorize(tx, nível, agora)` recebia o nível "alcançado" do chamador e a UI passava o
próprio nível exigido (`m_required`). Quem chamasse o motor autorizava sem PIN. Agora `m_authorize(tx, agora)` **não
recebe nível**: pergunta a `sec.c` (`auth_peek`) se existe uma **concessão** viva para aquela transação.

| Propriedade da concessão | Regra |
|---|---|
| Quem emite | **Só `pin_check`**, quando o PIN confere. Não há outra função que emita |
| Ligada a uma transação | A UI diz para qual transação o próximo PIN vale (`auth_expect`). PIN digitado **sem** pagamento esperando (tela de bloqueio) não emite nada |
| Nível | PIN certo → `L_PIN`. A segunda confirmação explícita (`auth_confirm`) sobe para o nível máximo; a **espera** de 30 s continua sendo regra do motor |
| Validade | `AUTH_TTL_S` = 120 s. Relógio que volta atrás invalida. A espera de cooldown (30 s) precisa caber na validade (há teste) |
| Uso único | O motor consome a concessão quando a transação é decidida (autorizada, falhou, expirou, cancelada, emergência). Recusa por nível ou por espera **não** consome |
| Outras transações | A concessão de uma transação não autoriza outra |
| Descartada quando | A tela bloqueia (`ui_lock`), a UI reinicia, volta da tela de PIN, cancela, ou uma nova expectativa é armada |

**Fluxo da UI.** "Autorizar" → `auth_expect(id)` → PIN → `pin_check` emite a concessão → se o valor exige só PIN,
`m_authorize`; senão, tela de confirmação → `auth_confirm(id)` → `m_authorize`.

**Limite, dito com clareza.** É **um processo só**. Código que já roda dentro dele ainda pode chamar qualquer função,
inclusive de `sec.c`. A concessão fecha o buraco de "passe o nível que quiser" e liga a autorização ao PIN realmente
conferido; **não é uma sandbox** nem substitui a tela de confirmação do sistema da issue #4. A "segunda confirmação"
é um toque da UI, não prova criptográfica.

**Gancho de teste.** `auth_test_mint` só existe com `BANKPHONE_AUTH_TEST`. Só o build do `make fuzz` o define (o
emissor real custa 18 ms por chamada). `make lint` falha se o nome aparecer em `build.sh`, `install.sh`, `scripts/`,
`.github/` ou nos arquivos do sistema.

**Não verificado:** o fluxo completo na tela do aparelho. Os testes cobrem o motor e o emissor; o caminho dos toques
em `ui.c` não tem teste automático.

## English

**What changed.** `m_authorize(tx, level, now)` used to take the "achieved" level from the caller, and the UI passed
the required level itself (`m_required`), so any caller could authorise without a PIN. Now `m_authorize(tx, now)`
**takes no level**: it asks `sec.c` (`auth_peek`) whether a live **grant** exists for that transaction.

The grant is issued **only by `pin_check`** when the PIN matches, and is tied to one transaction (the UI says which one
the next PIN is for with `auth_expect`; a PIN typed with no payment waiting, such as on the lock screen, issues
nothing). `L_PIN` after the PIN; the explicit second confirmation (`auth_confirm`) raises it; the 30 s wait stays an
engine rule. Lifetime `AUTH_TTL_S` = 120 s; a clock that goes backwards invalidates it; the cooldown must fit inside
the lifetime (tested). Single use: the engine consumes it when the transaction is decided; a refusal for level or wait
does not. A grant never authorises another transaction. It is dropped on lock, UI reset, leaving the PIN sheet,
cancel, or a new expectation.

**Limit, plainly.** It is **one process**: code already inside it can still call anything, including `sec.c`. The
grant closes the "pass the level you like" hole and ties authorisation to a PIN that was really verified; it is **not a
sandbox** and does not replace the system confirmation screen of issue #4. The "second confirmation" is a UI tap, not
cryptographic proof.

**Test hook.** `auth_test_mint` exists only with `BANKPHONE_AUTH_TEST`; only the `make fuzz` build defines it (the
real issuer costs 18 ms per call). `make lint` fails if the name shows up in `build.sh`, `install.sh`, `scripts/`,
`.github/` or the system files.

**Not verified:** the full flow on the phone's screen. The tests cover the engine and the issuer; the touch path in
`ui.c` has no automated test.
