# Mali barata (preset Odyssey no A15)

Objetivo: Mali só entrega rascunho barato; o painter MGD completa.

## Settings (aplicar no Eden antes de jogar)
- API: Vulkan; shader assíncrono ligado
- CPU: fast CPU + multicore; accuracy no mínimo estável (unsafe se não quebrar)
- Fastmem ligado; GPU accuracy normal (baixa só se não quebrar gráfico)
- Resolução interna: 0.5x (360p), pode testar 0.4x — menos pixel, menos tudo
- Sombras: desligado; Anti-aliasing: desligado
- Blur/motion blur/depth of field/pós: tudo desligado
- Texturas: médio (4 GB RAM); distância de visão baixa
- Docked mode: off; VSync: off; limite de fps: off durante teste
- Flavor: Eden Optimized (genshinSpoof) — relato de +10-15 fps na Mali

## Sistema (A15, fora do Eden)
- Game Booster/modo jogo ligado; economia de bateria DESLIGADA
- Reinicia o aparelho antes de jogar; fecha tudo (força parada, não só minimiza)
- Opções do desenvolvedor: processos em 2º plano no mínimo; animações 0.5x ou off
- NUNCA força 4x MSAA (derruba fps); overlay de HW mantém ligado
- Celular frio; joga sem capa se esquentar

## Regras do mapa mental
- Frame parado não renderiza de novo (reaproveita)
- LOD agressivo: longe = pouco triângulo
- Shaders: 100% do cache pré-compilado, zero compilação ao vivo
- Fechar todos os apps antes (4 GB RAM é o piso)

## Modo MGD Edge (lição do Skyline Edge)
- Dois modos no APK: estável + Edge experimental.
- Edge = tudo agressivo: 0.4x, accuracy mínima, batching máximo,
  frame parado reaproveitado, sem pós nenhum.
- Renderer pensando em GPU tiled (Mali): sem churn de framebuffer,
  draws fundidos, texturas comprimidas.
- Velocidade primeiro, precisão depois (Skyline voava assim).

## Medição
- Anotar fps com e sem cada item, cena parada e cena em movimento.
- Vale o que está medido, não o que foi projetado.
