#!/bin/bash
# BANKPHONE OS: grava a imagem de TESTE (somente leitura) no slot A/B INATIVO, ativa esse slot e reinicia.
# Flashes the READ-ONLY test image to the INACTIVE A/B slot, activates it and reboots.
#
# Nunca grava no slot ativo (o seu caminho de volta). O slot de destino é DESCOBERTO no aparelho, não
# presumido: se o slot ativo não puder ser lido, o script recusa e não grava nada.
# It never writes to the active slot (your way back). The target slot is READ from the phone, not
# assumed: if the active slot cannot be read, the script refuses and writes nothing.
#
# Uso / Usage:
#   tools/test_boot_b.sh [--dry-run] [--image ARQUIVO] [--wait SEGUNDOS] [--capture]
#
#   --dry-run   faz tudo, MENOS gravar, ativar ou reiniciar / does everything except flash, activate, reboot
#   --image     imagem de teste (padrão work/bankphone-os-ro.img) / test image
#   --wait      quanto esperar o fastboot (padrão 60 s; nunca espera para sempre) / how long to wait
#   --capture   depois de reiniciar, salva a serial em work/serial-capture.txt / save the serial output
#
# Para instalar normalmente use ./install.sh, que tem mais checagens. / For a normal install use ./install.sh.
set -u
cd "$(dirname "$0")/.."

DRY=0; CAPTURE=0; WAIT=60; IMG="work/bankphone-os-ro.img"
while [ $# -gt 0 ]; do
  case "$1" in
    --dry-run) DRY=1; shift ;;
    --capture) CAPTURE=1; shift ;;
    --image) IMG="${2:-}"; [ -n "$IMG" ] || { echo "ERRO: --image precisa de um arquivo"; exit 2; }; shift 2 ;;
    --wait) WAIT="${2:-}"; case "$WAIT" in ''|*[!0-9]*) echo "ERRO: --wait precisa de um número de segundos"; exit 2 ;; esac; shift 2 ;;
    -h|--help) sed -n 2,16p "$0"; exit 0 ;;
    *) echo "ERRO: opção desconhecida: $1 (use --help)"; exit 2 ;;
  esac
done

die() { echo "ERRO: $*" >&2; exit 1; }
say() { echo "==> $*"; }

[ -f local.env ] && . ./local.env   # opcional: PLATFORM_TOOLS=/caminho/do/platform-tools
[ -n "${PLATFORM_TOOLS:-}" ] && export PATH="$PLATFORM_TOOLS:$PATH"
command -v fastboot >/dev/null 2>&1 || die "fastboot não encontrado (defina PLATFORM_TOOLS em local.env)."

# ---------------------------------------------------------------- 1. a imagem
[ -e "$IMG" ] || die "imagem não encontrada: $IMG (rode ./build.sh e scripts/pack-test-image.sh). Nada foi gravado."
[ -f "$IMG" ] || die "'$IMG' não é um arquivo comum. Nada foi gravado."
[ -s "$IMG" ] || die "a imagem '$IMG' está vazia. Nada foi gravado."
[ "$(head -c 8 "$IMG" 2>/dev/null)" = "ANDROID!" ] || die "'$IMG' não parece uma imagem de boot (falta o cabeçalho ANDROID!). Nada foi gravado."
if command -v sha256sum >/dev/null 2>&1; then SHA="$(sha256sum "$IMG" | cut -d' ' -f1)"
elif command -v shasum >/dev/null 2>&1; then SHA="$(shasum -a 256 "$IMG" | cut -d' ' -f1)"
else die "nem sha256sum nem shasum encontrados: não dá para registrar o hash da imagem. Nada foi gravado."; fi
say "imagem: $IMG ($(wc -c < "$IMG" | tr -d ' ') bytes)"
say "sha256: $SHA"

# ---------------------------------------------------------------- 2. esperar o fastboot (com limite)
say "aguardando o fastboot por até ${WAIT} s..."
found=0; waited=0
while :; do
  if fastboot devices 2>/dev/null | grep -q fastboot; then found=1; break; fi
  [ "$waited" -ge "$WAIT" ] && break
  sleep 1; waited=$((waited + 1))
done
[ "$found" -eq 1 ] || die "o aparelho não apareceu no fastboot em ${WAIT} s. Nada foi gravado."
say "fastboot: $(fastboot devices 2>/dev/null | head -1)"

# ---------------------------------------------------------------- 3. o slot: LIDO, nunca presumido
getv() { fastboot getvar "$1" 2>&1 | tr -d '\r' | sed -n "s/^$1: *//p" | head -1; }
CNT="$(getv slot-count)"
CUR="$(getv current-slot)"
UNL="$(getv unlocked)"
[ "$CNT" = "2" ] || die "o aparelho não tem 2 slots A/B (slot-count='$CNT'). Nada foi gravado."
case "$CUR" in
  a) TARGET=b ;;
  b) TARGET=a ;;
  *) die "não consegui ler o slot ativo (current-slot='$CUR'). Recuso gravar sem saber qual é o slot em uso. Nada foi gravado." ;;
esac
case "$UNL" in
  no|NO|false) die "o bootloader está TRAVADO. Nada foi gravado." ;;
esac
[ "$TARGET" != "$CUR" ] || die "erro interno: o alvo seria o slot ativo. Nada foi gravado."

cat <<EOF

================ RESUMO ================
  Slot ativo ...... $CUR  (NÃO será alterado: é o seu caminho de volta)
  Vai gravar em ... boot_$TARGET
  Imagem .......... $IMG
  sha256 .......... $SHA
========================================
EOF

# ---------------------------------------------------------------- 4. dry-run e confirmação
if [ "$DRY" -eq 1 ]; then
  say "DRY-RUN: rodaria: fastboot flash boot_$TARGET $IMG && fastboot set_active $TARGET && fastboot reboot"
  say "Nada foi gravado."
  exit 0
fi
PHRASE="FLASH boot_$TARGET"
printf "Para gravar, digite exatamente: %s\n> " "$PHRASE"
ans=""; read -r ans || true
[ "$ans" = "$PHRASE" ] || die "confirmação não confere. Nada foi gravado."

# ---------------------------------------------------------------- 5. gravar
[ "$TARGET" != "$CUR" ] || die "recusado: o alvo seria o slot ativo."
fastboot flash "boot_$TARGET" "$IMG" || die "falha ao gravar boot_$TARGET. O slot $CUR não foi tocado."
fastboot set_active "$TARGET" || die "gravou, mas não consegui ativar o slot $TARGET."
fastboot reboot
say "gravado em boot_$TARGET e reiniciando. Para voltar: coloque no fastboot e rode ./install.sh --restore"

# ---------------------------------------------------------------- 6. captura opcional da serial
[ "$CAPTURE" -eq 1 ] || exit 0
mkdir -p work
python3 - <<'PY'
import os, glob, termios, time, select
t0 = time.time(); fd = None
while time.time() - t0 < 90 and fd is None:
    for p in glob.glob('/dev/cu.usbmodem*') + glob.glob('/dev/serial/by-id/*BANKPHONE*'):
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
