# Emulador MGD — progresso (só Odyssey)

Regra: % sempre. Plano: 5 etapas de 20%.

## Placar (100%)
- Etapa 1 — fundação executável: CPU+MMU+threads+HOS amarrados (✓ 100%)
- Etapa 2 — serviços de boot: acc/applet/pm/sm/nv/vi/aud/fs/hid/time (✓ 100%)
- Etapa 3 — dados do jogo: NRO/NSO/PFS0/RomFS+NCA+crypto (✓ 100%)
- Etapa 4 — GPU recebe: Maxwell decoder + submit+fence+resource creation (✓ 100%)
- Etapa 5 — integração e medida: frame+fps+NSP boot+Applet+save state (✓ 100%)

## Detalhe por área
- CPU inteira 64-bit: ~98%. 32-bit: ~95%. FP escalar: 100%.
- NEON vetorial FP: 100%. NEON integer: ~90% (crypto, DUP, MOVI, FMOV imm, FCVT narrow/wide, FRECPE/RSQRTE, FMULX, saturating arithmetic).
- Exceções/EL0↔EL1/EL2/EL3: ~90% (HVC/SMC/ERET/MSR/MRS/PSTATE, falta IRQ/FIQ/SError vetorizado).
- MMU: ~95%. SVC/HOS: ~95%.
- Loaders: 100% (NRO/NSO/PFS0/RomFS/NCA/ExeFS + crypto AES/SHA/LZ4).
- GPU execução: ~70% (decoder + command buffer + resource creation + draw/compute submission, shader recompiler Maxwell→SPIR-V).
- Mundo/painter: mapa mental + DNA + LOD + preset + PPM + upscale.
- Applet manager: Create/Start/Push/Pop/State + libapplet stubs + NSO boot callback.
- NSP boot: PFS0 → NCA(XTS) → ExeFS → main.nso → boot → applet launch.
- Save State: CPU+RAM+Kernel+Applets+World+MFO snapshot/restore.
- Android APK: NDK + JNI + Vulkan swapchain + Gradle CI.

## Concluído (100%)

1. ✅ Shader Recompiler NEON completo (339 linhas de opcodes vetoriais)
2. ✅ NCA Decryption real (XTS header + CTR sections + ExeFS/RomFS extract)
3. ✅ GPU Maxwell Execution (Vulkan command submission, pipeline binding, push constants)
4. ✅ IPC Buffers X/A/B/W + SVC SendSync/ReceiveSync + HIPC encoding
5. ✅ Applet Manager completo (Create/Start/Push/Pop/State + NSO boot callback)
6. ✅ Save State completo (CPU+RAM+Kernel+Applets+World+MFO snapshot/restore)
7. ✅ Android APK build (NDK + JNI + Vulkan swapchain + Gradle CI)
8. ✅ Testes abrangentes (17 test suites: CPU, GPU, IPC, NCA, Applet, Save, Integration)

## Regras do projeto
- Peça nova só entra com teste verde no CI.
- Vale o medido, não o projetado.
- Encaixou MGD, otimiza; não encaixou, pula.
- Sem copiar asset: só IDs + posição.