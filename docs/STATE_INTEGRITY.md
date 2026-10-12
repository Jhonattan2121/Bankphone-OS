# Proposta: integridade do estado persistido / Proposal: integrity of the persisted state

**Tudo é DEMO / TESTNET. / Everything is DEMO / TESTNET.** Este documento é uma **proposta técnica para revisão**. **Nada aqui está implementado.**
Responde à issue **#9** e às quatro perguntas da auditoria do PR #13. Status: rascunho para decisão.

**EN summary.** The saved state (the PIN verifier line plus the money ledger) is protected only by a CRC32 per slot and by a two-slot write
protocol. A CRC detects accidents; it does not stop a person who can write to the storage. This document says what can honestly be achieved on
this phone **without a hardware root of trust** (there is none: no TEE, unlocked bootloader, no monotonic counter): detect ledger tampering by
someone who does not know the PIN, with a MAC keyed from the PIN-derived state key. It also says what **cannot** be achieved: protecting the
lockout counter, detecting rollback to an older valid state, and any protection once the PIN is guessed offline. Encrypting the state is a separate
decision that needs a cipher the project does not have. Do not claim more than this.

## 0. Revisão da rodada 4 / Round 4 revision

Esta revisão **não implementa nada**. Ela corrige o que a primeira versão deixava implícito e acrescenta o ciclo de vida da chave, o ataque offline, o rollback e os critérios de decisão.

**Quatro propriedades diferentes, que um desenho não pode confundir / four different properties:**

| Propriedade | O que garante | O que **não** garante | Mecanismo possível aqui |
|---|---|---|---|
| **Integridade acidental** | detecta bit trocado, queda de energia, gravação pela metade | adulteração deliberada | CRC32 + dois slots + leitura de volta (**já existe**) |
| **Autenticidade (MAC)** | detecta alteração feita por quem **não tem a chave** | confidencialidade; restauração de um estado antigo válido; qualquer coisa se a chave vazar | HMAC-SHA256 com chave derivada do PIN (**proposto**, depende do #13) |
| **Confidencialidade (cifra)** | quem lê a partição não vê saldos nem transações | integridade, se for cifra sem autenticação; rollback | AEAD (**o repositório não tem cifra**); decisão pendente |
| **Anti-rollback** | detecta a volta a um estado antigo **válido** | — | **só** com contador monotônico que o atacante não consegue regredir. **Não existe no aparelho (verificado só por leitura)**; ver §9 |

**Consequência central, dita sem rodeios:** o verificador do PIN está **gravado ao lado do livro-razão**. Quem copia a partição testa PINs **offline**, sem nenhum bloqueio, e depois deriva `K_state`/`K_mac` do PIN certo e forja um MAC válido. Logo, **a segurança do MAC é exatamente a resistência offline do PIN**. Com PIN de 6 dígitos (10^6 combinações) e o scrypt padrão do #13 (118 a 141 ms por tentativa em um núcleo, medido no container, **não** no aparelho), isso é da ordem de **33 a 39 horas em um núcleo** e **cerca de 20 a 23 minutos com 100 núcleos** (estimativa linear, **não medida em paralelo**). O MAC protege contra adulteração **sem esse esforço** (um script, um erro, um estado trocado por engano), **não** contra um adversário que dedica algumas horas de máquina.

## 1. Estado atual (lido no código, `main` em `32932dd`)

| Fato | Evidência |
|---|---|
| O estado gravado é o texto `P …` (linha do PIN) seguido do livro-razão `V1 …`, **em claro** | `plat_save()` em `init/main.c`: `pin_serialize` + `m_serialize` |
| Cada slot tem cabeçalho (magic, versão, estado OK/PENDING, `seq`, tamanho, **CRC32**) | `init/store.c` (`SHead`, `crc32_`) |
| Dois slots em ping-pong; escrita marca PENDING, grava, lê de volta e confere o CRC | `slot_commit`, `store_save` |
| O CRC **não autentica**: quem escreve na partição escreve também o CRC certo | por construção |
| O carregador valida **campos** (faixas, coerência) mas não **origem** | `m_deserialize` (N15) |
| A chave de estado derivada do PIN existe **só no PR #13** e **nada a usa** | `pin_state_key()` não é chamada por nenhum código |
| O aparelho não tem TEE, tem bootloader destravado, e não há contador monotônico disponível | `docs/THREAT_MODEL.md` (PR #13) |

## 2. Modelo de ameaças

| Ameaça | Quem | Pode fazer hoje | Dá para impedir? |
|---|---|---|---|
| **T1** Corrupção acidental (queda de energia, flash) | ninguém | perder ou rasgar o estado | **Sim, já:** CRC + dois slots + leitura de volta (testado no host com queda em cada passo) |
| **T2** Adulteração do **livro-razão** por quem **não** sabe o PIN | ladrão com acesso de escrita ao armazenamento | trocar saldos e transações e recalcular o CRC | **Detectar: sim**, com MAC cuja chave vem do PIN (seção 4). Impedir a escrita: não |
| **T3** Adulteração do **verificador do PIN** e do contador de tentativas | idem | trocar o verificador por um PIN próprio; zerar o contador | **Não** com o que o aparelho tem: o verificador e o contador precisam ser lidos **antes** de a chave existir. Quem escreve consegue pôr o seu PIN. Resultado: a pessoa vê "PIN errado" (negação de uso), não acesso silencioso |
| **T4** **Reversão** (rollback) para um estado antigo e válido | idem | restaurar o estado de ontem (saldo, contador de erros) com o MAC de ontem | **Não** sem contador monotônico em hardware. O `seq` do slot não ajuda: o atacante restaura slot e `seq` juntos |
| **T5** Leitura do armazenamento + força bruta **offline** do PIN | ladrão com ferramentas | testar PINs sem bloqueio nenhum | **Só encarecer** (scrypt). PIN de 6 dígitos tem 10^6 combinações; 31,6 h em 1 núcleo no container de CI com o custo padrão (medido no `make bench-kdf`, **não** no aparelho) |
| **T6** Aparelho comprometido (root em execução) | malware | tudo | Fora de escopo |

**Consequência honesta:** o MAC só é tão forte quanto o PIN. Quem adivinhar o PIN offline forja um estado válido.

## 3. Dados e o que cada um precisa

| Dado | Integridade | Confidencialidade | Pode depender do PIN? |
|---|---|---|---|
| Verificador do PIN (sal, hash, custo) | desejável, **impossível** hoje (T3) | não (já é um hash lento) | **Não:** é lido antes do desbloqueio |
| Contador de tentativas e prazo do bloqueio | desejável, **impossível** hoje (T3) | não | **Não:** o bloqueio vale antes do PIN |
| Livro-razão (transações, saldos DEMO) | **sim** (T2) | **dispensável na fase DEMO**; necessária para qualquer dado real | **Sim:** só é mostrado depois do desbloqueio |
| Número de versão do formato | sim | não | Não |

## 4. Proposta mínima (a única que o hardware permite)

**Pré-requisito:** o PR #13 (scrypt do PIN e `pin_state_key()`) **mesclado e revisado**. Sem ele não há chave.

1. **Chaves.** `K_state = scrypt(PIN, sal, custo)` (já no #13). `K_mac = HKDF-SHA256(K_state, info = "bankphone/ledger-mac/v1")`. A chave de **verificação do PIN** e a de **MAC** vêm de rótulos diferentes (separação de domínio). `K_mac` vive só na memória, enquanto desbloqueado (o `pin_forget_key()` do #13 já apaga ao bloquear a tela).
2. **Formato `S2`** (versionado, o `V1` continua legível):
   ```
   S2 <seq> <comprimento-do-livro-razão>
   <livro-razão em texto, o mesmo formato V1 já validado pelo carregador>
   MAC <hex de 32 bytes>
   ```
   `MAC = HMAC-SHA256(K_mac, "bankphone/state/v2" ‖ seq ‖ comprimento ‖ livro-razão)`. O `seq` entra no MAC (o MAC de uma geração não serve para outra) mas, ver T4, **não** impede o rollback.
3. **Quando verificar.** O livro-razão é carregado no boot como hoje (para existir a estrutura), mas **nenhuma tela o mostra e nenhuma operação o usa** até o desbloqueio. Depois de `pin_check` bem-sucedido, `state_verify(K_mac)` confere o MAC **em tempo constante** (`sec_ct_eq`).
4. **Se o MAC falhar.** Falha **fechada**: não usa esse livro-razão; tenta o **outro slot** (que tem MAC próprio); se os dois falharem, abre com livro-razão vazio, mostra "estado inválido (adulterado ou corrompido)" e **não sobrescreve** os slots ruins até a pessoa reconhecer o aviso (os slots ruins são evidência). Reaproveita a regra já corrigida no `store`: um slot de conteúdo ruim não é slot bom.
5. **Gravar.** `plat_save()` só grava `S2` se `K_mac` existe (desbloqueado). Sem chave (por exemplo, gravar o contador de tentativas na tela de bloqueio), grava só a linha do PIN, que **não** é autenticada, e mantém o livro-razão do slot anterior.
   *Isto significa dois objetos gravados separadamente, ou um slot com duas seções; é a parte de maior mudança e precisa de desenho próprio.*
6. **Troca de PIN.** Gera sal novo, deriva `K_state` novo, **recalcula o MAC** do livro-razão e grava num slot novo; o slot antigo (com o MAC antigo) continua válido até o novo ser confirmado (o protocolo de dois slots já dá isso).
7. **Migração `V1` → `S2`.** No primeiro desbloqueio correto com o PIN scrypt: o livro-razão `V1` passa pelo carregador estrito (N15), é regravado como `S2` com MAC, num slot novo. Uma interrupção deixa o slot `V1` intacto (o protocolo de dois slots). Um `S2` **nunca** é aceito como `V1` (a versão é recusada, e o firmware antigo já recusa linha desconhecida, o que se liga ao risco de downgrade do #13).

## 5. O que esta proposta **não** resolve (dito sem rodeios)

- **Contador de tentativas e verificador do PIN seguem sem proteção** (T3). Não existe hoje uma chave que não dependa do PIN.
- **Rollback (T4)** não é detectado. Uma proteção real exige um contador monotônico que o aparelho **não oferece** (a partição `expdb` não é um). Não inventar um mecanismo de gestão de chaves que o hardware não tem.
- **Confidencialidade:** HMAC não cifra. Cifrar o livro-razão exige uma cifra. O repositório tem SHA-256, HMAC, PBKDF2, HKDF e scrypt, **sem AES nem ChaCha20**. Escrever uma cifra do zero soma risco criptográfico. Decisão pendente: (a) não cifrar na fase DEMO e dizer que não cifra; ou (b) adotar uma biblioteca revisada com AEAD (por exemplo libsodium, XChaCha20-Poly1305), que dá cifra **e** integridade com uma primitiva só, o que substitui o MAC desta proposta. Isso é **dependência nova** e precisa de decisão do mantenedor.
- Quem **adivinha o PIN** forja tudo (T5).
- A proposta não substitui a revisão independente do `sec.c`.

## 6. Comportamento nos casos de falha (contrato)

| Situação | Comportamento esperado |
|---|---|
| Arquivo inválido ou truncado | o carregador estrito recusa por inteiro (já verdade na `main`); nada parcial |
| Corrupção de um byte do livro-razão | CRC do slot falha → usa o outro slot; se o CRC passar e o MAC falhar → mesmo caminho |
| Reinicialização no meio de uma gravação | dois slots + PENDING + leitura de volta (testado no host com queda em cada passo de escrita e `fsync`) |
| Queda entre a migração `V1`→`S2` e a confirmação | o slot `V1` segue válido; a migração recomeça no próximo desbloqueio |
| PIN errado | nenhuma chave; o livro-razão não é mostrado; o bloqueio e o contador valem |
| Aparelho comprometido | sem garantia |

## 7. Critérios de aceite (testes de host que a implementação deve ter)

1. **Vetor + diferencial:** o HMAC de `S2` bate com o do Python (`hmac`) em 100+ casos aleatórios, e com vetores conhecidos.
2. **Adulteração byte a byte:** trocar **cada** byte do livro-razão e do MAC → o MAC falha em 100% dos casos (teste exaustivo num estado pequeno).
3. **Truncamento em cada tamanho** → recusado, nunca aceito parcial.
4. **Slot A adulterado, slot B bom** → o B é usado e o A não é sobrescrito antes do aviso.
5. **Os dois adulterados** → livro-razão vazio, aviso, nenhum slot sobrescrito.
6. **Migração `V1`→`S2`** com queda em cada passo (o injetor de falhas do `store` já existe).
7. **PIN errado** nunca revela o livro-razão e nunca grava.
8. **Rollback aceito e documentado:** um teste que **mostra** que restaurar um slot antigo com MAC válido é aceito, para que ninguém afirme o contrário.
9. **Comparação em tempo constante** do MAC (revisão de código; o `sec_ct_eq` existe).
10. Tudo com ASan e UBSan, e fuzz do parser do formato `S2`.

## 8. Decisões que dependem do mantenedor

| Decisão | Opções | Recomendação |
|---|---|---|
| Cifrar o livro-razão agora? | (a) não, na fase DEMO; (b) adotar biblioteca com AEAD | (a) agora, (b) quando houver dado real |
| Onde guardar o contador de tentativas | continua em claro no slot; ou numa seção separada sem MAC | seção separada, para o MAC do livro-razão não depender dele |
| Comportamento com os dois slots inválidos | esvaziar e avisar; ou recusar o boot do sistema financeiro | esvaziar e avisar, sem apagar os slots |
| Dependência externa | seguir sem; ou libsodium | decidir junto com a revisão do #13 |

## 9. Rollback e hardware: o que seria preciso, e o que **não** foi verificado

- Um contador **dentro do estado** é regredido junto com o estado. Um contador em **outra partição** é regredido por quem escreve nas duas. O `seq` do slot não ajuda (T4).
- O que resolveria é um contador monotônico **em hardware** que o atacante não consiga baixar. Candidatos, **todos NÃO VERIFICADOS neste aparelho** (HW-21): a partição **RPMB** do eMMC (contador de escrita autenticado), um TEE (o X669C com bootloader desbloqueado e sem Android: não há razão para supor que exista um acessível) ou um elemento seguro externo (não existe).
- **Aviso:** a chave da RPMB é programada **uma única vez e de forma irreversível**. Nenhum teste deste projeto deve gravá-la sem aprovação humana explícita; a investigação de HW-21 é só leitura de capacidade.
- Enquanto isso não for provado: **o sistema não detecta rollback**, e nenhum texto do projeto pode dizer o contrário. O teste de aceite 8 (§7) existe para impedir essa afirmação.

## 10. Ciclo de vida da chave (especificação, nada implementado)

| Etapa | Regra |
|---|---|
| Derivação | `K_state = scrypt(PIN, sal, custo)` (já no #13, rótulo `bankphone/pin/v2`); `K_mac = HKDF-SHA256(K_state, info = "bankphone/ledger-mac/v1")`. **Nunca** usar o verificador como chave nem o contrário (já separados por HKDF no #13) |
| Uso | só em memória, só enquanto desbloqueado; comparação do MAC em tempo constante (`sec_ct_eq`) |
| Invalidação | `pin_forget_key()` ao bloquear a tela (já existe no #13) e ao trocar de PIN; **nunca** gravar a chave |
| Migração `V1`→`S2` | só no primeiro desbloqueio certo com o PIN do #13; o livro-razão `V1` passa pelo carregador estrito (N15) e é regravado `S2` **num slot novo**; o slot `V1` fica válido até o novo ser confirmado (o protocolo de dois slots já dá isso) |
| Troca de PIN | sal novo, `K_state` novo, MAC recalculado, gravado num slot novo; o antigo vale até o novo ser confirmado |
| Recuperação | os dois slots têm MAC próprio; se um falha, o outro vale; se os dois falham, abre vazio **sem apagar** os slots (evidência) e avisa |
| Custo do scrypt trocado | o verificador carrega o custo (`P2`); mudar o custo exige o PIN (`pin_check` certo) e regrava |
| Downgrade para firmware antigo | o firmware antigo recusa `S2` (linha desconhecida) e **sobe com estado novo**: perda de dados (R-1). Mitigação: avisar antes de instalar; **não** inventar uma "versão mínima" gravada no estado (um atacante a regride) |

## 11. Critérios para **decidir** antes de implementar (preciso de aprovação humana)

| Pergunta | Opção A | Opção B | Recomendação |
|---|---|---|---|
| O MAC com chave do PIN justifica o custo/complexidade, dado o ataque offline? | sim, como proteção contra adulteração simples e como pré-requisito de qualquer cifra | não; esperar um segredo preso a hardware | **A, dita com honestidade** ("detecta adulteração simples; não resiste a quem testa PINs offline") |
| Cifrar o livro-razão agora? | não (fase DEMO, dado fictício) | adotar AEAD revisado (libsodium XChaCha20-Poly1305) | **A** até haver dado real; **B** antes de qualquer dado real |
| PIN de 6 dígitos ou senha longa? | manter (UX) | frase longa opcional para quem quiser | decisão de produto; o ganho de segurança vem daqui, não do MAC |
| Onde guarda contador de tentativas e bloqueio? | no slot, em claro (hoje) | seção separada sem MAC | **B**, para o MAC do livro-razão não depender dele (T3) |
| Dois slots inválidos | abrir vazio, avisar, **não** sobrescrever | recusar o boot | **A** |

**Bloqueadores antes de implementar:** (1) #13 revisado por pessoa independente e mesclado; (2) HW-07 (custo do scrypt no aparelho) para saber quanto custa cada desbloqueio; (3) as decisões da tabela acima; (4) a política do contador do #13 × `main` (`RECONCILE_PR13_PR15.md` §3).

## 12. Testes de aceite adicionais (além da §7)

11. **PIN offline:** um teste que, dado o estado `S2`, tenta recuperar o PIN por força bruta em um espaço pequeno e **mostra** que a chave sai (para documentar o limite da §0, não para "provar segurança").
12. **Migração com queda de energia em cada passo** (o injetor de falhas do `store` já existe) e **PIN errado durante a migração** nunca corrompe o `V1`.
13. **Troca de PIN com queda de energia:** o PIN antigo continua valendo até o novo slot ser confirmado.
14. **Estado `S2` com MAC válido de outro PIN** é recusado depois da troca de PIN.
15. **Fuzz** do parser do formato `S2` (truncado, campo gigante, MAC curto).
