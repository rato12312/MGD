# Emulador MGD — progresso (só Odyssey)

Regra: % sempre. Plano: 5 etapas de 20%.

## Placar (~14%)
- Etapa 1 — fundação executável: CPU+MMU+threads+HOS amarrados (feito)
- Etapa 2 — serviços de boot: acc/applet/pm/sm/nv/vi/aud/fs/hid/time (feito)
- Etapa 3 — dados do jogo: NRO/NSO/PFS0/RomFS+subdir, AES/SHA (feito)
- Etapa 4 — GPU recebe: submit+fence+dreno+bytes guardados (iniciado)
- Etapa 5 — integração e medida: frame+fps+preset (iniciado)

## Detalhe por área
- CPU inteira 64-bit: ~85%. 32-bit: ~80%. FP escalar: ~70%.
- NEON vetorial: ~18%. MMU: ~20%. SVC/HOS: ~15%.
- Loaders: ~60%. Cripto: pronta. GPU execução: ~4%.
- Mundo/painter: mapa mental + DNA + LOD + preset + PPM.

## Para rodar o Odyssey falta (ordem)
1. NEON inteiro completo (8H/4S/16B em tudo) + resto FP
2. Exceções/IRQ + mais sysregs
3. NCA descripto (com chaves do usuário) + ExeFS
4. IPC over SVC (passar buffers de verdade)
5. GPU Maxwell executando (o gigante)
6. Applet manager + boot flow real

## Regras do projeto
- Peça nova só entra com teste verde no CI.
- Vale o medido, não o projetado.
- Encaixou MGD, otimiza; não encaixou, pula.
- Sem copiar asset: só IDs + posição.
