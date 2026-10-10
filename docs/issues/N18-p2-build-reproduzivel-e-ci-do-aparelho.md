# [P2] Build reproduzível dos artefatos de boot e compilação ARM64 no CI

**Labels sugeridos:** `priority:P2`, `area:testing`, `area:boot`, `type:feature`
**Tipo:** engenharia de build
**Precisa do Infinix Hot 30i:** não
**EN:** Reproducible boot artifacts and an ARM64 build in CI.

## Contexto
O CI atual compila e testa só o que roda no computador. O binário do aparelho (`work/init`, ARM64) e as imagens de boot **nunca são geradas pelo CI**, então uma mudança que quebre o build do aparelho só aparece na máquina do autor.

## Estado atual
- `build.sh` compila com `zig cc -target aarch64-linux-musl -mcpu=cortex_a53 -static -O2 -s -Wall -Wextra ...` (`build.sh:15`). A versão do `zig` **não é fixada**. A auditoria não tinha `zig`, então o build ARM64 é **NÃO VERIFICADO**.
- `tools/mkramdisk.py` é determinístico (`gzip.compress(..., mtime=0)`, arquivos ordenados). `tools/mkboot.py` suporta **só** o cabeçalho de boot v2, com `hdr_size = 1660` fixo e `assert` em vez de erros claros (`tools/mkboot.py:12`, `:14`, `:34`; `hdr_size` literal em `:43`).
- `scripts/pack-test-image.sh` imprime o sha256 da imagem final, mas não registra os hashes de `init`, `ramdisk.gz` e `stock/*`.
- Não há testes de `mkboot.py`/`mkramdisk.py`. Não há fixtures.
- O instalador valida que o `boot.img` de origem é `ANDROID!` e não contém "bankphone" (`install.sh`), mas não valida versão de cabeçalho, tamanho nem arquitetura do kernel.

## Problema
- Falhas de build do aparelho passam despercebidas.
- Não dá para dizer se duas pessoas geraram a mesma imagem a partir das mesmas entradas.
- Entradas inválidas (cabeçalho v3, arquivo vazio) falham com `AssertionError` pouco claro.

## Objetivo
CI que compila o `init` para ARM64 e monta uma imagem de teste com **fixtures sintéticas**, e um processo documentado e verificável para reproduzir os artefatos.

## Tarefas
- [ ] Job de CI que instala uma versão **fixada** do `zig` e compila `init` para `aarch64-linux-musl` (sem arquivos do aparelho), confere com `file` que é ELF ARM64 estático e registra o tamanho e o sha256.
- [ ] Criar fixtures **sintéticas** (um `boot.img` v2 de mentira, kernel e DTB falsos) para testar `mkboot.py unpack/pack` e `mkramdisk.py` sem material do aparelho.
- [ ] Testes: ida e volta (`unpack` → `pack` mantém campos e ordem), cmdline longa, ramdisk vazio, assets ausentes, header v3 recusado **com mensagem**.
- [ ] Trocar `assert` por erros claros e códigos de saída.
- [ ] Verificar o conteúdo do ramdisk (lista de arquivos, `init` executável, `assets/Roboto-Regular.ttf`) com um script que falhe se algo faltar.
- [ ] Fazer o build imprimir e salvar um manifesto: versões de `zig`/`python`, flags, e sha256 de cada entrada e saída.
- [ ] Documentar em `docs/BUILD.md`: entradas, saídas, como reconstruir do zero e como comparar hashes.
- [ ] Separar artefatos de **teste** (somente leitura) dos de **instalação**, no nome e no manifesto.
- [ ] Verificar que arquivos ausentes (`STOCK_BOOT`, firmware) produzem erro explícito, não imagem incompleta.

## Critérios de aceite
- [ ] Um PR que quebre o build ARM64 falha no CI.
- [ ] Duas execuções com as mesmas entradas geram o mesmo sha256 do `ramdisk.gz` (e do `init`, se o compilador for determinístico; senão documentar).
- [ ] Os testes das ferramentas Python rodam no CI sem material do aparelho.
- [ ] `docs/BUILD.md` permite a outra pessoa reproduzir a compilação.

## Testes
CI: build ARM64, testes de `mkboot`/`mkramdisk` com fixtures, verificação do ramdisk.

## Riscos
Fixar o `zig` pode exigir manutenção. **Nenhum arquivo do aparelho entra no CI** (regra do repositório).

## Dependências
Independente das issues de hardware. Alimenta N01 e N02.

## Evidências
`build.sh:15`, `tools/mkboot.py:12`, `:14`, `:34`, `:43`, `tools/mkramdisk.py`, `scripts/pack-test-image.sh`, `docs/PROJECT_AUDIT.md` (seção 8).

## Fora do escopo
Assinar imagens (#8) e instalar no aparelho.

## Definição de concluído
CI compilando ARM64, ferramentas testadas com fixtures, guia de build publicado.
