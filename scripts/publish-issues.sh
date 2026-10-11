#!/bin/sh
# Publish the issue drafts in docs/issues/ (N01..N20) to GitHub, from YOUR account, with the gh CLI.
# Publica os rascunhos de docs/issues/ (N01..N20) no GitHub, da SUA conta, com o gh.
#
# Safe by default / seguro por padrão:
#   - Without --publish it only PRINTS what it would do and creates or changes nothing (dry run).
#     Sem --publish só MOSTRA o que faria e não cria nem altera nada (dry-run).
#   - --publish needs a typed confirmation phrase on stdin: PUBLISH <n> ISSUES.
#     --publish exige uma frase de confirmação digitada na entrada: PUBLISH <n> ISSUES.
#   - Duplicates are detected three ways: the draft id marker (<!-- draft-id: N05 -->) that this script
#     adds to every issue it creates, the exact title, and the title ignoring case, spaces, punctuation
#     and the [P1] prefix. Running it twice creates nothing new.
#     Duplicatas são detectadas de três jeitos: o marcador do rascunho (<!-- draft-id: N05 -->) que este
#     script põe em toda issue que cria, o título exato, e o título ignorando caixa, espaços, pontuação e
#     o prefixo [P1]. Rodar duas vezes não cria nada novo.
#   - A draft is refused if it mentions an AI tool/assistant, looks like it contains a secret (token),
#     has an empty title/body, or a title over 256 characters.
#     Rascunho é recusado se citar ferramenta/assistente de IA, parecer conter segredo (token), tiver
#     título/corpo vazio ou título com mais de 256 caracteres.
#   - Body and title go to gh as a file and as one argument, never through the shell, so Markdown and
#     special characters arrive unchanged.
#     Corpo e título vão para o gh como arquivo e como um único argumento, nunca pelo shell, então
#     Markdown e caracteres especiais chegam sem alteração.
#   - It stops at the first failed creation (no blind retries, which could duplicate) and waits --delay
#     seconds between creations (GitHub asks for pauses between content creations). Ctrl-C prints what
#     was already created. Nothing here prints environment variables or tokens.
#     Para na primeira falha ao criar (sem repetir às cegas, o que poderia duplicar) e espera --delay
#     segundos entre criações. Ctrl-C mostra o que já foi criado. Nada aqui imprime variáveis de
#     ambiente nem tokens.
#   - Labels are only applied with --with-labels, and only if they already exist in the repo.
#     Rótulos só com --with-labels, e só se já existirem no repositório.
#
# Usage / Uso:
#   scripts/publish-issues.sh                          dry run, all drafts / dry-run, todos
#   scripts/publish-issues.sh --only N05,N06           dry run, just these / dry-run, só estes
#   scripts/publish-issues.sh --publish --only N05     really create N05 (asks for the phrase)
#   options: --dir DIR (default docs/issues)  --repo OWNER/REPO  --delay SECONDS (default 2)
#            --with-labels  --dry-run (the default, explicit)
set -u
DIR=docs/issues; REPO=""; PUBLISH=0; LABELS=0; ONLY=""; DELAY=2
while [ $# -gt 0 ]; do
  case "$1" in
    --publish) PUBLISH=1 ;;
    --dry-run) PUBLISH=0 ;;
    --with-labels) LABELS=1 ;;
    --only) shift; ONLY="${1:-}" ;;
    --dir) shift; DIR="${1:-}" ;;
    --repo) shift; REPO="${1:-}" ;;
    --delay) shift; DELAY="${1:-}" ;;
    -h|--help) sed -n '2,41p' "$0"; exit 0 ;;
    *) echo "opção desconhecida / unknown option: $1" >&2; exit 2 ;;
  esac
  shift
done
case "$DELAY" in ''|*[!0-9]*) echo "ERRO: --delay precisa ser um número inteiro de segundos / --delay must be whole seconds" >&2; exit 2 ;; esac
[ -d "$DIR" ] || { echo "ERRO: pasta não encontrada / folder not found: $DIR" >&2; exit 2; }
command -v gh >/dev/null 2>&1 || { echo "ERRO: gh não encontrado / gh not found" >&2; exit 2; }
RARG=""; [ -n "$REPO" ] && RARG="--repo $REPO"

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
created=0; skipped=0; refused=0; failed=0; made=""
summary() {
  mode="dry-run"; [ "$PUBLISH" = 1 ] && mode="publicado / published"
  echo "resumo / summary ($mode): $created a criar-ou-criadas, $skipped puladas, $refused recusadas, $failed falhas"
  [ -n "$made" ] && echo "criadas / created:$made"
}
trap 'echo; echo "INTERROMPIDO / INTERRUPTED"; summary; exit 130' INT TERM

# normalise a title: lower case, no [P1] prefix, only letters and digits / normaliza um título
norm() { printf '%s' "$1" | tr 'A-Z' 'a-z' | sed 's/^ *\[p[0-3]\]//' | tr -cd 'a-z0-9'; }

# What already exists (open and closed): titles, and the draft id markers inside the bodies.
# O que já existe (abertas e fechadas): títulos e os marcadores de rascunho dentro dos corpos.
# shellcheck disable=SC2086
gh issue list $RARG --state all --limit 1000 --json title --jq '.[].title' > "$tmp/titles" || {
  echo "ERRO: não consegui listar as issues (gh logado? limite da API?) / could not list issues (gh logged in? API limit?)" >&2; exit 2; }
# shellcheck disable=SC2086
gh issue list $RARG --state all --limit 1000 --json body --jq '.[] | (.body // "") | scan("draft-id: N[0-9]+")' > "$tmp/markers" || {
  echo "ERRO: não consegui ler os corpos das issues / could not read the issue bodies" >&2; exit 2; }
: > "$tmp/normtitles"
while IFS= read -r line; do norm "$line"; echo; done < "$tmp/titles" >> "$tmp/normtitles"
if [ "$LABELS" = 1 ]; then
  # shellcheck disable=SC2086
  gh label list $RARG --limit 500 --json name --jq '.[].name' > "$tmp/labels" || {
    echo "ERRO: não consegui listar os rótulos / could not list the labels" >&2; exit 2; }
fi

# ---- pass 1: decide what would be created / passo 1: decide o que seria criado
: > "$tmp/todo"
for f in "$DIR"/N[0-9][0-9]-*.md; do
  [ -f "$f" ] || continue
  id=$(basename "$f" | cut -c1-3)
  if [ -n "$ONLY" ]; then case ",$ONLY," in *",$id,"*) ;; *) continue ;; esac; fi
  title=$(head -n 1 "$f" | sed 's/^# *//')
  if [ -z "$title" ]; then echo "$id: RECUSADA — título vazio / REFUSED — empty title"; refused=$((refused + 1)); continue; fi
  if [ "${#title}" -gt 256 ]; then echo "$id: RECUSADA — título com mais de 256 caracteres / REFUSED — title over 256 characters"; refused=$((refused + 1)); continue; fi
  if [ -z "$(tail -n +2 "$f" | tr -d ' \t\r\n')" ]; then echo "$id: RECUSADA — corpo vazio / REFUSED — empty body"; refused=$((refused + 1)); continue; fi
  if grep -qiE 'claude|anthropic|generated by|chatgpt|openai|copilot' "$f"; then
    echo "$id: RECUSADA — cita ferramenta/assistente de IA / REFUSED — mentions an AI tool or assistant"; refused=$((refused + 1)); continue; fi
  if grep -qE 'ghp_[A-Za-z0-9]{20,}|github_pat_[A-Za-z0-9_]{20,}|gh[ousr]_[A-Za-z0-9]{20,}|AKIA[0-9A-Z]{16}|-----BEGIN [A-Z ]*PRIVATE KEY-----' "$f"; then
    echo "$id: RECUSADA — parece conter segredo/token / REFUSED — looks like it contains a secret"; refused=$((refused + 1)); continue; fi
  if grep -qxF "draft-id: $id" "$tmp/markers"; then
    echo "$id: já existe (marcador do rascunho), pulada / already exists (draft marker), skipped"; skipped=$((skipped + 1)); continue; fi
  if grep -qxF "$title" "$tmp/titles"; then
    echo "$id: já existe (título igual), pulada / already exists (same title), skipped — $title"; skipped=$((skipped + 1)); continue; fi
  nt=$(norm "$title")
  if [ -n "$nt" ] && grep -qxF "$nt" "$tmp/normtitles"; then
    echo "$id: já existe (título parecido), pulada / already exists (similar title), skipped — $title"; skipped=$((skipped + 1)); continue; fi
  lab=""
  if [ "$LABELS" = 1 ]; then
    want=$(grep -m1 '^\*\*Labels sugeridos:\*\*' "$f" | grep -o '`[^`]*`' | tr -d '`')
    for l in $want; do
      if grep -qxF "$l" "$tmp/labels"; then lab="${lab:+$lab,}$l"
      else echo "$id: aviso — rótulo '$l' não existe no repositório, ignorado / warning — label '$l' does not exist, ignored"; fi
    done
  fi
  printf '%s\t%s\t%s\n' "$id" "$f" "$lab" >> "$tmp/todo"
done
n=$(wc -l < "$tmp/todo" | tr -d ' ')

if [ "$PUBLISH" != 1 ]; then
  while IFS="$(printf '\t')" read -r id f lab; do
    echo "$id: [dry-run] criaria / would create — $(head -n 1 "$f" | sed 's/^# *//')${lab:+  (labels: $lab)}"; created=$((created + 1))
  done < "$tmp/todo"
  summary; [ "$failed" = 0 ] && [ "$refused" = 0 ]; exit $?
fi

# ---- confirmation / confirmação
if [ "$n" = 0 ]; then echo "nada a criar / nothing to create"; summary; [ "$refused" = 0 ]; exit $?; fi
echo "Vai criar $n issue(s) no GitHub com a sua conta. / About to create $n issue(s) on GitHub with your account."
cut -f1 "$tmp/todo" | paste -sd' ' -
printf 'Para confirmar digite / To confirm type: PUBLISH %s ISSUES\n> ' "$n"
read -r ans || ans=""
if [ "$ans" != "PUBLISH $n ISSUES" ]; then echo "confirmação diferente da esperada; nada foi criado / confirmation did not match; nothing was created" >&2; exit 1; fi

# ---- pass 2: create / passo 2: cria
first=1
while IFS="$(printf '\t')" read -r id f lab; do
  [ "$first" = 1 ] || { [ "$DELAY" -gt 0 ] && sleep "$DELAY"; }
  first=0
  title=$(head -n 1 "$f" | sed 's/^# *//')
  { tail -n +2 "$f"; printf '\n\n<!-- draft-id: %s -->\n' "$id"; } > "$tmp/body.md"
  # shellcheck disable=SC2086
  if gh issue create $RARG --title "$title" --body-file "$tmp/body.md" ${lab:+--label "$lab"} >/dev/null </dev/null; then
    echo "$id: criada / created — $title"; created=$((created + 1)); made="$made $id"
  else
    echo "$id: FALHOU ao criar / FAILED to create — $title"; failed=$((failed + 1))
    echo "parando na primeira falha; rode de novo (não duplica) / stopping at the first failure; run again (it will not duplicate)" >&2
    break
  fi
done < "$tmp/todo"
summary
[ "$failed" = 0 ] && [ "$refused" = 0 ]
