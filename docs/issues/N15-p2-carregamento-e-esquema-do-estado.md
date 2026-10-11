# [P2] Carregamento do estado: tornar idempotente, validar o esquema e não ignorar erros

**Labels sugeridos:** `priority:P2`, `area:storage`, `area:finance`, `type:bug`
**Tipo:** defeitos confirmados (F-03) + lacunas de validação
**Precisa do Infinix Hot 30i:** não (a validação no aparelho vem depois)
**EN:** State loading: make it idempotent, validate the schema, and stop ignoring errors.

## Contexto
O `store` grava e lê com segurança contra queda de energia (dois slots, PENDING, CRC; 171 verificações no host). O que fica **depois** da leitura, isto é, a interpretação do texto salvo, é mais frágil.

## Estado atual
- `load_state()` (`init/main.c:875-918`) chama `pin_deserialize(buf); m_deserialize(nl + 1);` e **ignora os dois retornos** (`:907`).
- É chamado em `init/main.c:1033` ou `:1040` e **de novo** em `:1163` (retentativa quando a UI sobe depois do modo diagnóstico).
- `store_open()` atribui `sfd = open(...)` (`init/store.c:228`, `:240`) sem fechar o anterior. **Reproduzido:** duas aberturas bem-sucedidas deixam `fds +2`, e depois de `store_close()` ainda `fds +1`.
- `m_deserialize` (`init/money.c:174-195`) valida só o tipo (0 a 3) e o estado (0 a 10). **Não valida** `from`/`to` (moeda), sinal nem faixa dos valores, nem coerência entre campos. Já tem fuzzer e uma correção de leitura fora do buffer (PR #12).
- O estado não tem versão nem cabeçalho de esquema; a linha de PIN passa a ter versão no PR #13.
- Integridade = CRC32 no slot. **CRC detecta corrupção acidental; não autentica e não impede adulteração deliberada.**

## Problema
1. Estado válido para o CRC, mas semanticamente inválido (valor negativo, moeda 99), entra no livro-razão e pode virar saldo.
2. Chamar `load_state()` duas vezes vaza descritor e pode reabrir a área.
3. Falha de leitura/interpretação não aparece na tela nem no log.

## Objetivo
Carregamento que é seguro repetir, que **rejeita** estado inválido de forma previsível e que diz o que aconteceu.

## Tarefas
- [ ] Fazer `store_open()` fechar o descritor anterior (ou recusar reabertura) e `load_state()` ser idempotente (guardar "já carregado").
- [ ] Definir um **cabeçalho de esquema** do estado (`V<n>`), aceitar versões conhecidas e recusar as desconhecidas, com mensagem.
- [ ] Validar cada campo em `m_deserialize`: moeda em `{A_BRL, A_USDC, A_NONE}`, valores `≥ 0` e abaixo do máximo, tipo coerente com moedas, contagem ≤ `MAX_TX`, `seq` monótono.
- [ ] Propagar erro: se o estado for inválido, **não** aplicar parcialmente; manter o estado vazio e registrar o motivo no log e no diagnóstico.
- [ ] Testes: arquivo truncado, versão desconhecida, valor negativo, moeda inválida, duplicata de id, estado com 401 transações, `load_state` chamado duas vezes (contagem de descritores).
- [ ] Estender o fuzzer do motor para exigir que um estado aceito sempre satisfaça as invariantes (saldos ≥ 0).
- [ ] Documentar o formato e a migração em `docs/STATE_FORMAT.md` e diferenciar **detecção de corrupção** de **proteção contra adulteração** (esta fica no #9).

## Critérios de aceite
- [ ] Corrupção detectada nunca vira saldo válido silenciosamente.
- [ ] Chamadas repetidas de `load_state` não vazam descritores (teste).
- [ ] Todo caminho de erro de leitura aparece no log.
- [ ] Documentação do formato publicada.

## Testes
Host: novos casos em `tests/money_test.c` e `init/tests/test_store.c`; fuzzer; `make test-asan`.

## Riscos
Recusar estados que hoje carregam. Fazer a migração aceitar o formato atual sem versão como `V1`.

## Dependências
Relacionada ao PR #13 (formato de PIN versionado) e ao #9 (cifra e rollback). N11 (invariantes) define o que é "válido".

## Evidências
`init/main.c:875-918`, `:1033`, `:1040`, `:1163`, `init/store.c:228`, `:240`, `init/money.c:174-195`, experimento do descritor, `docs/PROJECT_AUDIT.md` (F-03).

## Fora do escopo
Cifrar o estado, proteger contra rollback e relógio monotônico (#9).

## Definição de concluído
Carregamento idempotente e validado, testes e documentação mesclados.
