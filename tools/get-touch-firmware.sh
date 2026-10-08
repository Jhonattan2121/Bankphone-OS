#!/bin/bash
# BANKPHONE OS: pega o firmware do toque (Novatek) do SEU aparelho.
#
# O sistema precisa desses dois arquivos para o toque funcionar. Eles são da
# fabricante do chip e NÃO fazem parte deste repositório: cada pessoa tira o dela.
#
# Uso:
#   tools/get-touch-firmware.sh              # via adb, com o Android original ligado
#   tools/get-touch-firmware.sh --from DIR   # copia de uma pasta que já tem os arquivos
#   tools/get-touch-firmware.sh --force      # sobrescreve arquivos que já existem
#
# Requisitos do modo adb: celular com o Android original ligado, "Depuração USB"
# ativa e este computador autorizado na tela do celular.
set -u
cd "$(dirname "$0")/.."
[ -f local.env ] && . ./local.env   # opcional: PLATFORM_TOOLS=/caminho/do/platform-tools
[ -n "${PLATFORM_TOOLS:-}" ] && export PATH="$PLATFORM_TOOLS:$PATH"

DEST="${FW_DEST:-rootfs-extra/vendor/firmware}"
FILES="novatek_ts_fw.bin novatek_ts_mp.bin"
EXPECT_SIZE=139264        # tamanho dos arquivos no Infinix Hot 30i X669C
FROM=""
FORCE=0

while [ $# -gt 0 ]; do
  case "$1" in
    --from)  FROM="${2:-}"; [ -n "$FROM" ] || { echo "ERRO: --from precisa de uma pasta"; exit 2; }; shift 2 ;;
    --force) FORCE=1; shift ;;
    -h|--help) sed -n 2,14p "$0"; exit 0 ;;
    *) echo "ERRO: opção desconhecida: $1 (use --help)"; exit 2 ;;
  esac
done

mkdir -p "$DEST"

if [ -z "$FROM" ]; then
  command -v adb >/dev/null || { echo "ERRO: adb não encontrado. Instale o Android platform-tools ou use --from DIR."; exit 1; }
  state="$(adb get-state 2>/dev/null || true)"
  if [ "$state" != "device" ]; then
    echo "ERRO: nenhum celular autorizado no adb (estado: ${state:-nenhum})."
    echo "  - ligue o Android original do aparelho"
    echo "  - ative Opções do desenvolvedor > Depuração USB"
    echo "  - aceite a autorização na tela do celular"
    echo "  - ou use --from DIR se você já tem os arquivos"
    exit 1
  fi
fi

fail=0
for f in $FILES; do
  if [ -f "$DEST/$f" ] && [ "$FORCE" -eq 0 ]; then
    echo "já existe: $DEST/$f (use --force para sobrescrever)"
  elif [ -n "$FROM" ]; then
    if [ -f "$FROM/$f" ]; then cp "$FROM/$f" "$DEST/$f" && echo "copiado: $f"
    else echo "ERRO: $FROM/$f não existe"; fail=1; continue; fi
  else
    if adb pull "/vendor/firmware/$f" "$DEST/$f" >/dev/null 2>&1; then echo "baixado do aparelho: $f"
    else
      echo "ERRO: não consegui puxar /vendor/firmware/$f do aparelho."
      echo "  O caminho pode ser outro na sua versão do Android; use --from DIR com os arquivos."
      fail=1; continue
    fi
  fi
  size=$(wc -c < "$DEST/$f" | tr -d ' ')
  if [ "$size" -eq 0 ]; then echo "ERRO: $f está vazio"; rm -f "$DEST/$f"; fail=1
  elif [ "$size" -ne "$EXPECT_SIZE" ]; then
    echo "AVISO: $f tem $size bytes (o esperado no X669C é $EXPECT_SIZE). Pode ser outra revisão do chip: teste antes de confiar."
  fi
done

if [ "$fail" -ne 0 ]; then echo "Faltam arquivos. O build segue, mas o toque não vai funcionar."; exit 1; fi
echo "Pronto. Rode ./build.sh para incluir o firmware no ramdisk."
