#!/bin/sh
# Runs the Python tests of tools/mkboot.py and tools/mkramdisk.py. Nothing touches a device.
# Roda os testes em Python do tools/mkboot.py e do tools/mkramdisk.py. Nada toca um aparelho.
cd "$(dirname "$0")/../.." || exit 1
command -v python3 >/dev/null 2>&1 || { echo "FAIL python3 nao encontrado"; exit 1; }
python3 tests/image_tools_test.py
