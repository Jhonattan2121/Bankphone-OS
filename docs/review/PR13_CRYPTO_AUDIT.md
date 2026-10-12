# Auditoria criptográfica do PR #13 (scrypt do PIN) / Cryptographic audit of PR #13

**Tudo é DEMO / TESTNET. / Everything is DEMO / TESTNET.**

## Decisão / Decision

> **REQUEST CHANGES → corrigido na própria branch; falta revisão humana independente e medição no aparelho.**
> **REQUEST CHANGES → fixed on the branch itself; an independent human review and a measurement on the phone are still missing.**

**EN summary.** The audit of `sec/scrypt-pin-kdf` at `066b13d` found one defect that blocked approval: the
failed-try counter read from the (unauthenticated) state file was not validated, so a value of `INT_MAX`
overflowed a signed integer (undefined behaviour, reproduced with UBSan) and a negative value gave unlimited
wrong tries. It was fixed in `0255d37`, and the PIN entry buffers are now wiped in `60935bc`, both with regression
tests. With those two commits the implementation matches RFC 7914 / 5869 / 4231 on every vector tried and the
migration and failure paths behave as described below. **That is not an approval of the PIN protection as a
whole**: the author wrote both the code and this audit, so it is **not an independent review**; the cost on the
Infinix Hot 30i X669C is **not measured**; and a 6-digit PIN does not resist an offline attack with any KDF.
Recommended handling: have a second person review `init/sec.c`, merge **only for DEMO/host use**, and keep issue #2
open until the hardware items in section 7 are done.

**Por que não `APPROVE` puro / why not a plain `APPROVE`:** (1) autor = auditor; (2) sem medição no X669C;
(3) o CI e os testes de host não provam segurança, só ausência de regressão nos casos testados.
**Por que não `BLOCKED`:** o defeito achado era pequeno, local e já está corrigido com teste; nenhum achado
restante impede o uso em DEMO.

Estado da branch auditada / audited branch state:

| Item | Valor |
|---|---|
| Branch | `sec/scrypt-pin-kdf` |
| Commit auditado | `066b13d` (cabeça do PR no começo da auditoria) |
| Correções desta auditoria | `0255d37` (contadores) e `60935bc` (limpar buffers do PIN) |
| Base | `main` em `86cda6f` |
| Arquivos principais | `init/sec.c`, `init/sec.h`, `init/ui.c`, `tests/sec_test.c`, `tests/crypto_diff.{c,py}`, `tests/fuzz/fuzz_pin.c`, `docs/THREAT_MODEL.md` |

## Como cada ponto foi verificado / How each point was checked

Níveis: **[CÓDIGO]** lido no código · **[HOST]** testado ou reproduzido no host (container Linux x86_64, sem o
aparelho) · **[APARELHO]** verificado no Infinix Hot 30i X669C · **[PLANO]** planejado, não comprovado.
**Nenhum item abaixo está no nível [APARELHO].**

## 1. Uso correto do scrypt e dos parâmetros de custo

| Verificação | Resultado | Nível |
|---|---|---|
| Algoritmo conforme a RFC 7914 (Salsa20/8, BlockMix, ROMix, PBKDF2-SHA256 nas duas pontas) | Conforme. Os vetores oficiais da RFC 7914 passam e **850 casos** comparados com o `hashlib.scrypt` do Python (OpenSSL) batem byte a byte | [HOST] |
| `Integerify` usa só a palavra baixa do último bloco | Correto enquanto `N <= 2^32`; o código limita `log2n <= 24` | [CÓDIGO] |
| Custo padrão | N=2^15, r=8, p=1 → 32 MiB por tentativa. **É escolha de projeto, não medida no X669C.** Ver seção 8 | [CÓDIGO] |
| Comparação com recomendações públicas | A recomendação pública para scrypt varia por tabela e por ano; **não consultei fontes externas nesta rodada**. Antes de fixar o custo, comparar com a tabela atual do OWASP Password Storage Cheat Sheet | [PLANO] |
| Separação em duas chaves | `scrypt → k`; HKDF-SHA256 com `info` diferente gera o verificador e a chave de estado. Conhecer o verificador não dá a chave de estado | [CÓDIGO] + teste "chave não vai para o estado" |
| Parâmetros do scrypt vêm de dado não confiável (estado gravado) | Validados por `scrypt_params_ok` antes de usar: `1<=log2n<=24`, `1<=r<=32`, `1<=p<=16`, memória `128*r*2^log2n <= 128 MiB` | [CÓDIGO] + [HOST] (casos absurdos recusados) |

**Limite que o teto de memória não cobre:** o **tempo**. Um custo gravado válido (por exemplo `p=16`) pode demorar
muito mais que o padrão, e a tela de desbloqueio trava enquanto roda. Só quem consegue gravar no estado pode
causar isso, o que já lhe dá outros meios (seção 5). Registrado como R-3.

## 2. Limites de entrada, overflow e falha de alocação

| Verificação | Resultado | Nível |
|---|---|---|
| PIN/senha: 1 a 64 bytes; mais que isso é **recusado, nunca cortado** (o código antigo cortava em 16) | Conforme; testes com 65 bytes e vazio | [HOST] |
| Aritmética de tamanhos | `(uint64_t)128*r<<log2n` no teto de memória; `(size_t)128*r*N` só depois do teto. Em `size_t` de 32 bits o teto de 128 MiB ainda cabe | [CÓDIGO] |
| `malloc` falha (`B`, `V` ou `XY`) | Libera o que alocou e devolve `-1`; `pin_derive_v2` devolve `-1`; `pin_check` devolve `3` **sem contar tentativa errada**; `pin_set` não define nada | [CÓDIGO] + [HOST] (custo inválido em memória → 3) |
| Falha de alocação **real** | **Não foi exercitada** (nenhuma injeção de falha em `malloc`). O caminho é pequeno e foi lido, mas não executado | [CÓDIGO] apenas |
| `sha256_update` com `len == 0` e ponteiro nulo | Corrigido (UBSan achou); teste de regressão | [HOST] |
| Fuzzing do parser do estado do PIN (`fuzz_pin`) | Ver seção 9: sem falhas nas execuções feitas | [HOST] |

## 3. Tratamento de erros sem fallback inseguro

| Verificação | Resultado | Nível |
|---|---|---|
| Se o scrypt falha, o código cai para o SHA-256 antigo? | **Não.** `pin_check` devolve `3`; `pin_set` devolve `-1`. O verificador legado só é usado para estado que **já é** legado (`ver == 1`) | [CÓDIGO] |
| Migração falha por falta de memória | O estado continua legado e a migração é tentada de novo no próximo desbloqueio correto | [CÓDIGO] + [HOST] |
| Sal do PIN fraco | Era o achado F-04 (N05): `plat_random` tinha plano B previsível. **Corrigido no PR #15**, que **depende** deste PR. Enquanto o #15 não entrar, o #13 sozinho ainda usa o `plat_random` antigo | [CÓDIGO] |
| `pin_deserialize` recusa em vez de aceitar parcialmente | Recusa linha curta, hash curto, custo absurdo, `set=1` com custo zero. **Antes de `0255d37` não validava `set`, contador e `locked_until`** (achado A-1) | [HOST] |

## 4. Limpeza de buffers sensíveis

`sec_wipe` escreve zeros por um ponteiro `volatile`, que o compilador não pode eliminar. É a melhor técnica portátil
em C; **não garante** que o compilador não tenha deixado cópias em registradores ou em variáveis temporárias da pilha.

| Dado | Limpo? | Nível |
|---|---|---|
| Chave intermediária do scrypt, `B`, `V`, `XY` | Sim, antes de `free` | [CÓDIGO] |
| Contextos SHA-256 e HMAC | Sim (`sha256_final`, `hmac_sha256`, `pbkdf2_sha256`, `hkdf_sha256`) | [CÓDIGO] |
| Verificador e chave de estado temporários em `pin_set`/`pin_check` | Sim | [CÓDIGO] |
| Chave de estado global `g_statekey` | Apagada por `pin_forget_key()` ao bloquear a tela e ao ler outro estado | [CÓDIGO] + [HOST] |
| Cópia do PIN digitado (`p[8]`), `pinbuf`, `pin_first`, sal na pilha de `ui.c` | **Antes: não** (`pinbuf[0] = 0` só terminava a string; achado A-2). **Agora: sim**, em `60935bc` | [CÓDIGO]; o caminho da UI **não tem teste automático** |
| Caminho legado `pin_derive` | Não limpa `buf` nem `h`. É código antigo mantido só para estado legado; deixado de propósito, registrado como R-4 | [CÓDIGO] |
| Memória `V` do scrypt pode ir para swap | O aparelho não usa swap, mas **não verifiquei** (`mlock` não é usado) | [PLANO] |

## 5. Proteção e ciclo de vida da chave de estado

- Nasce em `pin_set` (PIN novo) ou no desbloqueio correto (`pin_check`), vive em `g_statekey`, e **nunca** é gravada
  (teste: a linha serializada não contém a chave). É apagada em `ui_lock()` (tela apagou) e a cada `pin_deserialize`.
  [CÓDIGO] + [HOST]
- **Hoje nada usa essa chave.** O estado gravado **não é cifrado** (issue #9). O PR prepara a chave; não entrega
  confidencialidade do estado. O texto do PR e a issue #2 devem dizer isso com todas as letras.
- **Arquivo de estado não é autenticado.** O CRC do `store` detecta acidente, não adulteração. Quem escreve no
  armazenamento pode: (a) zerar o contador de erros, (b) trocar o verificador por um PIN próprio, (c) apagar a linha do
  PIN. Isto é inerente ao modelo (bootloader destravado, sem TEE) e está no `THREAT_MODEL.md`. A validação nova
  (`0255d37`) impede que um contador forjado cause comportamento **indefinido**; não impede que o atacante o zere.

## 6. Migração, compatibilidade e reinício

| Cenário | Resultado | Nível |
|---|---|---|
| Estado antigo (`P ...`) lido pelo código novo | Lido como versão 1; PIN errado **não** migra e conta como erro; PIN certo migra para `P2` e zera o contador | [HOST] |
| Linha `P` de PIN não definido | Continua idêntica byte a byte (compatibilidade e teste de regressão) | [HOST] |
| Interrupção durante a gravação da migração | A migração só vale depois do `plat_save()`. O `store` usa dois slots com marcador PENDING e CRC; uma queda de energia deixa o slot anterior (com o verificador antigo) válido. Testado no `store` com queda em cada passo; **a interação migração + queda não tem teste próprio** | [HOST] parcial |
| Reinício com `P2` | Lê o custo gravado, valida, e usa esse custo para conferir (não o padrão atual) | [HOST] |
| **Voltar para firmware anterior (downgrade)** | **Risco real.** Reproduzido com o `sec.c` da `main`: ele **recusa a linha `P2`** (`pin_deserialize` devolve 0, `PIN.set = 0`). O firmware antigo então trata o aparelho como "sem PIN" e a tela pede para **criar um PIN novo**, sobrescrevendo o verificador. Ligado a #8 (proteção contra versão antiga) e #9 | [HOST] reproduzido |
| Fallback A/B para o slot antigo com estado já migrado | Mesma consequência do item anterior. Mitigação: documentar; decidir em #8/#9 se o estado ganha um número de versão que o firmware antigo reconheça e recuse | [PLANO] |

## 7. Tentativas incorretas e bloqueio

- 5 erros livres; do 5º em diante o bloqueio é `30 s × 2^(erros−5)`, com expoente limitado a 6 (≈ 32 min). [CÓDIGO]
- O contador é gravado por `plat_save()` **depois** de `pin_check` e **só com a área de estado armada** (a imagem
  somente leitura o mantém em memória). Cortar a energia durante a conferência pode perder uma tentativa. O arquivo
  não é autenticado, então um atacante com acesso ao armazenamento ignora o bloqueio. Corrigir isso é a issue **#9**,
  não este PR. [CÓDIGO]
- **A-1 (corrigido):** contador `INT_MAX` → `++PIN.fails` com overflow de inteiro com sinal; contador negativo →
  tentativas ilimitadas (reproduzido: 50 tentativas erradas sem bloqueio com `fails = -1000`); `set = 7` era aceito;
  `locked_until` negativo era aceito. Agora: `set ∈ {0,1}`, `0 <= fails <= 1000`, `locked_until >= 0`, e `pin_check`
  limita o contador a 1000. 5 verificações novas **falham** com o código de `066b13d` e passam com o de `0255d37`.
  [HOST]
- `locked_until` muito grande (por exemplo `INT64_MAX`) trava o aparelho para sempre. Só quem altera o estado faz isso,
  e não dá acesso, só negação de uso. Registrado como R-5. [HOST] reproduzido
- A tela de bloqueio aceita **só 6 dígitos**. O PIN de 6 dígitos tem 10^6 combinações; o scrypt apenas encarece cada
  tentativa offline. Ver seção 8. [CÓDIGO]

## 8. Desempenho e interface

Medido **neste container de CI** (`make bench-kdf`, x86_64, 4 núcleos, **não é o X669C**; o próprio benchmark é
executado em um núcleo):

| KDF | Tempo por tentativa | 10^6 PINs de 6 dígitos em 1 núcleo |
|---|---|---|
| SHA-256 × 50.000 (antigo) | 18,0 ms | 5,0 h |
| scrypt N=2^14, r=8 (16 MiB) | 55,6 ms | 15,4 h |
| scrypt N=2^15, r=8 (32 MiB) — **padrão** | 113,8 ms | 31,6 h |
| scrypt N=2^16, r=8 (64 MiB) | 273,2 ms | 75,9 h |

- Esses números variam entre execuções (o `THREAT_MODEL.md` registra 118–141 ms em outra execução). **Não são o
  desempenho do X669C.** O núcleo do aparelho é bem mais lento; a pendência está em `HARDWARE_TESTS.md` (HW-07).
- Num núcleo, o scrypt padrão é só ≈ 6–7× mais caro que o verificador antigo. O ganho real é contra atacante
  **paralelo**, que **não foi medido**. Um PIN de 6 dígitos cai em minutos ou horas para quem tem vários núcleos.
- A derivação roda **na thread da interface do PID 1**: a tela de desbloqueio congela enquanto o KDF roda (`THREAT_MODEL.md`
  já diz). Não sei, sem medir, quanto tempo isso dura no aparelho nem se o `cpu_boost` ajuda (hipótese H-4).
  Se o PID 1 ficar muito tempo sem responder, **não sei** se isso interage com o watchdog (hipótese H-1).
- Alocar 32 MiB no PID 1 é pequeno para o aparelho, mas **não foi verificado** em memória real.

## 9. Testes: o que existe e o que falta

| Teste | O que prova | O que **não** prova |
|---|---|---|
| `tests/sec_test.c` (76 verificações em `60935bc`) | Vetores RFC 7914 (scrypt), RFC 4231 (HMAC), RFC 5869 (HKDF), SHA-256; migração; limites; contadores adulterados; chave de estado fora do estado | Correção em CPU ARM; tempo no aparelho |
| `tests/crypto_diff.{py,c}` (850 casos) | Concordância byte a byte com OpenSSL, via Python, em casos aleatórios e de borda | Só roda com `hashlib.scrypt` disponível; é **pulado** sem ele |
| `tests/fuzz/fuzz_pin.c` | O parser do estado do PIN não cai nem lê fora do buffer | Não chama `pin_check` depois do parse (por custo); o overflow A-1 passou despercebido por isso. O caso agora tem teste dedicado |
| Casos negativos | PIN errado, vazio, longo, custo absurdo, linha cortada, hash curto, KDF falhando | Falha real de `malloc` |
| **Faltam** | Teste do caminho de `ui.c` (recusar PIN fraco, limpar buffers); teste migração + queda de energia; injeção de falha de `malloc`; revisão independente | |

Resultados reais da suíte: ver `docs/GITHUB_MAINTENANCE_REVIEW.md`, seção 6.

## 10. Achados e riscos

| ID | Achado | Gravidade | Estado |
|---|---|---|---|
| A-1 | Contador, `set` e `locked_until` do estado sem validação (overflow com sinal, tentativas ilimitadas) | Média (exige escrever no estado) | **Corrigido** em `0255d37`, com teste |
| A-2 | Dígitos do PIN permaneciam em `pinbuf`/`pin_first`/pilha | Baixa | **Corrigido** em `60935bc`; caminho de UI sem teste automático |
| R-1 | Downgrade para firmware antigo apaga o PIN do ponto de vista dele | Média | Aberto; depende de #8/#9 |
| R-2 | Estado não autenticado; contador gravado depois da conferência | Alta, mas inerente ao modelo atual | Aberto em #9; documentado no modelo de ameaças |
| R-3 | Custo gravado válido pode ser lento (`p` alto) | Baixa | Aceito; só com escrita no estado |
| R-4 | Caminho legado não limpa `buf`/`h` | Muito baixa | Aceito; some quando o legado for removido |
| R-5 | `locked_until` enorme trava o aparelho | Baixa | Aceito; negação de uso, exige escrita no estado |
| R-6 | Custo e tempo no X669C desconhecidos | — | **Pendência de hardware** (HW-07) |
| R-7 | PIN de 6 dígitos não resiste a ataque offline | Inerente | Documentado; PIN mais longo é decisão de produto |
| R-8 | A chave de estado ainda não protege nada | — | Dito nos textos; entrega real em #9 |

## 11. O que precisa acontecer antes de chamar a proteção do PIN de "validada"

1. Revisão independente de `init/sec.c` por outra pessoa. (Quem escreveu e quem auditou foi a mesma.)
2. Medir o scrypt no X669C (HW-07) e escolher o custo com esse número, não com o do container.
3. Decidir o comportamento no downgrade (R-1) junto com #8 e #9.
4. Teste de queda de energia durante a migração do verificador.
5. Mesclar o #15 (sal sem plano B) **depois** deste.

A issue **#2** fica **aberta**: o critério "KDF forte mesclado e medido no aparelho" não foi cumprido.
