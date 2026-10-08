#!/bin/bash
# Espera o fastboot, grava SOMENTE boot_b com a imagem de teste (somente leitura), ativa b, reinicia
# e salva a saída serial do BANKPHONE OS em work/serial-capture.txt. boot_a (sistema original) não é tocado.
cd "$(dirname "$0")/.."
[ -f local.env ] && . ./local.env   # opcional: PLATFORM_TOOLS=/caminho/do/platform-tools
[ -n "${PLATFORM_TOOLS:-}" ] && export PATH="$PLATFORM_TOOLS:$PATH"
command -v fastboot >/dev/null || { echo "ERRO: fastboot não encontrado (defina PLATFORM_TOOLS em local.env)."; exit 1; }
IMG=work/bankphone-os-ro.img
echo "aguardando fastboot..."
until fastboot devices 2>/dev/null | grep -q fastboot; do sleep 1; done
echo "fastboot: $(fastboot devices)"
fastboot getvar current-slot 2>&1 | head -1
fastboot flash boot_b "$IMG" || { echo "FALHOU ao gravar boot_b; nada mais foi feito"; exit 1; }
fastboot set_active b && fastboot reboot
echo "GRAVADO em boot_b e reiniciando; lendo serial..."
python3 - <<'PY'
import os, glob, termios, time, select
t0 = time.time(); fd = None
while time.time() - t0 < 90 and fd is None:
    for p in glob.glob('/dev/cu.usbmodem*'):
        try: fd = os.open(p, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK); break
        except OSError: pass
    time.sleep(0.5)
if fd is None: print("serial não apareceu"); raise SystemExit
a = termios.tcgetattr(fd); a[0] = a[1] = a[3] = 0; a[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
a[4] = a[5] = termios.B115200; termios.tcsetattr(fd, termios.TCSANOW, a)
out = open('work/serial-capture.txt', 'wb'); t = time.time()
while time.time() - t < 60:
    if select.select([fd], [], [], 0.5)[0]:
        try: d = os.read(fd, 4096)
        except (BlockingIOError, OSError): continue
        out.write(d); out.flush()
print("serial salva em work/serial-capture.txt")
PY
