# BANKPHONE OS: threat model / modelo de ameaças

**Everything here is DEMO / TESTNET.** There is no real money. This document says what the security work
is meant to stop, what it does not stop, and where the numbers come from.
**Tudo aqui é DEMO / TESTNET.** Não existe dinheiro real. Este documento diz o que a segurança quer
impedir, o que ela não impede, e de onde vêm os números.

Status: first version, written with the scrypt PIN KDF (issue #2). It will change as the wallet, the
persistent state and the other issues land. / Status: primeira versão, escrita junto com o KDF scrypt do
PIN (issue #2). Vai mudar quando a carteira, o estado persistente e as outras issues chegarem.

---

# English

## The honest summary

- The phone has **no hardware keystore (no TEE), no verified boot, and an unlocked bootloader**.
- So anyone who holds the phone can copy its storage and attack the PIN **offline**, with as many
  computers as they like. No software on the phone can prevent that.
- The only thing that slows that attack is **how expensive each PIN guess is** and **how many possible
  PINs there are**. A 6-digit PIN has only one million possibilities. **It will not stop a determined
  attacker who gets the storage.**
- The promise is **"reduce the attack surface"**, not "unhackable".

## What is being protected

| Asset | Today | Where it lives |
|---|---|---|
| PIN verifier | exists | `PIN` in `init/sec.c`, saved in the state line `P2 ...` (`P ...` before the upgrade) |
| State key (to encrypt state and, later, the wallet key) | exists in memory only | `pin_state_key()`, only while unlocked; never stored |
| Wallet key / seed | does not exist yet | issue #1 |
| Balance and transactions | demo data | `init/money.c` |

## Who might attack, and what stops them

| Attacker | What they can do | Defended? |
|---|---|---|
| **A1. Thief with the locked phone, no tools** | Types PINs on the screen | **Yes, partly.** 5 wrong tries lock the phone, with a delay that doubles up to about 32 minutes. The counter is saved with the state after each try, but **only when the state area is armed** (`plat_save()` right after `pin_check()`); the read-only test images keep it in memory, so a reboot resets it there. It is saved **after** the PIN check, so cutting the power during the check can lose a try, and whoever can edit storage can restore an older copy (issue #9). |
| **A2. Thief with the phone and tools** (unlocked bootloader, reads the storage) | Copies the verifier and guesses PINs offline, no lockout | **Only by the KDF cost and by PIN length.** See the numbers below. A 6-digit PIN falls. |
| **A3. Malicious USB host** | Sends commands on the serial port | **Yes.** The only command (`REBOOT-BOOTLOADER`) exists only in test images (`bankphone.devcmd=1`); a production image accepts none. The reader was fuzzed. |
| **A4. Someone watching the screen** | Reads the PIN as it is typed | **No.** The keypad is fixed. |
| **A5. Owner forced to unlock** | Is made to unlock and send money | **No.** Planned: duress PIN (#3), time-locked vault (#7). |
| **A6. Tampered system image** | Flashes a modified image that records the PIN | **No.** There is no verified boot. Planned: signed updates (#8), which only protect the update path, not someone with fastboot access. |
| **A7. Code running as root on the phone** | Reads memory | **No.** There is no TEE or process isolation that would stop it. |
| **A8. Another app** | Reads or fakes the confirmation | **Not applicable by design:** there are no apps. |

## The PIN KDF (this change)

**Before.** The verifier was `SHA-256(salt, PIN)` followed by 50,000 more SHA-256 rounds. It used almost
no memory, so it parallelises very well on GPUs and ASICs. The PIN was also **cut at 16 bytes**.

**Now.** `scrypt(PIN, salt, N=2^15, r=8, p=1)` (RFC 7914), 32 MiB per guess, then HKDF-SHA256 splits the
result into **two independent keys**:

1. the **verifier**, which is stored and used to check the PIN;
2. the **state key**, which is **never stored** and only exists in memory while the phone is unlocked.

Knowing the verifier does not give the state key. The PIN is no longer truncated (limit: 64 bytes). The
stored cost is read back from the state and refused if it is absurd, so a corrupted state cannot ask for
gigabytes. Old `P ...` state still unlocks, and is upgraded to `P2 ...` at the next correct unlock.

Why scrypt and not Argon2id: scrypt needs only SHA-256, HMAC and PBKDF2, which the code base already
had, plus Salsa20/8. It has official test vectors (RFC 7914), and the implementation here is checked
against them and against OpenSSL. Argon2id is newer but needs BLAKE2b and much more code to audit. The
stored format carries `version | cost`, so moving to Argon2id later does not need a wipe.

## The numbers, and where they come from

Measured with `make bench-kdf`, on **one core** of a cloud x86-64 computer (**not the phone, not a GPU**).
Runs vary; these are two runs. Do not trust them for the phone: the phone is slower, and has to be
measured (see "Not yet done").

| Verifier | Time per guess (1 core) | All 10^6 six-digit PINs, 1 core |
|---|---|---|
| Legacy, SHA-256 x 50,000 | about 17 to 18 ms | about 5 hours |
| scrypt N=2^15, r=8 (default) | about 118 to 141 ms | about 33 to 39 hours |

How to read this, honestly:

- On a plain CPU core the new verifier is only about **7 times** slower per guess. That alone is a small
  gain.
- The real gain is against **parallel hardware**. The old verifier needed almost no memory, so a GPU could
  run thousands of guesses at once. scrypt needs 32 MiB per guess, which limits how many fit in a GPU's
  memory and bandwidth. **How much that helps was not measured here.** Do not quote a GPU speed-up from
  this document.
- A 6-digit PIN is 10^6 possibilities. 39 hours on one core is **about 23 minutes on 100 cores**. That is
  still short. **The cost of the KDF cannot rescue a short PIN.**

Search space, by arithmetic only (time = guesses x 0.14 s on one core, rounded):

| Secret | Possibilities | One core | 1,000 cores |
|---|---|---|---|
| 6 digits | 10^6 | about 1.6 days | about 2.3 minutes |
| 8 digits | 10^8 | about 5 months | about 3.9 hours |
| 10 digits | 10^10 | about 45 years | about 16 days |
| 4 random words (7,776-word list) | about 3.7 x 10^15 | about 1.6 x 10^7 years | about 1.6 x 10^4 years |

**Recommendation:** where real value is at stake, use a passphrase of random words, not a 6-digit PIN.
The storage format already allows it (up to 64 bytes). The lock screen currently asks for 6 digits; that
UI change is not done.

## Not yet done (please do not assume these)

- **The cost on the phone is not measured.** The default (2^15, 32 MiB) is a starting point. It must be
  timed on the X669C (`make bench-kdf` cross-compiled), aiming at about 0.5 to 1 s, never above 2 s. The
  unlock screen **freezes while the KDF runs**, as the UI is single-threaded.
- **The failed-attempt counter is only as strong as the state area behind it.** It is saved after each try when the state area is armed, **not** in read-only images, and it is written after the check instead of before (issue #9). Rollback of the state by someone who edits storage offline is not solved. Without these, A1 is weaker than it looks.
- **Rollback of the state** by someone who edits storage offline is not solved (issue #9).
- **Hardware-backed keys** do not exist on this device.
- The lock screen still takes **6 digits only**.
- **Memory wiping** is done for the keys and intermediate values in `sec.c` (`sec_wipe`), but not audited
  for the rest of the system, and the compiler or the kernel (swap, core dumps) can still leave copies.

## How to report a problem

Open an issue. Do not post anything taken from a real device (keys, dumps, images).

---

# Português

## O resumo honesto

- O celular **não tem cofre de chaves em hardware (sem TEE), nem verified boot, e o bootloader está
  destravado**.
- Então quem estiver com o celular na mão pode copiar o armazenamento e atacar o PIN **offline**, com
  quantos computadores quiser. Nenhum software no celular impede isso.
- A única coisa que atrapalha esse ataque é **quanto custa cada tentativa de PIN** e **quantos PINs
  possíveis existem**. Um PIN de 6 dígitos tem só um milhão de possibilidades. **Ele não vai parar um
  atacante determinado que consiga o armazenamento.**
- A promessa é **"reduzir a superfície de ataque"**, e não "inhackeável".

## O que se protege

| Ativo | Hoje | Onde está |
|---|---|---|
| Verificador do PIN | existe | `PIN` em `init/sec.c`, gravado na linha de estado `P2 ...` (`P ...` antes da migração) |
| Chave do estado (para cifrar o estado e, depois, a chave da carteira) | existe só na memória | `pin_state_key()`, só enquanto desbloqueado; nunca gravada |
| Chave / semente da carteira | ainda não existe | issue #1 |
| Saldo e transações | dados de demonstração | `init/money.c` |

## Quem pode atacar, e o que segura

| Atacante | O que consegue | Defendido? |
|---|---|---|
| **A1. Ladrão com o celular bloqueado, sem ferramentas** | Digita PINs na tela | **Sim, em parte.** 5 erros bloqueiam, com espera que dobra até cerca de 32 minutos. O contador é gravado com o estado depois de cada tentativa, mas **só quando a área de estado está armada** (`plat_save()` logo depois do `pin_check()`); as imagens de teste somente leitura o mantêm na memória, então reiniciar o zera ali. Ele é gravado **depois** de conferir o PIN, então faltar energia durante a conferência pode perder uma tentativa, e quem consegue editar o armazenamento pode restaurar uma cópia antiga (issue #9). |
| **A2. Ladrão com o celular e ferramentas** (bootloader destravado, lê o armazenamento) | Copia o verificador e testa PINs offline, sem bloqueio | **Só pelo custo do KDF e pelo tamanho do PIN.** Veja os números abaixo. Um PIN de 6 dígitos cai. |
| **A3. Host USB malicioso** | Manda comandos pela serial | **Sim.** O único comando (`REBOOT-BOOTLOADER`) só existe em imagens de teste (`bankphone.devcmd=1`); a imagem de produção não aceita nenhum. O leitor passou por fuzzing. |
| **A4. Alguém olhando a tela** | Lê o PIN enquanto é digitado | **Não.** O teclado é fixo. |
| **A5. Dono obrigado a desbloquear** | É forçado a desbloquear e mandar dinheiro | **Não.** Planejado: PIN de coação (#3), cofre com tempo (#7). |
| **A6. Imagem do sistema adulterada** | Grava uma imagem modificada que registra o PIN | **Não.** Não há verified boot. Planejado: atualização assinada (#8), que só protege o caminho de atualização, não quem tem acesso ao fastboot. |
| **A7. Código rodando como root no celular** | Lê a memória | **Não.** Não há TEE nem isolamento de processos que impeça. |
| **A8. Outro app** | Lê ou falsifica a confirmação | **Não se aplica, por desenho:** não há apps. |

## O KDF do PIN (esta mudança)

**Antes.** O verificador era `SHA-256(sal, PIN)` mais 50.000 rodadas de SHA-256. Quase não usava memória,
então paraleliza muito bem em GPU e ASIC. O PIN também era **cortado em 16 bytes**.

**Agora.** `scrypt(PIN, sal, N=2^15, r=8, p=1)` (RFC 7914), 32 MiB por tentativa, e depois o HKDF-SHA256
separa o resultado em **duas chaves independentes**:

1. o **verificador**, que é gravado e usado para conferir o PIN;
2. a **chave do estado**, que **nunca é gravada** e só existe na memória com o celular desbloqueado.

Conhecer o verificador não dá a chave do estado. O PIN não é mais truncado (limite: 64 bytes). O custo
gravado é relido do estado e recusado se for absurdo, então um estado corrompido não consegue pedir
gigabytes. O estado antigo `P ...` ainda desbloqueia e é migrado para `P2 ...` no próximo desbloqueio
certo.

Por que scrypt e não Argon2id: o scrypt só precisa de SHA-256, HMAC e PBKDF2, que o código já tinha, mais
o Salsa20/8. Tem vetores de teste oficiais (RFC 7914), e a implementação daqui é conferida contra eles e
contra o OpenSSL. O Argon2id é mais novo, mas precisa de BLAKE2b e de muito mais código para auditar. O
formato gravado carrega `versão | custo`, então trocar para Argon2id depois não exige apagar nada.

## Os números, e de onde vêm

Medidos com `make bench-kdf`, em **um núcleo** de um computador x86-64 na nuvem (**não é o celular, não é
GPU**). Varia de execução para execução; estas são duas execuções. Não confie neles para o celular: o
celular é mais lento e precisa ser medido (veja "Ainda não feito").

| Verificador | Tempo por tentativa (1 núcleo) | Todos os 10^6 PINs de 6 dígitos, 1 núcleo |
|---|---|---|
| Antigo, SHA-256 x 50.000 | cerca de 17 a 18 ms | cerca de 5 horas |
| scrypt N=2^15, r=8 (padrão) | cerca de 118 a 141 ms | cerca de 33 a 39 horas |

Como ler isso, com honestidade:

- Num núcleo de CPU comum, o verificador novo é só cerca de **7 vezes** mais lento por tentativa. Isso,
  sozinho, é um ganho pequeno.
- O ganho de verdade é contra **hardware paralelo**. O antigo quase não usava memória, então uma GPU rodava
  milhares de tentativas ao mesmo tempo. O scrypt exige 32 MiB por tentativa, o que limita quantas cabem
  na memória e na banda de uma GPU. **Quanto isso ajuda não foi medido aqui.** Não cite um ganho de GPU a
  partir deste documento.
- Um PIN de 6 dígitos são 10^6 possibilidades. 39 horas num núcleo são **cerca de 23 minutos em 100
  núcleos**. Continua curto. **O custo do KDF não salva um PIN curto.**

Espaço de busca, só por conta (tempo = tentativas x 0,14 s num núcleo, arredondado):

| Segredo | Possibilidades | 1 núcleo | 1.000 núcleos |
|---|---|---|---|
| 6 dígitos | 10^6 | cerca de 1,6 dia | cerca de 2,3 minutos |
| 8 dígitos | 10^8 | cerca de 5 meses | cerca de 3,9 horas |
| 10 dígitos | 10^10 | cerca de 45 anos | cerca de 16 dias |
| 4 palavras aleatórias (lista de 7.776) | cerca de 3,7 x 10^15 | cerca de 1,6 x 10^7 anos | cerca de 1,6 x 10^4 anos |

**Recomendação:** onde houver valor de verdade em jogo, use uma frase com palavras aleatórias, e não um
PIN de 6 dígitos. O formato gravado já permite (até 64 bytes). A tela de bloqueio hoje pede 6 dígitos;
essa mudança de interface não foi feita.

## Ainda não feito (não suponha que está)

- **O custo no celular não foi medido.** O padrão (2^15, 32 MiB) é um ponto de partida. Precisa ser
  cronometrado no X669C (`make bench-kdf` compilado para ele), mirando cerca de 0,5 a 1 s, nunca mais de
  2 s. A tela de desbloqueio **trava enquanto o KDF roda**, porque a interface tem uma thread só.
- **O contador de tentativas erradas só vale tanto quanto a área de estado que o guarda.** Ele é gravado a cada tentativa quando a área está armada, **não** nas imagens somente leitura, e é escrito depois da conferência e não antes (issue #9). O rollback do estado por quem edita o armazenamento offline não está resolvido. Sem isso, o A1 é mais fraco do que
  parece.
- **Rollback do estado** por quem edita o armazenamento offline não está resolvido (issue #9).
- **Chaves em hardware** não existem neste aparelho.
- A tela de bloqueio ainda aceita **só 6 dígitos**.
- **Apagar da memória** é feito para as chaves e valores intermediários do `sec.c` (`sec_wipe`), mas não foi
  auditado no resto do sistema, e o compilador ou o kernel (swap, core dump) ainda podem deixar cópias.

## Como relatar um problema

Abra uma issue. Não publique nada tirado de um aparelho de verdade (chaves, dumps, imagens).
