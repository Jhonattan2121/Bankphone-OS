#!/bin/bash
# BANKPHONE OS: instalador.
# Reconhece o aparelho pelo cabo USB e instala o sistema no slot A/B INATIVO.
# Nunca grava no slot ativo (o seu caminho de volta) e sempre pede confirmação digitada.
#
# Uso:
#   ./install.sh                        instala (Android ligado com adb, ou aparelho já no fastboot)
#   ./install.sh --dry-run              faz tudo, MENOS gravar no aparelho
#   ./install.sh --stock-boot FILE      usa o boot.img do seu aparelho em vez de buscá-lo (fastboot fetch)
#   ./install.sh --skip-firmware        não puxa o firmware do toque
#   ./install.sh --restore              volta o slot ativo para o original (não grava nenhuma imagem)
#
# Sem garantia. Só testado no Infinix Hot 30i (X669C). Leia o README antes.
set -u
cd "$(dirname "$0")"

DRY=0; RESTORE=0; STOCK_ARG=""; SKIPFW=0
while [ $# -gt 0 ]; do
  case "$1" in
    --dry-run) DRY=1; shift ;;
    --restore) RESTORE=1; shift ;;
    --skip-firmware) SKIPFW=1; shift ;;
    --stock-boot) STOCK_ARG="${2:-}"; [ -n "$STOCK_ARG" ] || { echo "ERRO: --stock-boot precisa de um arquivo"; exit 2; }; shift 2 ;;
    -h|--help) sed -n 2,14p "$0"; exit 0 ;;
    *) echo "ERRO: opção desconhecida: $1 (use --help)"; exit 2 ;;
  esac
done

die()  { echo "ERRO: $*" >&2; exit 1; }
say()  { echo "==> $*"; }
warn() { echo "AVISO: $*" >&2; }

[ -f local.env ] && . ./local.env
[ -n "${PLATFORM_TOOLS:-}" ] && PATH="$PLATFORM_TOOLS:$PATH"
export PATH

need() { command -v "$1" >/dev/null 2>&1 || die "$1 não encontrado. $2"; }
need python3 "Instale o Python 3."
need adb "Instale o Android platform-tools (ou defina PLATFORM_TOOLS em local.env)."
need fastboot "Instale o Android platform-tools (ou defina PLATFORM_TOOLS em local.env)."
if [ "$RESTORE" -eq 0 ]; then
  ZIGBIN="${ZIG:-$(command -v zig || true)}"
  [ -n "$ZIGBIN" ] && [ -x "$ZIGBIN" ] || die "zig não encontrado. Instale o zig (ziglang.org) ou defina ZIG em local.env."
fi

STATE="work/.install-state"
mkdir -p work
getv() { fastboot getvar "$1" 2>&1 | tr -d '\r' | sed -n "s/^$1: *//p" | head -1; }
other() { [ "$1" = a ] && echo b || echo a; }
state_get() { [ -f "$STATE" ] && sed -n "s/^$1=//p" "$STATE" | head -1; }

# ------------------------------------------------------------ 1. reconhecer o aparelho
MODE=""; MODEL=""
adb_state="$(adb get-state 2>/dev/null | tr -d '\r')"
if [ "$adb_state" = "device" ]; then
  MODE="android"
  MODEL="$(adb shell getprop ro.product.model 2>/dev/null | tr -d '\r')"
  say "Aparelho em modo Android: modelo '${MODEL:-desconhecido}'"
  echo "$MODEL" | tr a-z A-Z | grep -q "X669C" || die "o modelo '$MODEL' não é o Infinix Hot 30i X669C, o único testado. Nada foi alterado."
elif fastboot devices 2>/dev/null | grep -q fastboot; then
  MODE="fastboot"
  say "Aparelho em modo fastboot (o modelo não pode ser lido por aqui)."
  printf "Digite o modelo do seu aparelho para continuar (só o X669C foi testado): "
  read -r ans
  [ "$(echo "$ans" | tr a-z A-Z)" = "X669C" ] || die "modelo não confirmado. Nada foi alterado."
else
  die "nenhum aparelho encontrado.
  - Cabo USB ligado e aparelho desbloqueado na tela.
  - Para o modo Android: ative Opções do desenvolvedor > Depuração USB e aceite a autorização no celular.
  - Ou coloque o aparelho no fastboot e rode de novo."
fi

# ------------------------------------------------------------ 2. firmware do toque (só com Android)
need_fastboot() {
  say "Reiniciando para o bootloader..."
  adb reboot bootloader >/dev/null 2>&1
  for _ in $(seq 1 45); do fastboot devices 2>/dev/null | grep -q fastboot && return 0; sleep 2; done
  die "o aparelho não apareceu no fastboot em 90 s."
}

if [ "$MODE" = "android" ]; then
  if [ "$RESTORE" -eq 0 ] && [ "$SKIPFW" -eq 0 ]; then
    say "Pegando o firmware do toque do seu aparelho..."
    tools/get-touch-firmware.sh || warn "sem o firmware do toque a tela NÃO responderá ao toque. O resto segue."
  fi
  need_fastboot
fi

# ------------------------------------------------------------ 3. checagens no fastboot
CUR="$(getv current-slot)"; CNT="$(getv slot-count)"
[ "$CNT" = "2" ] || die "o aparelho não tem 2 slots A/B (slot-count='$CNT'). Este instalador só serve para aparelhos A/B."
{ [ "$CUR" = "a" ] || [ "$CUR" = "b" ]; } || die "não consegui ler o slot ativo (current-slot='$CUR')."
UNL="$(getv unlocked)"
case "$UNL" in
  no|NO|false) die "o bootloader está TRAVADO. Destrave-o antes (isso apaga os dados do aparelho)." ;;
  "") warn "não consegui confirmar se o bootloader está destravado. Se a gravação falhar, é por isso." ;;
esac

# slots: se já instalamos antes, o alvo é o mesmo; senão é o slot inativo.
S_ORIG="$(state_get orig)"; S_TARGET="$(state_get target)"
if [ -n "$S_ORIG" ] && [ -n "$S_TARGET" ] && { [ "$CUR" = "$S_ORIG" ] || [ "$CUR" = "$S_TARGET" ]; }; then
  ORIG="$S_ORIG"; TARGET="$S_TARGET"
else
  ORIG="$CUR"; TARGET="$(other "$CUR")"
fi
[ "$ORIG" != "$TARGET" ] || die "erro interno: slot original igual ao alvo."

# ------------------------------------------------------------ restore
if [ "$RESTORE" -eq 1 ]; then
  say "Slot ativo agora: $CUR. Slot original registrado: $ORIG."
  printf "Para voltar ao slot original, digite exatamente: RESTORE %s\n> " "$ORIG"
  read -r ans; [ "$ans" = "RESTORE $ORIG" ] || die "confirmação não confere. Nada foi alterado."
  if [ "$DRY" -eq 1 ]; then say "DRY-RUN: rodaria: fastboot set_active $ORIG && fastboot reboot"; exit 0; fi
  fastboot set_active "$ORIG" && fastboot reboot && say "Voltando para o slot $ORIG." && exit 0
  die "não consegui trocar de slot."
fi

# ------------------------------------------------------------ 4. boot do SEU aparelho
STOCK=""
if [ -n "$STOCK_ARG" ]; then STOCK="$STOCK_ARG"
elif [ -n "${STOCK_BOOT:-}" ]; then STOCK="$STOCK_BOOT"
fi
if [ -z "$STOCK" ]; then
  STOCK="work/stock-boot.img"
  say "Buscando o boot do slot original (boot_$ORIG) no aparelho (fastboot fetch)..."
  if ! fastboot fetch "boot_$ORIG" "$STOCK" >/dev/null 2>&1 || [ ! -s "$STOCK" ]; then
    rm -f "$STOCK"
    die "este aparelho não aceitou 'fastboot fetch'.
  Plano B: pegue o boot.img do firmware oficial do seu modelo e rode:
    ./install.sh --stock-boot /caminho/boot.img"
  fi
fi
[ -f "$STOCK" ] || die "arquivo de boot não encontrado: $STOCK"
python3 - "$STOCK" <<'PY' || die "o arquivo '$STOCK' não parece um boot.img válido, ou já é uma imagem do BANKPHONE. Use o boot ORIGINAL do aparelho."
import sys
d = open(sys.argv[1], "rb").read(4096)
sys.exit(0 if d[:8] == b"ANDROID!" and b"bankphone" not in d else 1)
PY
SHA="$(shasum -a 256 "$STOCK" | cut -d' ' -f1)"
say "boot de origem: $STOCK (sha256 ${SHA:0:16}...)"

# ------------------------------------------------------------ 5. compilar
if [ -d work/stock ] && [ "$(cat work/.stock-sha256 2>/dev/null)" != "$SHA" ]; then
  mv work/stock "work/stock.antigo.$(date +%s)"
fi
echo "$SHA" > work/.stock-sha256
export STOCK_BOOT="$STOCK" ZIG="$ZIGBIN"
say "Compilando..."
./build.sh >work/install-build.log 2>&1 || { tail -15 work/install-build.log; die "o build falhou (log completo em work/install-build.log)."; }
bash scripts/pack-test-image.sh >work/install-pack.log 2>&1 || { tail -15 work/install-pack.log; die "o empacotamento falhou (work/install-pack.log)."; }
IMG="work/bankphone-os-ro.img"
[ -s "$IMG" ] || die "a imagem $IMG não foi gerada."
IMGSHA="$(shasum -a 256 "$IMG" | cut -d' ' -f1)"

# ------------------------------------------------------------ 6. confirmar e gravar
cat <<EOF

================ RESUMO ================
  Modelo ........ ${MODEL:-confirmado por você: X669C}
  Slot ativo .... $CUR$( if [ "$CUR" = "$TARGET" ]; then echo "  (é o slot de TESTE de uma instalação anterior: será regravado)"; else echo "  (NÃO será alterado)"; fi )
  Slot original . $ORIG (seu caminho de volta)
  Vai gravar em . boot_$TARGET
  Imagem ........ $IMG
  sha256 ........ ${IMGSHA:0:32}...
  Modo da imagem  somente leitura (bankphone.ro=1): não grava estado
========================================
EOF
if [ "$DRY" -eq 1 ]; then
  say "DRY-RUN: rodaria: fastboot flash boot_$TARGET $IMG && fastboot set_active $TARGET && fastboot reboot"
  say "Nada foi gravado."
  exit 0
fi
PHRASE="FLASH boot_$TARGET"
printf "Para gravar, digite exatamente: %s\n> " "$PHRASE"
read -r ans; [ "$ans" = "$PHRASE" ] || die "confirmação não confere. Nada foi gravado."

[ "$TARGET" != "$ORIG" ] || die "recusado: o alvo seria o slot original."
fastboot flash "boot_$TARGET" "$IMG" || die "falha ao gravar boot_$TARGET. O slot $ORIG não foi tocado."
printf "orig=%s\ntarget=%s\nimg=%s\n" "$ORIG" "$TARGET" "$IMGSHA" > "$STATE"
fastboot set_active "$TARGET" || die "gravou, mas não consegui ativar o slot $TARGET."
fastboot reboot
say "Pronto. O aparelho está reiniciando no slot $TARGET."
echo "Para voltar ao sistema original: coloque o aparelho no fastboot e rode ./install.sh --restore"
