# [P2] Padronizar os logs de boot (níveis, códigos) e não vazar identificadores do aparelho

**Labels sugeridos:** `priority:P2`, `area:boot`, `area:security`, `type:feature`
**Tipo:** melhoria + risco de privacidade (F-08, **não verificado no aparelho**)
**Precisa do Infinix Hot 30i:** parcialmente (capturar um relatório real)
**EN:** Standardize boot logs (levels, error codes) and avoid leaking device identifiers.

## Contexto
O diagnóstico depende de logs. O mesmo log vai para a tela, a serial USB (qualquer host que abra a porta) e, na imagem completa, para uma partição. Ele também é compartilhado publicamente em dev logs e issues.

## Estado atual
- `bd_log(fmt, ...)` (`init/bootdiag.c`) não tem nível; há centenas de chamadas livres em português.
- Existe uma numeração de estágios (1 a 16) e códigos de falha (`enum bd_fail`, `init/bootdiag.h:47-60`, e `F_FONT`, `F_ASSETS`, `F_UI`, `F_PSTORE` em `init/main.c:58-61`), mas os códigos não são usados de forma uniforme nas mensagens de texto.
- O relatório despeja `/proc/cmdline`, `/proc/mounts` e `/proc/partitions` (`init/bootdiag.c:913`, `:965-966`) e é enviado pela serial quando o host levanta o DTR.
- **Conferido por `grep`:** os logs de `ui.c`, `sheets.c`, `screens.c`, `money.c` e `sec.c` **não imprimem PIN nem hash**.
- A cmdline típica de aparelhos Android pode conter número de série e outros identificadores. **NÃO VERIFICADO neste aparelho.**
- Falha ao gravar o relatório é registrada e o sistema segue (`init/bootdiag.c:415`); bom.

## Problema
1. Sem nível nem código estável, não dá para filtrar nem automatizar a leitura.
2. O relatório pode expor identificadores quando for compartilhado.
3. Não há limite documentado para o tamanho do log persistente além dos dois slots de 64 KiB.

## Objetivo
Logs com nível e código, um filtro de sanitização para o que sai do aparelho, e documentação de como coletar e interpretar.

## Tarefas
- [ ] Definir níveis (`ERR`, `WARN`, `INFO`, `DBG`) e um formato fixo: `[nível][fase][código] mensagem`.
- [ ] Padronizar códigos de erro numa tabela única (`init/errors.h`) e documentar em `docs/LOGS.md`.
- [ ] No aparelho: capturar um relatório real e listar **quais campos** são identificadores (série, MAC, IMEI parcial, nomes de partição). Só então decidir o que redigir.
- [ ] Criar um filtro de redação aplicado a `/proc/cmdline` e inventário **antes** de ir para a serial e para o relatório (por exemplo, trocar o valor de `androidboot.serialno=`, MACs e semelhantes por `<redigido>`).
- [ ] Manter um modo de depuração explícito (`bankphone.logfull=1`) que não redige, desligado por padrão.
- [ ] Teste de host: entradas de exemplo com identificadores, esperando a saída redigida.
- [ ] Confirmar que a falha de log nunca derruba o sistema (teste com backend de relatório que falha).
- [ ] Documentar o limite de tamanho e a política de sobrescrita.

## Critérios de aceite
- [ ] Todo `bd_log` novo tem nível e fase.
- [ ] O relatório padrão não contém número de série nem MAC.
- [ ] `docs/LOGS.md` explica como coletar e interpretar.
- [ ] Teste de redação passa; falha de log não derruba o sistema.

## Testes
Host: redação, formato, falha do backend. Hardware: relatório real antes e depois.

## Riscos
Redigir demais e perder informação de diagnóstico. Por isso o modo `logfull` e a decisão baseada em relatório real.

## Dependências
N01 (coleta), N06 (relatório em partição), N10 (serial).

## Evidências
`init/bootdiag.h:47-60`, `init/main.c:58-61`, `init/bootdiag.c:913`, `:965-966`, `docs/PROJECT_AUDIT.md` (F-08).

## Fora do escopo
Mudar o idioma das mensagens e logs de rede (não existe rede).

## Definição de concluído
Formato e códigos documentados, redação testada, relatório real conferido.
