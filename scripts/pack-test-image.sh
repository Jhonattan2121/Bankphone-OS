#!/bin/bash
# BANKPHONE OS — empacota a imagem de TESTE (somente leitura) para `fastboot boot`.
#
# O que este script faz:
#   - reaproveita work/stock (kernel + dtb de fábrica) e work/ramdisk.gz já gerados;
#   - acrescenta "bankphone.ro=1" à cmdline => o init NÃO grava nenhuma partição;
#   - gera work/bankphone-os-ro.img e imprime o sha256.
#
# O que ele NÃO faz (por política):
#   - não grava em nenhuma partição, não chama fastboot, não toca no seu
#     work/bankphone-os.img, mkboot.py, mkramdisk.py nem build.sh.
#
# Uso:  bash scripts/pack-test-image.sh [raiz-do-repo]
set -euo pipefail

REPO="${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
cd "$REPO"

err() { echo "ERRO: $*" >&2; exit 1; }

[ -f tools/mkboot.py ] || err "tools/mkboot.py não encontrado (rode na raiz do repo)"
[ -d work/stock ] || err "work/stock não existe — rode ./build.sh pelo menos uma vez para extrair o boot_a de fábrica"
[ -f work/stock/header.json ] || err "work/stock/header.json não existe (extração incompleta)"
[ -f work/ramdisk.gz ] || err "work/ramdisk.gz não existe — rode ./build.sh (ele gera o ramdisk)"
[ -f work/init ] || err "work/init não existe — rode ./build.sh"

# A cmdline do teste: a mesma de fábrica, sem as depurações de memória, + somente leitura.
CMD="$(python3 - <<'PY'
import json
c = json.load(open("work/stock/header.json"))["cmdline"]
c = " ".join(t for t in c.split() if not t.startswith(("slub_debug", "page_owner")))
print((c + " bankphone.ro=1").strip())
PY
)"

echo "cmdline do teste: $CMD"
python3 tools/mkboot.py pack work/stock work/bankphone-os-ro.img --ramdisk work/ramdisk.gz --cmdline "$CMD"

echo
echo "== imagem de teste =="
ls -l work/bankphone-os-ro.img
if command -v sha256sum >/dev/null 2>&1; then sha256sum work/bankphone-os-ro.img; else shasum -a 256 work/bankphone-os-ro.img; fi
echo
echo "Conferência independente (recomendada):"
echo "  bankphonectl ingest work/bankphone-os-ro.img --deep"
echo
echo "Teste em RAM, sem gravar nada:"
echo "  fastboot boot work/bankphone-os-ro.img"
