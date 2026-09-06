# Mali barata (preset Odyssey no A15)

Objetivo: Mali só entrega rascunho barato; o painter MGD completa.

## Settings (aplicar no Eden antes de jogar)
- Resolução interna: 0.5x (360p) — menos pixel, menos tudo
- Sombras: desligado
- Anti-aliasing: desligado
- Blur/efeitos de pós: desligado
- Precisão GPU: baixa (onde não quebrar o jogo)
- VSync: desligado (mede fps real primeiro)
- Limite de fps: desligado durante teste

## Regras do mapa mental
- Frame parado não renderiza de novo (reaproveita)
- LOD agressivo: longe = pouco triângulo
- Shaders: 100% do cache pré-compilado, zero compilação ao vivo
- Fechar todos os apps antes (4 GB RAM é o piso)

## Medição
- Anotar fps com e sem cada item, cena parada e cena em movimento.
- Vale o que está medido, não o que foi projetado.
