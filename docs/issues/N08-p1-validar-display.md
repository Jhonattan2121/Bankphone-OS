# [P1] Validar framebuffer, resolução, stride e formato de pixel no Infinix Hot 30i, com evidência arquivada

**Labels sugeridos:** `priority:P1`, `area:hardware`, `area:ui`, `type:research`
**Tipo:** validação (o código existe; a prova no aparelho não está no repositório)
**Precisa do Infinix Hot 30i:** sim
**EN:** Validate framebuffer geometry and pixel format on the X669C and archive the evidence.

## Contexto
O sistema desenha por software direto no framebuffer. Se resolução, stride ou ordem de canais estiverem errados, a tela fica corrompida ou com cores trocadas.

## Estado atual
- `bd_fb_open` descobre o nó, lê `FBIOGET_VSCREENINFO`/`FSCREENINFO`, aceita 16/24/32 bpp e mapeia `stride × (yoffset + altura)` (`init/bootdiag.c:700-726`).
- `blit()` usa o menor entre o tamanho da tela e o da UI e trata BGR/RGB/565 (`init/main.c:140-168`). Um comentário diz que o aparelho é BGR em memória; **não há captura arquivada que prove isso**.
- A UI é projetada para 720×1612 (`touch_arm` usa 720/1612 como pior caso, `init/main.c:295`).
- Desenho testado no host (28 checks em `init/tests/test_bootdiag_fb.c`).
- Desempenho "pior quadro 13 ms" é **relatado pelo autor**; o código imprime uma linha `perf:` a cada 5 s (`init/main.c:520-537`).

## Problema
Não há evidência arquivada de que os parâmetros reais do painel batem com o que o código assume, nem teste simples de pixels e limites que rode no aparelho.

## Objetivo
Evidência reproduzível (log mais foto) de resolução, stride, bpp, ordem de canais e barras de cor corretas, e um teste de limites.

## Tarefas
- [ ] Capturar a linha `fb: ... WxH bpp= stride= red= green= blue=` do log e comparar com o assumido no código.
- [ ] Fotografar as barras de cor (`bars_gfx`) e confirmar a ordem das cores.
- [ ] Tela de teste: gradientes, moldura de 1 pixel nas quatro bordas, quadrados nos cantos. Confirmar que nada aparece cortado nem deslocado.
- [ ] Extrair a conversão de pixels de `blit()` para uma função testável no host, com teste dos três formatos e de `xoffset`/`yoffset` não nulos.
- [ ] Verificar que `xoffset + largura` não passa de `stride` (hoje não há essa checagem em `blit()`).
- [ ] Registrar as linhas `perf:` de pelo menos 2 minutos de uso.
- [ ] Arquivar tudo em `docs/evidence/display/` (sem IMEI, número de série nem etiquetas).

## Critérios de aceite
- [ ] Parâmetros do painel registrados com log e foto.
- [ ] Teste de pixels e limites rodando no host.
- [ ] Nenhum acesso fora do buffer nos testes (ASan limpo).
- [ ] README declara o display como "relatado" ou "verificado em <data>".

## Testes
Host: novo teste de conversão e limites. Hardware: barras, moldura e `perf:`.

## Riscos
Baixo (apenas leitura e desenho). Cuidado com fotos que mostrem identificadores.

## Dependências
N01 (coleta do log). Independente de N09.

## Evidências
`init/bootdiag.c:700-726`, `init/main.c:140-168`, `init/main.c:295`, `init/main.c:520-537`, `init/tests/test_bootdiag_fb.c`.

## Fora do escopo
Redesenhar a interface (N, "UI utilizável") e trocar o método de desenho.

## Definição de concluído
Evidência arquivada, teste de host mesclado, README atualizado.
