# [P1] O relatório de boot grava em `expdb` por padrão, protegido só por lista de nomes

**Labels sugeridos:** `priority:P1`, `area:storage`, `area:recovery`, `area:security`, `type:bug`
**Tipo:** risco confirmado no código (F-05); efeito real no aparelho **não verificado**
**Precisa do Infinix Hot 30i:** parcialmente (para medir desgaste e conteúdo de `expdb`)
**EN:** The boot report writes to the raw `expdb` partition by default, guarded only by a name blocklist.

## Contexto
O relatório persistente do `bootdiag` guarda o log do boot numa partição bruta para sobreviver a reinícios. O `store` (estado) segue regras bem mais estritas para o mesmo tipo de escrita.

## Estado atual
- `bd_report_open(NULL)` usa `"expdb"` (`init/bootdiag.c:331-337`), na posição `BD_BASE` = 8 MiB (`:230`).
- Proteção: lista de bloqueio por nome `g_deny` (`:242-258`). Qualquer partição **fora** da lista é aceita.
- Se `BLKGETSIZE64` falhar, segue **sem checar o tamanho** (`:363`).
- Não exige o hash do backup da região, ao contrário do `store` (regra R3 em `init/store.h`).
- `bd_flush()` (`:398`) faz `pwrite` de até 64 KiB e `fsync`. É chamada em **34 pontos** do código.
- A imagem somente leitura (`bankphone.ro=1`) **não abre** o relatório (`init/main.c:961-967`) e é a única que o `install.sh` grava.
- O risco aparece com a imagem completa `work/bankphone-os.img` (gerada por `./build.sh`).

## Problema
1. A lista de bloqueio é frágil (nome novo ou diferente passa).
2. Sem checagem de tamanho se o `ioctl` falhar, a escrita pode passar do fim da partição.
3. Sem requisito de backup, a primeira escrita pode sobrescrever conteúdo real de `expdb` na região usada.
4. Escrita com `fsync` a cada `bd_flush` pode causar desgaste da eMMC (**não medido**).

## Objetivo
O relatório só grava numa área que o projeto **assumiu** de forma verificada, com a mesma disciplina do `store`, e com frequência de escrita limitada.

## Tarefas
- [ ] Trocar a lista de bloqueio por **lista de permissão**: o relatório só grava na partição informada de forma explícita pela cmdline (como o `store`), nunca por padrão.
- [ ] Recusar a escrita se o tamanho não puder ser lido.
- [ ] Exigir o hash do backup da região antes da primeira escrita (reutilizar a lógica e o script `scripts/state-region-hash.sh`) ou usar uma região separada, com assinatura própria.
- [ ] Limitar a frequência de `bd_flush` (por exemplo, no máximo uma gravação a cada N segundos, mais em eventos críticos) e contar gravações no log.
- [ ] No aparelho: ler o conteúdo de `expdb` na região usada e registrar se ela estava vazia; estimar gravações por boot.
- [ ] Testes de host com backend de arquivo (como `init/tests/test_store.c`) para: partição recusada, tamanho desconhecido, primeira escrita sem hash, limite de frequência.

## Critérios de aceite
- [ ] Sem `bankphone.report=<partição>` (nome a definir) na cmdline, nenhuma escrita acontece.
- [ ] Tamanho desconhecido recusa a escrita.
- [ ] Primeira escrita exige o hash do backup (ou outra prova equivalente) e registra os dois hashes.
- [ ] Número de gravações por boot documentado.
- [ ] README explica qual imagem grava o quê.

## Testes
Host: backend de arquivo com injeção de queda; `make test`, `make test-asan`. Hardware: leitura de `expdb` e contagem de gravações por boot.

## Riscos
Mudar o comportamento de diagnóstico que o autor usa hoje. Manter a imagem somente leitura inalterada e documentar a nova opção.

## Dependências
N01 (coleta de evidência usa esse relatório); N02 (recuperação).

## Evidências
`init/bootdiag.c:230`, `:242-258`, `:331-337`, `:363`, `:398-420`, `init/main.c:961-967`, `init/store.h` (regra R3).

## Fora do escopo
Mudar o formato do relatório; mexer no `store`.

## Definição de concluído
Relatório com a mesma disciplina de segurança do `store`, testes mesclados e contagem de escritas documentada.
