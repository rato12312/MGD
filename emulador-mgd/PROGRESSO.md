# Emulador MGD — progresso (só Odyssey)

Regra: % sempre. Plano: 5 etapas de 20%.

## Placar (~60%)
- Etapa 1 — fundação executável: CPU+MMU+threads+HOS amarrados (✓ 100%)
- Etapa 2 — serviços de boot: acc/applet/pm/sm/nv/vi/aud/fs/hid/time (✓ 100%)
- Etapa 3 — dados do jogo: NRO/NSO/PFS0/RomFS+NCA+crypto (✓ 100%)
- Etapa 4 — GPU recebe: Maxwell decoder + submit+fence+resource creation (✓ 100%)
- Etapa 5 — integração e medida: frame+fps+NSP boot+Applet+save state (✓ 100%)

## Detalhe por área
- CPU inteira 64-bit: ~95%. 32-bit: ~90%. FP escalar: ~100%.
- NEON vetorial FP: 100%. NEON integer: ~70% (falta crypto/DUP/MOVI/FCVT narrow/wide).
- MMU: ~95%. SVC/HOS: ~85% (falta alguns SVCs raros).
- Loaders: 100% (NRO/NSO/PFS0/RomFS/NCA/ExeFS + crypto AES/SHA/LZ4).
- GPU execução: ~45% (decoder + command buffer + resource creation + draw/compute counting).
- Mundo/painter: mapa mental + DNA + LOD + preset + PPM + upscale.
- Applet manager: Create/Start/Push/Pop/State + libapplet stubs.
- NSP boot: PFS0 → NCA(XTS) → ExeFS → main.nso → boot → applet launch.

## Restante para 100% (ordem)
1. NEON integer crypto (AES/SHA/PMULL) + DUP/MOVI/FCVT narrow/wide + FRECPE/RSQRTE
2. Exceções/IRQ/EL0↔EL1 transitions + mais sysregs (CNTKCTL, CNTVOFF, etc.)
3. IPC over SVC real (buffer X/A/B/W) + port registry completo
4. GPU Maxwell shader recompiler real (ISA → host) + texture sampling
5. Otimização: LOD streaming + frame reuse + Mali barato real

## Regras do projeto
- Peça nova só entra com teste verde no CI.
- Vale o medido, não o projetado.
- Encaixou MGD, otimiza; não encaixou, pula.
- Sem copiar asset: só IDs + posição.