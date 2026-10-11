#!/bin/sh
# Host test of scripts/publish-issues.sh with a FAKE `gh` on PATH. Nothing touches GitHub.
# Teste de host do scripts/publish-issues.sh com um `gh` FALSO no PATH. Nada toca o GitHub.
# This proves the behaviour against the fake only, NOT against the real GitHub API.
# Isto prova o comportamento contra o falso, NÃO contra a API real do GitHub.
# Each PASS/FAIL line is one check / Cada linha PASS/FAIL é uma verificação.
cd "$(dirname "$0")/../.." || exit 1
SCRIPT="$PWD/scripts/publish-issues.sh"
T="$(mktemp -d)"; trap 'rm -rf "$T"' EXIT
bad=0
ok()   { echo "PASS $1"; }
fail() { echo "FAIL $1"; bad=$((bad + 1)); }
check() { if [ "$1" = 0 ]; then ok "$2"; else fail "$2"; fi; }

mkdir -p "$T/bin" "$T/d" "$T/bodies"
cat > "$T/bin/gh" <<'GH'
#!/bin/sh
# fake gh. list: titles from $FAKE_TITLES, draft markers from $FAKE_MARKERS. create: logged, body kept.
all="$*"
case "$1 $2" in
  "issue list")
    [ "${FAKE_LIST_FAIL:-0}" = 1 ] && exit 1
    case "$all" in
      *"--json title"*) [ -f "${FAKE_TITLES:-}" ] && cat "$FAKE_TITLES" ;;
      *"--json body"*)  [ -f "${FAKE_MARKERS:-}" ] && cat "$FAKE_MARKERS" ;;
    esac ;;
  "label list") [ -f "${FAKE_LABELS:-}" ] && cat "$FAKE_LABELS" ;;
  "issue create")
    n=$(grep -c '^issue create' "$FAKE_LOG" 2>/dev/null); n=$((n + 1))
    [ "${FAKE_CREATE_FAIL_AT:-0}" = "$n" ] && exit 1
    echo "$all" >> "$FAKE_LOG"
    while [ $# -gt 0 ]; do
      case "$1" in --body-file) cp "$2" "$FAKE_BODYDIR/$n.body" ;; --title) printf '%s' "$2" > "$FAKE_BODYDIR/$n.title" ;; esac
      shift
    done ;;
esac
exit 0
GH
chmod +x "$T/bin/gh"

printf '# [P1] Primeira\n\n**Labels sugeridos:** `priority:P1`, `type:bug`\n\nCorpo um\n' > "$T/d/N01-a.md"
printf '# [P2] Segunda\n\nCorpo dois\n' > "$T/d/N02-b.md"
printf '# [P2] Terceira\n\nGerado por Claude Code\n' > "$T/d/N03-c.md"
printf '# Indice\n' > "$T/d/README.md"

# run <envs|-> <stdin> args...   (FAKE_* go through env, not "VAR=x function")
run() { : > "$T/log"; rm -f "$T"/bodies/*; envs="$1"; inp="$2"; shift 2; [ "$envs" = "-" ] && envs=""
  printf '%s' "$inp" | env PATH="$T/bin:$PATH" FAKE_LOG="$T/log" FAKE_BODYDIR="$T/bodies" $envs sh "$SCRIPT" --dir "$T/d" --delay 0 "$@" > "$T/out" 2>&1; RC=$?; }
created() { grep -c '^issue create' "$T/log"; }
YES2="PUBLISH 2 ISSUES
"

# ---- dry run
run - "" --only N01,N02
check $([ $RC = 0 ] && [ "$(created)" = 0 ] && grep -q 'dry-run' "$T/out" && echo 0 || echo 1) "sem --publish: nada e criado (dry-run)"
run - "" --dry-run --publish --only N01
check $([ "$(created)" = 0 ] && echo 0 || echo 1) "--dry-run explicito vence quando vem depois"

# ---- confirmation
run - "" --publish --only N01,N02
check $([ $RC != 0 ] && [ "$(created)" = 0 ] && echo 0 || echo 1) "--publish sem confirmacao: nada e criado"
run - "yes
" --publish --only N01,N02
check $([ $RC != 0 ] && [ "$(created)" = 0 ] && echo 0 || echo 1) "confirmacao errada: nada e criado"
run - "PUBLISH 5 ISSUES
" --publish --only N01,N02
check $([ $RC != 0 ] && [ "$(created)" = 0 ] && echo 0 || echo 1) "confirmacao com numero errado: nada e criado"
run - "$YES2" --publish --only N01,N02
check $([ $RC = 0 ] && [ "$(created)" = 2 ] && echo 0 || echo 1) "confirmacao certa cria as duas issues"
check $(grep -q -- '--title \[P1\] Primeira' "$T/log" && grep -q -- '--body-file' "$T/log" && echo 0 || echo 1) "titulo e corpo vao para o gh"
check $(grep -q -- '--label' "$T/log" && echo 1 || echo 0) "sem --with-labels nenhum rotulo e enviado"
check $(grep -q 'draft-id: N01' "$T/bodies/1.body" && grep -q 'draft-id: N02' "$T/bodies/2.body" && echo 0 || echo 1) "o corpo criado leva o marcador do rascunho"

# ---- special characters arrive unchanged / caracteres especiais chegam sem alteração
printf '# [P1] Título com "aspas", $VAR e `crase` & mais\n\nCorpo com $(echo x), `cmd`, *negrito*, | tabela |, \\ barra, 100%% e <tag>\n' > "$T/d/N04-s.md"
run - "PUBLISH 1 ISSUES
" --publish --only N04
check $([ "$(cat "$T/bodies/1.title")" = '[P1] Título com "aspas", $VAR e `crase` & mais' ] && echo 0 || echo 1) "titulo com caracteres especiais chega identico"
check $(grep -qF 'Corpo com $(echo x), `cmd`, *negrito*, | tabela |, \ barra, 100% e <tag>' "$T/bodies/1.body" && echo 0 || echo 1) "corpo com Markdown e especiais chega identico (nada e expandido)"
rm "$T/d/N04-s.md"

# ---- labels
run - "PUBLISH 1 ISSUES
" --publish --only N01
check $(grep -q -- '--label' "$T/log" && echo 1 || echo 0) "um so rascunho, sem rotulos por padrao"
printf 'priority:P1\n' > "$T/labels"
run "FAKE_LABELS=$T/labels" "PUBLISH 1 ISSUES
" --publish --with-labels --only N01
check $(grep -q -- '--label priority:P1$' "$T/log" && grep -q "rótulo 'type:bug' não existe" "$T/out" && echo 0 || echo 1) "--with-labels envia so os rotulos que existem e avisa dos outros"

# ---- duplicates: marker, exact title, similar title / duplicatas
printf 'draft-id: N01\n' > "$T/markers"
run "FAKE_MARKERS=$T/markers" "PUBLISH 1 ISSUES
" --publish --only N01,N02
check $([ "$(created)" = 1 ] && grep -q 'N01: já existe (marcador' "$T/out" && echo 0 || echo 1) "marcador do rascunho existente: pula, mesmo com titulo diferente"
echo "[P1] Primeira" > "$T/titles"
run "FAKE_TITLES=$T/titles" "PUBLISH 1 ISSUES
" --publish --only N01,N02
check $([ "$(created)" = 1 ] && grep -q 'N01: já existe (título igual' "$T/out" && echo 0 || echo 1) "titulo igual existente: pula"
echo "primeira!!" > "$T/titles"
run "FAKE_TITLES=$T/titles" "PUBLISH 1 ISSUES
" --publish --only N01,N02
check $([ "$(created)" = 1 ] && grep -q 'N01: já existe (título parecido' "$T/out" && echo 0 || echo 1) "titulo parecido (caixa, pontuacao, sem [P1]): pula"
run "FAKE_TITLES=$T/titles FAKE_MARKERS=$T/markers" "" --publish
check $([ "$(created)" = 0 ] && echo 0 || echo 1) "rodar de novo com tudo existente nao pede nem cria nada"

# ---- refusals / recusas
run - "$YES2" --publish
check $([ $RC != 0 ] && grep -q 'N03: RECUSADA — cita ferramenta' "$T/out" && echo 0 || echo 1) "rascunho que cita ferramenta de IA e recusado e o script sai com erro"
printf '# T\n\ntoken ghp_abcdefghijklmnopqrstuvwxyz0123456789\n' > "$T/d/N05-x.md"
run - "" --only N05
check $(grep -q 'N05: RECUSADA — parece conter segredo' "$T/out" && echo 0 || echo 1) "rascunho com cara de token e recusado"
printf '# \n\ncorpo\n' > "$T/d/N05-x.md"
run - "" --only N05
check $(grep -q 'N05: RECUSADA — título vazio' "$T/out" && echo 0 || echo 1) "titulo vazio e recusado"
printf '# Titulo sem corpo\n\n   \n' > "$T/d/N05-x.md"
run - "" --only N05
check $(grep -q 'N05: RECUSADA — corpo vazio' "$T/out" && echo 0 || echo 1) "corpo vazio e recusado"
printf '# %s\n\ncorpo\n' "$(printf 'a%.0s' $(seq 1 300))" > "$T/d/N05-x.md"
run - "" --only N05
check $(grep -q 'N05: RECUSADA — título com mais de 256' "$T/out" && echo 0 || echo 1) "titulo com mais de 256 caracteres e recusado"
rm "$T/d/N05-x.md"

# ---- selection / seleção
run - "PUBLISH 1 ISSUES
" --publish --only N02
check $(! grep -q 'N01\|N03' "$T/out" && echo 0 || echo 1) "--only ignora as outras"
check $(grep -q 'README' "$T/out" && echo 1 || echo 0) "README nao e tratado como issue"

# ---- errors / erros
run FAKE_LIST_FAIL=1 "" --publish
check $([ $RC = 2 ] && [ "$(created)" = 0 ] && echo 0 || echo 1) "gh sem login ou sem API: aborta sem criar nada"
run FAKE_CREATE_FAIL_AT=1 "$YES2" --publish --only N01,N02
check $([ $RC != 0 ] && grep -q 'FALHOU' "$T/out" && [ "$(created)" = 0 ] && echo 0 || echo 1) "falha ao criar: reportada, exit != 0"
run FAKE_CREATE_FAIL_AT=2 "$YES2" --publish --only N01,N02
check $([ "$(created)" = 1 ] && grep -q 'parando na primeira falha' "$T/out" && grep -q 'criadas / created: N01' "$T/out" && echo 0 || echo 1) "falha na segunda: para, e diz que a primeira foi criada"
run - "" --opcao-estranha
check $([ $RC = 2 ] && echo 0 || echo 1) "opcao desconhecida: erro de uso"
run - "" --dir "$T/nao-existe"
check $([ $RC = 2 ] && echo 0 || echo 1) "pasta inexistente: erro de uso"
run - "" --delay abc
check $([ $RC = 2 ] && echo 0 || echo 1) "--delay invalido: erro de uso"

# ---- no environment or token in the output / nada de ambiente ou token na saída
run "GH_TOKEN=ghp_SEGREDOSEGREDOSEGREDO123456 GITHUB_TOKEN=ghp_SEGREDOSEGREDOSEGREDO123456" "$YES2" --publish --only N01,N02
check $(grep -q 'SEGREDO' "$T/out" "$T/log" && echo 1 || echo 0) "tokens do ambiente nao aparecem na saida nem no que vai ao gh"

# ---- the real drafts, when present / os rascunhos reais, quando existirem
if ls docs/issues/N[0-9][0-9]-*.md >/dev/null 2>&1; then
  check $(grep -qiE 'claude|anthropic|generated by' docs/issues/N[0-9][0-9]-*.md && echo 1 || echo 0) "rascunhos reais nao citam ferramenta de IA"
fi

[ "$bad" = 0 ] && echo "TODOS OK" || echo "FALHAS: $bad"
exit "$bad"
