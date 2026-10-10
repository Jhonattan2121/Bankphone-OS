#!/bin/sh
# Teste de host de tools/test_boot_b.sh com um `fastboot` FALSO no PATH. Nada toca um aparelho.
# Host test of tools/test_boot_b.sh with a FAKE `fastboot` on PATH. Nothing touches a phone.
#
# Cada linha PASS/FAIL é uma verificação (o Makefile conta as linhas PASS).
# Each PASS/FAIL line is one check (the Makefile counts the PASS lines).
#
# O que se prova / what is proved: o script nunca grava no slot ativo, nunca grava sem confirmação, nunca
# grava no --dry-run, recusa quando não consegue ler o slot, e não espera para sempre.
cd "$(dirname "$0")/../.." || exit 1
SCRIPT="$PWD/tools/test_boot_b.sh"
T="$(mktemp -d)"; trap 'rm -rf "$T"' EXIT
bad=0
ok()   { echo "PASS $1"; }
fail() { echo "FAIL $1"; bad=$((bad + 1)); }
check() { if [ "$1" = 0 ]; then ok "$2"; else fail "$2"; fi; }

# ---- fastboot falso: lê o comportamento de variáveis de ambiente e registra o que foi chamado
mkdir -p "$T/bin"
cat > "$T/bin/fastboot" <<'FB'
#!/bin/sh
case "$1" in
  devices) [ "${FAKE_NO_DEVICE:-0}" = 1 ] || echo "FAKE0001	fastboot" ;;
  getvar)
    case "$2" in
      current-slot) echo "current-slot: ${FAKE_CURRENT-a}" >&2 ;;
      slot-count)   echo "slot-count: ${FAKE_SLOTCOUNT-2}" >&2 ;;
      unlocked)     echo "unlocked: ${FAKE_UNLOCKED-yes}" >&2 ;;
    esac ;;
  flash|set_active|reboot) echo "$*" >> "$FAKE_LOG" ;;
esac
exit 0
FB
chmod +x "$T/bin/fastboot"

# ---- imagens de teste
printf 'ANDROID!%s' "$(printf 'x%.0s' $(seq 1 200))" > "$T/ok.img"      # cabeçalho certo
: > "$T/empty.img"
printf 'NOTANIMAGE%s' "$(printf 'x%.0s' $(seq 1 200))" > "$T/bad.img"   # sem ANDROID!

# run <envs|-> <entrada-stdin> [args do script]
# As variaveis FAKE_* vao por `env`, NAO como "VAR=x funcao": em dash isso vaza para os testes seguintes.
# The FAKE_* variables go through `env`, NOT as "VAR=x function": in dash that leaks into later tests.
run() {
  : > "$T/log"; envs="$1"; inp="$2"; shift 2
  [ "$envs" = "-" ] && envs=""
  printf '%s' "$inp" | env PATH="$T/bin:$PATH" FAKE_LOG="$T/log" $envs sh "$SCRIPT" "$@" > "$T/out" 2>&1
  RC=$?
}
flashed() { grep -c '^flash ' "$T/log"; }

# 1. slot a ativo: grava SÓ boot_b, com a frase certa
run "FAKE_CURRENT=a" "FLASH boot_b
" --image "$T/ok.img" --wait 2
check $([ $RC = 0 ] && grep -q '^flash boot_b ' "$T/log" && [ "$(flashed)" = 1 ] && echo 0 || echo 1) "slot a ativo: grava boot_b e so ele"
check $(grep -q '^set_active b' "$T/log" && grep -q '^reboot' "$T/log" && echo 0 || echo 1) "slot a ativo: ativa b e reinicia"

# 2. slot b ativo: o script antigo gravava boot_b mesmo assim. Agora tem de gravar boot_a.
run "FAKE_CURRENT=b" "FLASH boot_a
" --image "$T/ok.img" --wait 2
check $([ $RC = 0 ] && grep -q '^flash boot_a ' "$T/log" && ! grep -q 'boot_b' "$T/log" && echo 0 || echo 1) "slot b ativo: grava boot_a e NUNCA boot_b"

# 3. slot b ativo e a frase do script antigo (FLASH boot_b): tem de ser recusada
run "FAKE_CURRENT=b" "FLASH boot_b
" --image "$T/ok.img" --wait 2
check $([ $RC != 0 ] && [ "$(flashed)" = 0 ] && echo 0 || echo 1) "slot b ativo + frase de boot_b: recusa, nada gravado"

# 4. slot ativo ilegivel / invalido: recusa
run "FAKE_CURRENT=" "FLASH boot_b
" --image "$T/ok.img" --wait 2
check $([ $RC != 0 ] && [ "$(flashed)" = 0 ] && grep -q 'não consegui ler o slot ativo' "$T/out" && echo 0 || echo 1) "slot ativo vazio: recusa com motivo"
run "FAKE_CURRENT=c" "FLASH boot_b
" --image "$T/ok.img" --wait 2
check $([ $RC != 0 ] && [ "$(flashed)" = 0 ] && echo 0 || echo 1) "slot ativo invalido (c): recusa"
run "FAKE_CURRENT=_a" "FLASH boot_b
" --image "$T/ok.img" --wait 2
check $([ $RC != 0 ] && [ "$(flashed)" = 0 ] && echo 0 || echo 1) "slot ativo '_a': recusa"

# 5. aparelho sem 2 slots / bootloader travado
run "FAKE_SLOTCOUNT=1" "FLASH boot_b
" --image "$T/ok.img" --wait 2
check $([ $RC != 0 ] && [ "$(flashed)" = 0 ] && echo 0 || echo 1) "sem 2 slots: recusa"
run "FAKE_UNLOCKED=no" "FLASH boot_b
" --image "$T/ok.img" --wait 2
check $([ $RC != 0 ] && [ "$(flashed)" = 0 ] && echo 0 || echo 1) "bootloader travado: recusa"

# 6. imagem ausente / vazia / inválida
run - "FLASH boot_b
" --image "$T/nao-existe.img" --wait 2
check $([ $RC != 0 ] && [ "$(flashed)" = 0 ] && echo 0 || echo 1) "imagem ausente: recusa"
run - "FLASH boot_b
" --image "$T/empty.img" --wait 2
check $([ $RC != 0 ] && [ "$(flashed)" = 0 ] && echo 0 || echo 1) "imagem vazia: recusa"
run - "FLASH boot_b
" --image "$T/bad.img" --wait 2
check $([ $RC != 0 ] && [ "$(flashed)" = 0 ] && echo 0 || echo 1) "imagem sem cabecalho ANDROID!: recusa"

# 7. confirmação
run - "flash boot_b
" --image "$T/ok.img" --wait 2
check $([ $RC != 0 ] && [ "$(flashed)" = 0 ] && echo 0 || echo 1) "frase errada: nada gravado"
run - "" --image "$T/ok.img" --wait 2
check $([ $RC != 0 ] && [ "$(flashed)" = 0 ] && echo 0 || echo 1) "sem confirmacao (stdin vazio): nada gravado"

# 8. --dry-run nunca grava, mesmo com a frase certa na entrada
run "FAKE_CURRENT=a" "FLASH boot_b
" --dry-run --image "$T/ok.img" --wait 2
check $([ $RC = 0 ] && [ ! -s "$T/log" ] && grep -q DRY-RUN "$T/out" && echo 0 || echo 1) "--dry-run: nenhum comando de escrita, mesmo com a frase certa"

# 9. hash da imagem na saida
EXP="$( (sha256sum "$T/ok.img" 2>/dev/null || shasum -a 256 "$T/ok.img") | cut -d' ' -f1)"
run - "" --dry-run --image "$T/ok.img" --wait 2
check $(grep -q "$EXP" "$T/out" && echo 0 || echo 1) "mostra o sha256 da imagem"

# 10. sem aparelho: falha dentro do limite de tempo (nao espera para sempre)
t0=$(date +%s)
run "FAKE_NO_DEVICE=1" "FLASH boot_b
" --image "$T/ok.img" --wait 2
t1=$(date +%s)
check $([ $RC != 0 ] && [ "$(flashed)" = 0 ] && [ $((t1 - t0)) -le 8 ] && echo 0 || echo 1) "sem aparelho: falha em tempo limitado (--wait 2)"

# 11. opcoes invalidas
run - "" --wait abc --image "$T/ok.img"
check $([ $RC = 2 ] && echo 0 || echo 1) "--wait invalido: erro de uso"
run - "" --nao-existe
check $([ $RC = 2 ] && echo 0 || echo 1) "opcao desconhecida: erro de uso"

# 12. checagem estatica: nenhum slot fixo no comando de gravar
check $(grep -nE 'fastboot +flash +boot_[ab]([^_$A-Za-z]|$)' "$SCRIPT" >/dev/null && echo 1 || echo 0) "script nao tem 'flash boot_a/boot_b' fixo"

[ "$bad" = 0 ] && echo "TODOS OK" || echo "FALHAS: $bad"
exit "$bad"
