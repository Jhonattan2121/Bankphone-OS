#!/bin/bash
# BANKPHONE OS: compila init, monta ramdisk e gera work/bankphone-os.img (kernel e DTB do aparelho + userspace nosso).
set -e
cd "$(dirname "$0")"

# Configuração local (opcional): crie um arquivo local.env (fora do git) com
#   ZIG=/caminho/do/zig
#   STOCK_BOOT=/caminho/do/boot_a.img   <- extraído do SEU aparelho (nunca distribuído aqui)
[ -f local.env ] && . ./local.env
Z="${ZIG:-$(command -v zig || true)}"
[ -x "$Z" ] || { echo "ERRO: zig não encontrado. Instale o zig ou defina ZIG em local.env."; exit 1; }
[ -n "${STOCK_BOOT:-}" ] && [ -f "$STOCK_BOOT" ] || { echo "ERRO: defina STOCK_BOOT (boot.img do seu próprio aparelho) em local.env."; exit 1; }
mkdir -p work rootfs-extra
[ -f rootfs-extra/vendor/firmware/novatek_ts_fw.bin ] || echo "AVISO: firmware do toque ausente. Rode tools/get-touch-firmware.sh (o build segue, mas a tela NÃO responde ao toque)."
$Z cc -target aarch64-linux-musl -mcpu=cortex_a53 -static -O2 -s -Wall -Wextra -Wno-unused-function -Wno-unused-parameter -o work/init init/main.c init/gfx.c init/money.c init/sec.c init/store.c init/ui.c init/screens.c init/sheets.c init/components.c init/icons.c init/hw.c init/bootdiag.c -lm
[ -d work/stock ] || python3 tools/mkboot.py unpack "$STOCK_BOOT" work/stock
python3 tools/mkramdisk.py work/init work/ramdisk.gz assets rootfs-extra
# remove depuração de memória que deixa o kernel lento (substitui por espaços no DTB, mesmo tamanho)
python3 - <<'PY'
p = "work/stock/dtb"; o = p + ".orig"
import os
if not os.path.exists(o): open(o, "wb").write(open(p, "rb").read())
a = b"slub_debug=OFZPU page_owner=on"; open(p, "wb").write(open(o, "rb").read().replace(a, b" " * len(a)))
PY
CMD=$(python3 -c "import json;c=json.load(open('work/stock/header.json'))['cmdline'];print(' '.join(t for t in c.split() if not t.startswith(('slub_debug','page_owner'))))")
python3 tools/mkboot.py pack work/stock work/bankphone-os.img --ramdisk work/ramdisk.gz --cmdline "$CMD"
ls -la work/bankphone-os.img
