# [P2] `plat_boot_prop` lê a cmdline pelo primeiro trecho parecido: depende da ordem e confunde prefixos

**Labels sugeridos:** `priority:P2`, `area:boot`, `type:bug`, `good first issue`
**Tipo:** defeito confirmado (F-02), **reproduzido**
**Precisa do Infinix Hot 30i:** não
**EN:** `plat_boot_prop` matches the first similar substring in the kernel command line: order-dependent and prefix-confused.

## Contexto
As opções do sistema vêm da cmdline do kernel: `bankphone.ro`, `bankphone.state`, `bankphone.statehash`, `bankphone.devcmd`, `bankphone.fastboot`, `bankphone.fwkick`, `bankphone.touchdbg`, `bankphone.touchbox`, `bankphone.cpuboost`. A trava `bankphone.ro=1` é a que impede qualquer gravação.

## Estado atual
`init/main.c:858-865`:
```c
char *p = strstr(cl, key); size_t kl = strlen(key);
if (p && p[kl] == '=') { ... copia o valor ... }
```
Só considera a **primeira** ocorrência do texto da chave.

**Reproduzido** com uma cópia da função num programa de teste (fora do repositório):

| cmdline | consulta | resultado |
|---|---|---|
| `... bankphone.state=expdb bankphone.statehash=0123... ` | `bankphone.state` | `expdb` (certo) |
| `... bankphone.statehash=0123... bankphone.state=expdb` | `bankphone.state` | **vazio** (errado: o 1º `strstr` achou `statehash`) |
| `... xbankphone.ro=1` | `bankphone.ro` | `1` (errado: casou dentro de outra palavra) |
| `... androidboot.bankphone.ro=1` | `bankphone.ro` | `1` (errado) |

## Problema
- A persistência é ignorada em silêncio se `statehash` vier antes de `state` (falha para o lado seguro, mas sem aviso).
- Uma chave pode casar dentro de outra palavra. Hoje nenhuma chave existente é prefixo de `ro`, então a trava não falha, mas depende de sorte.

## Objetivo
Leitura por **token inteiro** separado por espaço, com chave exata, independente da ordem.

## Tarefas
- [ ] Extrair a lógica para uma função pura (`cmdline_get(const char *cl, const char *key, char *out, size_t n)`) em um header testável.
- [ ] Percorrer tokens separados por espaço/tab/nova linha e comparar `chave=` exata; usar a **última** ocorrência (como o kernel faz para parâmetros repetidos) e documentar a escolha.
- [ ] Tratar valor vazio, `chave` sem `=`, valor maior que o buffer e cmdline de 4096 bytes.
- [ ] Testes de host com a tabela acima e casos extras (ordem, prefixo, repetição, truncamento).
- [ ] Adicionar um alvo de fuzzing simples para a função.
- [ ] Registrar no log (`bd_log`) quando uma chave `bankphone.*` for ignorada por estar mal formada.

## Critérios de aceite
- [ ] Os quatro casos da tabela acima dão o resultado correto.
- [ ] `bankphone.ro=1` só é reconhecida como token inteiro.
- [ ] Testes e fuzzer passam sob ASan/UBSan.

## Testes
Host (tabela, bordas, fuzz).

## Riscos
Baixo. Manter o mesmo comportamento para cmdlines que hoje funcionam.

## Dependências
Nenhuma.

## Evidências
`init/main.c:858-865`, experimento descrito acima, `docs/PROJECT_AUDIT.md` (F-02).

## Fora do escopo
Mudar os nomes das opções.

## Definição de concluído
Função extraída, testes e fuzzer mesclados, comportamento documentado.
