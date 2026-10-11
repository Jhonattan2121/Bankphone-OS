#!/bin/bash
# BANKPHONE OS: installer.
# Recognises the phone over the USB cable and installs the system on the INACTIVE A/B slot.
# It never writes to the active slot (your way back) and always asks for a typed confirmation.
#
# Usage:
#   ./install.sh                        install. The phone can be on Android (adb), already running
#                                       BANKPHONE (USB serial port) or in fastboot: the installer takes it
#                                       to fastboot by itself, with no button pressed.
#   ./install.sh --dry-run              do everything EXCEPT write to the phone
#   ./install.sh --stock-boot FILE      use the boot.img of your phone (fastboot fetch is refused by the X669C)
#   ./install.sh --skip-firmware        do not pull the touch firmware
#   ./install.sh --restore              put the original slot back as active (writes no image)
# Language: BANKPHONE_LANG=en|pt (default follows $LANG).
#
# No warranty. Only tested on the Infinix Hot 30i (X669C). Read the README first.
#
# ---- Português ----
# BANKPHONE OS: instalador. Reconhece o aparelho pelo cabo USB e instala no slot A/B INATIVO.
# Nunca grava no slot ativo (o seu caminho de volta) e sempre pede confirmação digitada.
# Opções: --dry-run (tudo menos gravar), --stock-boot ARQUIVO (o X669C recusa o fastboot fetch),
# --skip-firmware, --restore. Idioma: BANKPHONE_LANG=en|pt. Sem garantia; leia o README antes.
set -u
cd "$(dirname "$0")"

# Language / idioma: BANKPHONE_LANG=pt or en; otherwise follows $LANG (Portuguese if it starts with pt).
case "${BANKPHONE_LANG:-${LANG:-}}" in pt*|PT*) BP_L=pt ;; *) BP_L=en ;; esac
T() { if [ "$BP_L" = pt ]; then printf '%s' "$1"; else printf '%s' "$2"; fi; }

DRY=0; RESTORE=0; STOCK_ARG=""; SKIPFW=0
while [ $# -gt 0 ]; do
  case "$1" in
    --dry-run) DRY=1; shift ;;
    --restore) RESTORE=1; shift ;;
    --skip-firmware) SKIPFW=1; shift ;;
    --stock-boot) STOCK_ARG="${2:-}"; [ -n "$STOCK_ARG" ] || { echo "$(T ERRO ERROR): $(T "--stock-boot precisa de um arquivo" "--stock-boot needs a file")"; exit 2; }; shift 2 ;;
    -h|--help) sed -n 2,17p "$0" | sed "s/^# \{0,1\}//"; exit 0 ;;
    *) echo "$(T ERRO ERROR): $(T "opção desconhecida: $1 (use --help)" "unknown option: $1 (use --help)")"; exit 2 ;;
  esac
done

die()  { echo "$(T ERRO ERROR): $*" >&2; exit 1; }
say()  { echo "==> $*"; }
warn() { echo "$(T AVISO WARNING): $*" >&2; }

[ -f local.env ] && . ./local.env
[ -n "${PLATFORM_TOOLS:-}" ] && PATH="$PLATFORM_TOOLS:$PATH"
export PATH

need() { command -v "$1" >/dev/null 2>&1 || die "$1 $(T "não encontrado." "not found.") $2"; }
need python3 "$(T "Instale o Python 3." "Install Python 3.")"
need adb "$(T "Instale o Android platform-tools (ou defina PLATFORM_TOOLS em local.env)." "Install the Android platform-tools (or set PLATFORM_TOOLS in local.env).")"
need fastboot "$(T "Instale o Android platform-tools (ou defina PLATFORM_TOOLS em local.env)." "Install the Android platform-tools (or set PLATFORM_TOOLS in local.env).")"
if [ "$RESTORE" -eq 0 ]; then
  ZIGBIN="${ZIG:-$(command -v zig || true)}"
  [ -n "$ZIGBIN" ] && [ -x "$ZIGBIN" ] || die "$(T "zig não encontrado. Instale o zig (ziglang.org) ou defina ZIG em local.env." "zig not found. Install zig (ziglang.org) or set ZIG in local.env.")"
fi

STATE="work/.install-state"
mkdir -p work
getv() { fastboot getvar "$1" 2>&1 | tr -d '\r' | sed -n "s/^$1: *//p" | head -1; }
other() { [ "$1" = a ] && echo b || echo a; }
state_get() { [ -f "$STATE" ] && sed -n "s/^$1=//p" "$STATE" | head -1; }

# ------------------------------------------------------------ 1. reconhecer o aparelho
# Porta serial USB do BANKPHONE (quando o aparelho já roda o sistema).
find_bp_port() {
  [ -n "${BANKPHONE_PORT:-}" ] && { echo "$BANKPHONE_PORT"; return; }
  for p in /dev/cu.usbmodemBANKPHONE* /dev/serial/by-id/*BANKPHONE*; do
    [ -e "$p" ] && { echo "$p"; return; }
  done
}

MODE=""; MODEL=""
adb_state="$(adb get-state 2>/dev/null | tr -d '\r')"
BP_PORT="$(find_bp_port)"
if [ "$adb_state" = "device" ]; then
  MODE="android"
  MODEL="$(adb shell getprop ro.product.model 2>/dev/null | tr -d '\r')"
  say "$(T "Aparelho em modo Android: modelo" "Phone in Android mode: model") '${MODEL:-$(T desconhecido unknown)}'"
  echo "$MODEL" | tr a-z A-Z | grep -q "X669C" || die "$(T "o modelo '$MODEL' não é o Infinix Hot 30i X669C, o único testado. Nada foi alterado." "the model '$MODEL' is not the Infinix Hot 30i X669C, the only one tested. Nothing was changed.")"
elif fastboot devices 2>/dev/null | grep -q fastboot; then
  MODE="fastboot"
  say "$(T "Aparelho em modo fastboot (o modelo não pode ser lido por aqui)." "Phone in fastboot mode (the model cannot be read from here).")"
elif [ -n "$BP_PORT" ]; then
  MODE="bankphone"
  say "$(T "Aparelho rodando o BANKPHONE OS (porta $BP_PORT). O modelo não pode ser lido por aqui." "Phone running BANKPHONE OS (port $BP_PORT). The model cannot be read from here.")"
else
  die "$(T "nenhum aparelho encontrado.
  - Cabo USB ligado e aparelho ligado.
  - Para o modo Android: ative Opções do desenvolvedor > Depuração USB e aceite a autorização no celular.
  - Se o aparelho está desligado ou travado, ligue-o e rode de novo." "no phone found.
  - USB cable plugged in and the phone turned on.
  - For Android mode: enable Developer options > USB debugging and accept the authorisation on the phone.
  - If the phone is off or stuck, turn it on and run this again.")"
fi
if [ "$MODE" != "android" ]; then
  printf "%s " "$(T "Digite o modelo do seu aparelho para continuar (só o X669C foi testado):" "Type the model of your phone to continue (only the X669C was tested):")"
  read -r ans
  [ "$(echo "$ans" | tr a-z A-Z)" = "X669C" ] || die "$(T "modelo não confirmado. Nada foi alterado." "model not confirmed. Nothing was changed.")"
fi

wait_fastboot() {
  for _ in $(seq 1 "${1:-45}"); do fastboot devices 2>/dev/null | grep -q fastboot && return 0; sleep 2; done
  return 1
}

need_fastboot() {
  say "$(T "Reiniciando para o bootloader..." "Rebooting to the bootloader...")"
  adb reboot bootloader >/dev/null 2>&1
  wait_fastboot 45 || die "$(T "o aparelho não apareceu no fastboot em 90 s." "the phone did not show up in fastboot within 90 s.")"
}

# Manda o comando pela serial USB: o BANKPHONE reinicia sozinho no bootloader.
serial_to_fastboot() {
  say "$(T "Mandando o aparelho para o fastboot pela USB (sem apertar nada)..." "Sending the phone to fastboot over USB (no button pressed)...")"
  python3 - "$BP_PORT" <<'PY' || die "$(T "não consegui abrir a porta serial $BP_PORT." "could not open the serial port $BP_PORT.")"
import os, sys, termios, time
fd = os.open(sys.argv[1], os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
a = termios.tcgetattr(fd)
a[0] = a[1] = a[3] = 0
a[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
a[4] = a[5] = termios.B115200
termios.tcsetattr(fd, termios.TCSANOW, a)
for _ in range(3):                      # 3 vezes: se a primeira se perder, a seguinte chega
    try:
        os.write(fd, b"\nBANKPHONE:REBOOT-BOOTLOADER\n")
    except OSError:
        break                           # a porta sumiu: o aparelho já reiniciou no bootloader
    time.sleep(0.7)
try:
    os.close(fd)
except OSError:
    pass
PY
  if ! wait_fastboot "${BANKPHONE_WAIT:-25}"; then
    die "$(T "o aparelho não entrou no fastboot em 50 s.
  A imagem que está no aparelho provavelmente é antiga e ainda não aceita o comando.
  Entre no fastboot uma vez (segure Vol+ por 3 s no BANKPHONE) e rode ./install.sh de novo:
  a imagem que este instalador grava já aceita o comando, e dali em diante é tudo automático." "the phone did not enter fastboot within 50 s.
  The image on the phone is probably older and does not accept the command yet.
  Enter fastboot once (hold Vol+ for 3 s on BANKPHONE) and run ./install.sh again:
  the image this installer flashes already accepts the command, and from then on it is automatic.")"
  fi
}

# ------------------------------------------------------------ 2. firmware do toque e ir ao fastboot
if [ "$MODE" = "android" ]; then
  if [ "$RESTORE" -eq 0 ] && [ "$SKIPFW" -eq 0 ]; then
    say "$(T "Pegando o firmware do toque do seu aparelho..." "Getting the touch firmware from your phone...")"
    tools/get-touch-firmware.sh || warn "$(T "sem o firmware do toque a tela NÃO responderá ao toque. O resto segue." "without the touch firmware the screen will NOT respond to touch. The rest goes on.")"
  fi
  need_fastboot
elif [ "$MODE" = "bankphone" ]; then
  serial_to_fastboot
fi

# ------------------------------------------------------------ 3. checagens no fastboot
CUR="$(getv current-slot)"; CNT="$(getv slot-count)"
[ "$CNT" = "2" ] || die "$(T "o aparelho não tem 2 slots A/B (slot-count='$CNT'). Este instalador só serve para aparelhos A/B." "the phone does not have 2 A/B slots (slot-count='$CNT'). This installer only works with A/B phones.")"
{ [ "$CUR" = "a" ] || [ "$CUR" = "b" ]; } || die "$(T "não consegui ler o slot ativo (current-slot='$CUR')." "could not read the active slot (current-slot='$CUR').")"
UNL="$(getv unlocked)"
case "$UNL" in
  no|NO|false) die "$(T "o bootloader está TRAVADO. Destrave-o antes (isso apaga os dados do aparelho)." "the bootloader is LOCKED. Unlock it first (this erases the data on the phone).")" ;;
  "") warn "$(T "não consegui confirmar se o bootloader está destravado. Se a gravação falhar, é por isso." "could not confirm that the bootloader is unlocked. If flashing fails, that is why.")" ;;
esac

# slots: se já instalamos antes, o alvo é o mesmo; senão é o slot inativo.
S_ORIG="$(state_get orig)"; S_TARGET="$(state_get target)"
if [ -n "$S_ORIG" ] && [ -n "$S_TARGET" ] && { [ "$CUR" = "$S_ORIG" ] || [ "$CUR" = "$S_TARGET" ]; }; then
  ORIG="$S_ORIG"; TARGET="$S_TARGET"
else
  ORIG="$CUR"; TARGET="$(other "$CUR")"
fi
[ "$ORIG" != "$TARGET" ] || die "$(T "erro interno: slot original igual ao alvo." "internal error: original slot equals the target.")"

# ------------------------------------------------------------ restore
if [ "$RESTORE" -eq 1 ]; then
  say "$(T "Slot ativo agora: $CUR. Slot original registrado: $ORIG." "Active slot now: $CUR. Recorded original slot: $ORIG.")"
  printf "%s RESTORE %s\n> " "$(T "Para voltar ao slot original, digite exatamente:" "To go back to the original slot, type exactly:")" "$ORIG"
  read -r ans; [ "$ans" = "RESTORE $ORIG" ] || die "$(T "confirmação não confere. Nada foi alterado." "confirmation does not match. Nothing was changed.")"
  if [ "$DRY" -eq 1 ]; then say "$(T "DRY-RUN: rodaria:" "DRY-RUN: would run:") fastboot set_active $ORIG && fastboot reboot"; exit 0; fi
  fastboot set_active "$ORIG" && fastboot reboot && say "$(T "Voltando para o slot $ORIG." "Going back to slot $ORIG.")" && exit 0
  die "$(T "não consegui trocar de slot." "could not switch slots.")"
fi

# ------------------------------------------------------------ 4. boot do SEU aparelho
STOCK=""
if [ -n "$STOCK_ARG" ]; then STOCK="$STOCK_ARG"
elif [ -n "${STOCK_BOOT:-}" ]; then STOCK="$STOCK_BOOT"
fi
if [ -z "$STOCK" ]; then
  STOCK="work/stock-boot.img"
  say "$(T "Buscando o boot do slot original (boot_$ORIG) no aparelho (fastboot fetch)..." "Fetching the boot image of the original slot (boot_$ORIG) from the phone (fastboot fetch)...")"
  if ! fastboot fetch "boot_$ORIG" "$STOCK" >/dev/null 2>&1 || [ ! -s "$STOCK" ]; then
    rm -f "$STOCK"
    die "$(T "este aparelho não aceitou 'fastboot fetch'.
  Plano B: pegue o boot.img do firmware oficial do seu modelo e rode:
    ./install.sh --stock-boot /caminho/boot.img" "this phone did not accept 'fastboot fetch'.
  Plan B: take the boot.img from the official firmware of your model and run:
    ./install.sh --stock-boot /path/to/boot.img")"
  fi
fi
[ -f "$STOCK" ] || die "$(T "arquivo de boot não encontrado: $STOCK" "boot file not found: $STOCK")"
python3 - "$STOCK" <<'PY' || die "$(T "o arquivo '$STOCK' não parece um boot.img válido, ou já é uma imagem do BANKPHONE. Use o boot ORIGINAL do aparelho." "the file '$STOCK' does not look like a valid boot.img, or it is already a BANKPHONE image. Use the ORIGINAL boot of the phone.")"
import sys
d = open(sys.argv[1], "rb").read(4096)
sys.exit(0 if d[:8] == b"ANDROID!" and b"bankphone" not in d else 1)
PY
SHA="$(shasum -a 256 "$STOCK" | cut -d' ' -f1)"
say "$(T "boot de origem:" "source boot:") $STOCK (sha256 ${SHA:0:16}...)"

# ------------------------------------------------------------ 5. compilar
if [ -d work/stock ] && [ "$(cat work/.stock-sha256 2>/dev/null)" != "$SHA" ]; then
  mv work/stock "work/stock.antigo.$(date +%s)"
fi
echo "$SHA" > work/.stock-sha256
export STOCK_BOOT="$STOCK" ZIG="$ZIGBIN"
say "$(T "Compilando..." "Building...")"
./build.sh >work/install-build.log 2>&1 || { tail -15 work/install-build.log; die "$(T "o build falhou (log completo em work/install-build.log)." "the build failed (full log in work/install-build.log).")"; }
bash scripts/pack-test-image.sh >work/install-pack.log 2>&1 || { tail -15 work/install-pack.log; die "$(T "o empacotamento falhou (work/install-pack.log)." "packing failed (work/install-pack.log).")"; }
IMG="work/bankphone-os-ro.img"
[ -s "$IMG" ] || die "$(T "a imagem $IMG não foi gerada." "the image $IMG was not generated.")"
IMGSHA="$(shasum -a 256 "$IMG" | cut -d' ' -f1)"

# ------------------------------------------------------------ 6. confirmar e gravar
echo
echo "================ $(T RESUMO SUMMARY) ================"
echo "  $(T "Modelo ........" "Model .........") ${MODEL:-$(T "confirmado por você: X669C" "confirmed by you: X669C")}"
if [ "$CUR" = "$TARGET" ]; then NOTE="$(T "(é o slot de TESTE de uma instalação anterior: será regravado)" "(it is the TEST slot of a previous install: it will be rewritten)")"; else NOTE="$(T "(NÃO será alterado)" "(will NOT be changed)")"; fi
echo "  $(T "Slot ativo ...." "Active slot .") $CUR  $NOTE"
echo "  $(T "Slot original ." "Original slot") $ORIG $(T "(seu caminho de volta)" "(your way back)")"
echo "  $(T "Vai gravar em ." "Will write to ") boot_$TARGET"
echo "  $(T "Imagem ........" "Image ........") $IMG"
echo "  sha256 ........ ${IMGSHA:0:32}..."
echo "  $(T "Modo da imagem " "Image mode .. ") $(T "somente leitura (bankphone.ro=1): não grava estado" "read-only (bankphone.ro=1): writes no state")"
echo "========================================"
if [ "$DRY" -eq 1 ]; then
  say "$(T "DRY-RUN: rodaria:" "DRY-RUN: would run:") fastboot flash boot_$TARGET $IMG && fastboot set_active $TARGET && fastboot reboot"
  say "$(T "Nada foi gravado. O aparelho está no fastboot: para reiniciá-lo, rode" "Nothing was written. The phone is in fastboot: to restart it, run")  fastboot reboot"
  exit 0
fi
PHRASE="FLASH boot_$TARGET"
printf "%s %s\n> " "$(T "Para gravar, digite exatamente:" "To flash, type exactly:")" "$PHRASE"
read -r ans; [ "$ans" = "$PHRASE" ] || die "$(T "confirmação não confere. Nada foi gravado." "confirmation does not match. Nothing was written.")"

[ "$TARGET" != "$ORIG" ] || die "$(T "recusado: o alvo seria o slot original." "refused: the target would be the original slot.")"
fastboot flash "boot_$TARGET" "$IMG" || die "$(T "falha ao gravar boot_$TARGET. O slot $ORIG não foi tocado." "failed to write boot_$TARGET. Slot $ORIG was not touched.")"
printf "orig=%s\ntarget=%s\nimg=%s\n" "$ORIG" "$TARGET" "$IMGSHA" > "$STATE"
fastboot set_active "$TARGET" || die "$(T "gravou, mas não consegui ativar o slot $TARGET." "written, but could not activate slot $TARGET.")"
fastboot reboot
say "$(T "Pronto. O aparelho está reiniciando no slot $TARGET." "Done. The phone is rebooting into slot $TARGET.")"
echo "$(T "Para voltar ao sistema original: coloque o aparelho no fastboot e rode ./install.sh --restore" "To go back to the original system: put the phone in fastboot and run ./install.sh --restore")"
