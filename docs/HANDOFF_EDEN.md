# Handoff Eden → MGD (captura)

Contrato já existe: `core/bridge/EmulatorHandoff.h` (`HandoffFrame`:
câmera + regiões/polígonos visíveis por ID, sem copiar assets).
Falta: `IHandoffSource` lendo estado real do Eden.

## Onde fisgar no Eden (a investigar no checkout ~/eden)
- Renderer Vulkan: `src/video_core/renderer_vulkan/` — fim do frame
  (`SwapBuffers`/apresentação) é o ponto de captura por frame.
- Lista de draws do frame → deriva `visible_polygons` (mapear
  draw → PolygonID via cache do mapa mental, não copiar vértice).
- Câmera: vem do estado do jogo (RAM emulada), não do renderer.
  Caminho: serviço/camera do HOS ou leitura da RAM no endereço
  que o Odyssey usa (exige engenharia reversa por jogo).
- UI: flag `ui_visible` via camada de applet/overlay do HOS.

## Ordem de implementação
1. Hook vazio no fim do frame (só conta frames, sem ler nada).
2. Captura de câmera (primeiro via chute: posição fixa → depois real).
3. Derivação de polígonos visíveis a partir dos draws.
4. Endereço da câmera do Odyssey na RAM (por versão do jogo).

## Regras
- Nunca copiar asset: só IDs + posição (filosofia do MGD).
- Captura nunca trava o frame: falha fechada (sem estado → `poll` false).
- Medir custo do hook (tem que ser < 1 ms/frame).
