#!/usr/bin/env bash
# ==============================================================================
# BANKPHONE OS — host-preview.sh
#
# Renderiza as TELAS em PNG na SUA máquina, ANTES de qualquer teste no aparelho.
# Usa exatamente os mesmos arquivos de desenho do sistema: ui.c, screens.c,
# sheets.c, components.c, icons.c + gfx.c (com stb_truetype).
#
# Nada disso vai para o celular: é só para você olhar e aprovar.
#
# Uso:
#   ./scripts/host-preview.sh                 # macOS/Linux, escreve em work/pv
#   ./scripts/host-preview.sh saida/          # outro diretório
#   ./scripts/host-preview.sh saida/ --amostra # valores de amostra (bateria 87% etc.)
#
# Requisitos: um compilador C (clang/cc/gcc). No macOS já existe (clang do Xcode).
# Se você tiver o zig, use:  ZIG="zig cc" ./scripts/host-preview.sh
# ==============================================================================
set -uo pipefail
cd "$(dirname "$0")/.." || exit 1

OUT="${1:-work/pv}"
shift || true
EXTRA="$*"

CC="${CC:-${ZIG:-}}"
if [[ -z "$CC" ]]; then
  for c in cc clang gcc; do command -v "$c" >/dev/null 2>&1 && { CC="$c"; break; }; done
fi
[[ -z "$CC" ]] && { echo "ERRO: nenhum compilador C encontrado (instale o Xcode Command Line Tools)"; exit 1; }

# O repo do aparelho guarda as fontes em init/assets; a cópia de trabalho usa
# src/init. O script aceita as duas arrumações sem você precisar mover nada.
if   [[ -f src/init/host.c ]]; then SRC="src/init"
elif [[ -f init/host.c ]];     then SRC="init"
else echo "ERRO: não achei host.c (procurei em src/init/ e init/)"; exit 1; fi

FONTE="${BANKPHONE_FONT:-}"
for f in "$SRC/assets/Roboto-Regular.ttf" "assets/Roboto-Regular.ttf" \
         "$SRC/assets/Roboto-Medium.ttf"  "assets/Roboto-Medium.ttf"; do
  [[ -n "$FONTE" ]] && break
  [[ -f "$f" ]] && FONTE="$f"
done
if [[ -z "$FONTE" ]]; then
  cat <<AVISO
ERRO: nenhuma fonte encontrada (procurei em $SRC/assets/ e assets/).
  Copie o Roboto-Regular.ttf (que já existe no seu repo) para $SRC/assets/.
  O sistema NÃO desenha texto sem essa fonte — nem aqui, nem no aparelho.
AVISO
  exit 1
fi
echo "fonte: $FONTE"
if [[ "$FONTE" != *"Roboto-Regular"* ]]; then
  echo "AVISO: Roboto-Regular.ttf não encontrado — vou usar $FONTE."
  echo "       (o Roboto-Medium é Apache-2.0 e está no repo como alternativa; a letra sai mais pesada)"
fi

BIN="$(mktemp -t bankphone-host-XXXXXX)"
echo "compilando com: $CC"
# shellcheck disable=SC2086
$CC -O1 -o "$BIN" \
  "$SRC/host.c" "$SRC/ui.c" "$SRC/screens.c" "$SRC/sheets.c" "$SRC/components.c" \
  "$SRC/icons.c" "$SRC/hw.c" "$SRC/gfx.c" "$SRC/money.c" "$SRC/sec.c" "$SRC/store.c" \
  -lm -lz || { echo "ERRO: compilação falhou"; exit 1; }

mkdir -p "$OUT"
# roda da RAIZ do repo: o host procura a fonte em assets/ e src/init/assets/
export BANKPHONE_FONT="$FONTE"
"$BIN" "$OUT" $EXTRA
echo
echo "PNGs em: $OUT"
echo "  pv-*.png  → as telas (entrega)"
echo "  dev-*.png → conferência: rolagem, folhas, aviso"
echo "  LEIA-ME.txt → diz qual fonte foi usada e que são capturas de HOST (não do aparelho)"
