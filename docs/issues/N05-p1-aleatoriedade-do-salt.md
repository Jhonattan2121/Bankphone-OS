# [P1] `plat_random` cai para bytes previsíveis sem avisar e não confere leitura curta

**Labels sugeridos:** `priority:P1`, `area:security`, `type:bug`
**Tipo:** defeito confirmado (F-04), lido no código
**Precisa do Infinix Hot 30i:** não
**EN:** `plat_random` silently falls back to predictable bytes and ignores short reads; it produces the PIN salt.

## Contexto
O salt do PIN precisa ser imprevisível. Ele vem de `plat_random`.

## Estado atual
`init/main.c:848`:
```c
void plat_random(uint8_t *b, int n) { int f = open("/dev/urandom", O_RDONLY);
  if (f >= 0) { if (read(f, b, n) < 0) {} close(f); }
  else for (int i = 0; i < n; i++) b[i] = (uint8_t)(boot_mono() * 31 + i * 17); }
```
- Se `/dev/urandom` não abrir, usa uma conta com o relógio monotônico (previsível).
- Ignora o retorno de `read` (leitura curta deixa o resto do buffer sem preencher).
- É chamada ao definir o PIN (`init/ui.c:219-220`).

## Problema
Um salt previsível enfraquece o PIN sem nenhum aviso para a pessoa nem no log.

## Objetivo
`plat_random` ou entrega bytes aleatórios de verdade, ou **falha de forma visível**, e o chamador recusa definir o PIN.

## Tarefas
- [ ] Usar `getrandom(2)` quando existir (bloqueando até o pool estar pronto) e `/dev/urandom` como alternativa, conferindo que **todos** os bytes foram lidos.
- [ ] Remover o preenchimento previsível. Em caso de falha, devolver erro.
- [ ] Mudar a assinatura para devolver sucesso/falha e fazer `ui.c` recusar a criação do PIN, com aviso e log (`bd_log`), se falhar.
- [ ] Documentar o comportamento logo após o boot (pool de entropia).
- [ ] Teste de host com uma fonte de aleatoriedade injetada que falha.

## Critérios de aceite
- [ ] Não existe caminho que preencha o salt com valor derivado do relógio.
- [ ] Falha de entropia aparece na tela e no log, e o PIN não é definido.
- [ ] Teste de host cobre falha de abertura e leitura curta.

## Testes
Host: injeção de falha na leitura; `make test-asan` verde.

## Riscos
Bloquear a criação do PIN se o pool de entropia demorar. Medir no aparelho; em caso de espera longa, mostrar "aguardando entropia".

## Dependências
Relacionada ao PR #13 (scrypt), que usa o salt, mas independente dele.

## Evidências
`init/main.c:848`, `init/ui.c:219-220`, `docs/PROJECT_AUDIT.md` (F-04).

## Fora do escopo
Carteira e geração de chaves (#1), que terão requisitos próprios de entropia.

## Definição de concluído
Código e teste mesclados, comportamento documentado.
