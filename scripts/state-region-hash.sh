#!/usr/bin/env bash
# ==============================================================================
# BANKPHONE OS — state-region-hash.sh
#
# Responde UMA pergunta, fora do aparelho:
#   "qual é o hash da REGIÃO DE ESTADO desta partição, do jeito que o init
#    calcula (src/init/store.c), a partir do dump que eu já tenho?"
#
# Para que serve: o init só TOMA (assume) uma área de estado na PRIMEIRA vez se
# a região bater com o hash lido do backup verificado (`bankphone.statehash=`).
# Sem isso ele RECUSA gravar e diz, na tela de Diagnóstico, os dois hashes.
# Este script é o lado "fora do aparelho" dessa conferência.
#
# O que este script NÃO faz, por decisão de projeto:
#   - não escreve em nada (não toca em partição, não faz backup, não apaga);
#   - não adivinha tamanho: se o dump não tiver o tamanho da partição, RECUSA;
#   - não inventa: partição fora do tamanho mínimo é recusada com o motivo.
#
# Uso:
#   scripts/state-region-hash.sh <dump.img> [--bytes N] [--slots] [--json]
#   scripts/state-region-hash.sh <dump.img> --confirmar <16 hex>
#   scripts/state-region-hash.sh --selftest
#
# Como obter o dump (o backup verificado do bankphonectl):
#   python3 -m bankphone_host.cli backup --partition <part> --dir <backups>
#   ...e depois aponte este script para o arquivo do backup.
#
# Geometria (idêntica a src/init/store.h — se mudar lá, muda aqui):
#   ST_SLOT_BYTES  = 256 KiB   (cada slot)
#   ST_SEP_MIN     = 512 KiB   (separação FIXA entre o início de A e o de B)
#   ST_TAIL_MARGIN =  64 KiB   (margem no fim da partição)
#   span = ST_SEP_MIN + ST_SLOT_BYTES      = 768 KiB
#   base = tam_particao - span - ST_TAIL_MARGIN    (a região é o FIM da partição)
# ==============================================================================
set -uo pipefail

PROG="$(basename "$0")"
DUMP=""
BYTES=""
SLOTS=0
JSON=0
CONFIRMAR=""
SELFTEST=0

uso() { sed -n '2,40p' "$0" | sed 's/^# \{0,1\}//'; }

while [[ $# -gt 0 ]]; do
  case "$1" in
    --bytes)      BYTES="${2:-}"; shift 2 ;;
    --slots)      SLOTS=1; shift ;;
    --json)       JSON=1; shift ;;
    --confirmar)  CONFIRMAR="${2:-}"; shift 2 ;;
    --selftest)   SELFTEST=1; shift ;;
    -h|--help)    uso; exit 0 ;;
    -*)           echo "$PROG: opção desconhecida: $1" >&2; exit 2 ;;
    *)            DUMP="$1"; shift ;;
  esac
done

if [[ $SELFTEST -eq 0 && -z "$DUMP" ]]; then uso; exit 2; fi

PY="$(command -v python3 || true)"
if [[ -z "$PY" ]]; then echo "$PROG: preciso de python3 (sha256 + crc32). Nada foi lido." >&2; exit 3; fi

# ---------------------------------------------------------------- selftest ----
# Confere que ESTE script e o código em C (src/init/store.c) chegam aos MESMOS
# números no MESMO arquivo. Duas implementações independentes concordando é o
# que autoriza confiar no hash do lado de fora do aparelho.
if [[ $SELFTEST -eq 1 ]]; then
  echo "== autoteste: script (python) x store.c (C) =="
  SRC="$(cd "$(dirname "$0")/.." && pwd)/src/init"
  TMP="$(mktemp -d)"
  IMG="$TMP/fake-part.img"
  # 20 MiB, primeiro trecho com lixo e o resto zerado (parecido com uma partição real)
  "$PY" - "$IMG" <<'PYEOF'
import os, sys, random
p = sys.argv[1]
random.seed(7)
data = bytearray(20 << 20)
for i in range(0, 6 << 20, 4096):
    data[i:i+4096] = bytes(random.randrange(256) for _ in range(4096))
open(p, "wb").write(data)
PYEOF
  gcc -O1 -Wall -Wextra -DBANKPHONE_STORE_TEST -I"$SRC" -o "$TMP/geo" \
      "$SRC/tests/store_dump.c" "$SRC/store.c" "$SRC/sec.c" 2>"$TMP/cc.log"
  if [[ $? -ne 0 ]]; then echo "  [falhou] não compilei o medidor em C:"; sed -n '1,12p' "$TMP/cc.log"; exit 1; fi
  CUOUT="$("$TMP/geo" "file:$IMG")"
  C_BASE="$(sed -n 's/^GEO base=\([0-9]*\).*/\1/p' <<<"$CUOUT")"
  C_SPAN="$(sed -n 's/^GEO base=[0-9]* span=\([0-9]*\).*/\1/p' <<<"$CUOUT")"
  C_HASH="$(sed -n 's/^HASH region16=\(.*\)$/\1/p' <<<"$CUOUT")"
  S_OUT="$("$0" "$IMG")"
  S_BASE="$(sed -n 's/^base_kib *= *\([0-9]*\).*/\1/p' <<<"$S_OUT")"
  S_SPAN="$(sed -n 's/^span_kib *= *\([0-9]*\).*/\1/p' <<<"$S_OUT")"
  S_HASH="$(sed -n 's/^hash16 *= *\(.*\)$/\1/p' <<<"$S_OUT")"
  ok=1
  [[ "$C_BASE" == "$((S_BASE * 1024))" ]] || { echo "  [falhou] base: C=$C_BASE script=$S_BASE KiB"; ok=0; }
  [[ "$C_SPAN" == "$((S_SPAN * 1024))" ]] || { echo "  [falhou] span: C=$C_SPAN script=$S_SPAN KiB"; ok=0; }
  [[ "$C_HASH" == "$S_HASH" ]]            || { echo "  [falhou] hash: C=$C_HASH script=$S_HASH"; ok=0; }
  if [[ $ok -eq 1 ]]; then
    echo "  [ok] base/span/hash idênticos nos dois lados: base=$C_BASE span=$C_SPAN hash16=$C_HASH"
    echo "  (é este o número que o aparelho mostra em Diagnóstico → 'Conferência do backup')"
  fi
  rm -rf "$TMP"
  [[ $ok -eq 1 ]] || exit 1
  exit 0
fi

# ------------------------------------------------------------------- dump -----
[[ -f "$DUMP" ]] || { echo "$PROG: '$DUMP' não existe. Nada foi lido." >&2; exit 3; }
TAM_ARQ="$(wc -c <"$DUMP" | tr -d ' ')"
[[ -n "$BYTES" ]] || BYTES="$TAM_ARQ"

# Se você disser que a partição tem N bytes e o dump tiver outro tamanho, o
# hash seria de outro pedaço de mundo: recuso em vez de arriscar.
if [[ "$BYTES" != "$TAM_ARQ" ]]; then
  echo "$PROG: RECUSO — o dump tem $TAM_ARQ bytes, mas você disse que a partição tem $BYTES." >&2
  echo "  O init calcula a região a partir do FIM da partição; com tamanho errado o hash não vale." >&2
  echo "  Confira o dump (dd truncado? hexdump? arquivo esparso?) e repita." >&2
  exit 4
fi
case "$BYTES" in ''|*[!0-9]*) echo "$PROG: --bytes precisa ser um número inteiro (bytes)." >&2; exit 2 ;; esac

"$PY" - "$DUMP" "$BYTES" "$SLOTS" "$JSON" "$CONFIRMAR" "$PROG" <<'PYEOF'
import hashlib, json, sys, zlib

cam, bytes_str, slots_s, json_s, confirmar, prog = sys.argv[1:7]
slots = slots_s == "1"
json_out = json_s == "1"
part = int(bytes_str)
SLOT = 256 << 10
SEP  = 512 << 10
MARG =  64 << 10
span = SEP + SLOT
base = part - span - MARG

def erro(msg, code=4):
    print(f"{prog}: RECUSO — {msg}", file=sys.stderr)
    sys.exit(code)

if part < span + MARG + (1 << 20):
    erro(f"tamanho {part} bytes = {part/1024:.0f} KiB é pequeno demais para uma região de "
         f"{span/1024:.0f} KiB + {MARG/1024:.0f} KiB de margem + 1 MiB de partição útil "
         f"(o init recusa a mesma coisa, com o mesmo número)")

with open(cam, "rb") as f:
    f.seek(base); regiao = f.read(span)
    f.seek(0);    antes = f.read(base)
    f.seek(0);    tudo = f.read()

if len(regiao) != span:
    erro(f"não consegui ler os {span} bytes da região (li {len(regiao)})")

h_reg  = hashlib.sha256(regiao).hexdigest()
h_antes = hashlib.sha256(antes).hexdigest()
h_tudo = hashlib.sha256(tudo).hexdigest()
h16 = h_reg[:16]

virgem = all(b == 0x00 for b in regiao) or all(b == 0xFF for b in regiao)

def cabecalho(off):
    f_bytes = regiao[off:off+24]
    if len(f_bytes) < 24: return None
    magic = int.from_bytes(f_bytes[0:4], "little")
    ver   = int.from_bytes(f_bytes[4:6], "little")
    state = int.from_bytes(f_bytes[6:8], "little")
    seq   = int.from_bytes(f_bytes[8:12], "little")
    ln    = int.from_bytes(f_bytes[12:16], "little")
    crc   = int.from_bytes(f_bytes[16:20], "little")
    hcrc  = int.from_bytes(f_bytes[20:24], "little")
    sealed = zlib.crc32(f_bytes[0:20]) & 0xFFFFFFFF
    nosso = (magic == 0x31305342)
    d = {
        "offset_no_arquivo": base + off,
        "magic_ascii": f_bytes[0:4].decode("ascii", "replace"),
        "assinatura_bs01": nosso,
        "ver": ver, "seq": seq, "len": ln,
        "estado": ("OK" if state == 0xA5A5 else "PENDENTE" if state == 0x5A5A else f"desconhecido({state:#06x})"),
        "crc_payload_conferido": None,
        "crc_cabecalho_ok": (hcrc == sealed) if nosso else None,
    }
    if nosso and 0 < ln <= SLOT - 24 and hcrc == sealed:
        pl = regiao[off+24:off+24+ln]
        d["crc_payload_conferido"] = (len(pl) == ln and (zlib.crc32(pl) & 0xFFFFFFFF) == crc)
    return d

cab = [cabecalho(0), cabecalho(SEP)] if slots else []
ja_nosso = any(c and c["assinatura_bs01"] for c in cab) if slots else None

if json_out:
    print(json.dumps({
        "dump": cam, "bytes": part,
        "base": base, "base_kib": base // 1024, "span": span, "span_kib": span // 1024,
        "sep": SEP, "slot_bytes": SLOT, "tail_margin": MARG,
        "regiao_virgem": virgem,
        "hash16": h16, "sha256_regiao": h_reg,
        "sha256_antes_da_regiao": h_antes, "sha256_particao_inteira": h_tudo,
        "cabecalhos": cab, "area_ja_e_nossa": ja_nosso,
        "cmdline": f"bankphone.state=<partição> bankphone.statehash={h16}",
    }, ensure_ascii=False, indent=2))
    sys.exit(0)

print(f"dump              = {cam}")
print(f"bytes             = {part}  ({part/1048576:.1f} MiB)")
print(f"base_kib          = {base // 1024}   (a região COMEÇA aqui, no fim da partição)")
print(f"span_kib          = {span // 1024}")
print(f"sep_kib           = {SEP // 1024}   (separação fixa entre o slot A e o B)")
print(f"regiao            = [{base}, {base + span})  ->  {base // 1024}..{(base + span) // 1024} KiB")
print(f"regiao_virgem     = {'sim' if virgem else 'NÃO (já tem conteúdo: pode ser lixo de crash dump ou estado antigo)'}")
print(f"hash16            = {h16}")
print(f"sha256_regiao     = {h_reg}")
print(f"sha256_antes      = {h_antes}   (o resto da partição, para o registro do backup)")
print(f"sha256_particao   = {h_tudo}")
if slots:
    for i, c in enumerate(cab):
        nome = "A" if i == 0 else "B"
        if c is None:
            print(f"slot {nome}           = (não consegui ler)")
        elif not c["assinatura_bs01"]:
            print(f"slot {nome}           = vazio/de outro dono (magic '{c['magic_ascii']}')")
        else:
            print(f"slot {nome}           = BS01 ver={c['ver']} estado={c['estado']} seq={c['seq']} len={c['len']} "
                  f"cabecalho={'ok' if c['crc_cabecalho_ok'] else 'CORROMPIDO'} "
                  f"payload={c['crc_payload_conferido']}")
    print(f"area_ja_e_nossa   = {'sim — o aparelho já toma conta desta região (o hash vira dispensável)' if ja_nosso else 'não'}")
print()
print("PARA USAR NO BOOT (o init exige este hash na PRIMEIRA tomada):")
print(f"  bankphone.state=<partição> bankphone.statehash={h16}")
print("  (no Diagnóstico, 'Conferência do backup' mostra este mesmo número lido do aparelho)")
if confirmar:
    if confirmar.strip().lower() == h16:
        print(f"CONFERIDO: o valor informado ({confirmar.strip()}) é o mesmo. Nada foi escrito.")
    else:
        print(f"DIVERGE: informado {confirmar.strip()} x medido {h16}. Nada foi escrito.")
        sys.exit(5)
PYEOF
