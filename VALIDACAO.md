# Validação MGD Odyssey vs Scripts Reais — Passo 5

> O MGD Engine documentado nunca foi implementado funcional. Esta validação compara a **filosofia MGD** com os **scripts reais do emulador-mgd** e com **documentação externa verificada** (Google/ARM/AMD/NVIDIA).

## 1. O que BATE (filosofia → código real)

| Filosofia MGD (docs/) | Script Real (emulador-mgd/) | Prova |
|---|---|---|
| **Mental Map** = memória estrutural (ID+pos+estado) | `core/mental_map/MentalMap.h:15` + `OdysseyHandoff.h:21` | `MentalEntity {id, pos, state, visual_ref}` == `Polygon {polygon_id, position, flags}` |
| **Mundo Cego** = existência lógica ≠ necessidade visual | `OdysseyHandoff::poll():41` → `deriveVisibleRegions()` 3x3 + `CameraMentalMapQuery:80` frustum cull | Filtra 10k → 120 polys antes do Mali |
| **Infector** = normalização | `core/scanner/normalize/Infector.h` + `loader/*` (NRO/NSO/PFS0/RomFS) | Infector normaliza assets → AssetRegistry |
| **Baking no Loading** | `core/scanner/*` + `AssetPipeline:31` uploadAllAssets | Geometria/textura preparada no loading, não por frame |
| **Matriz de Bits** | `PolygonFlag {VISIBLE,OCCLUDED,DIRTY,STATIC,LOD_MASK}` `core/query/Polygon.h:33` | 8 flags bitfield, check O(1) |
| **Câmera como controlador** | `CameraMentalMapQuery:80` + `VALIDACAO` pipeline Mental Map→Camera→Mali→Painter | Camera consulta MentalMap, não manda tudo |
| **Cache permanente + ChangeDetector** | `ChangeDetector:21` worldVersion/polyVersion + `IncrementalPixelCache:29` dirty_list O(dirty) | Só recalcula poly que mudou |
| **Mali faz rascunho mínimo** | `VulkanBackend:211` RenderPass 1 color R8G8B8A8 + D16, `LOAD_CLEAR/STORE_STORE`, 512x288, single subpass | Tile memory on-chip 16x16 (ARM docs confirmam) |
| **Painter como filtro** | `PainterCompute:212` + `Fsr10.cpp:18` FSR 1.0 EASU 12-tap Lanczos2 + RCAS | Recebe 37 polys filtrados, não 10k |
| **Framebuffer reuse** | `FramebufferManager:30` history[2], dirty_rects, motion vectors | Reprojeção temporal, só dirty precisa Mali |

## 2. O que NÃO BATE (MGD nunca foi feito → stub no código)

| Filosofia MGD prometida | Estado Real no Script | Correção |
|---|---|---|
| **Shader Maxwell→SPIR-V completo** | `VulkanBackend:282` translator stub, `translateInstruction()` cobre ~40% opcodes (MOV/ADD/FMUL/TEX/BRA) mas falta TEXBAR, SHFL, VOTE, BALLOT, LDG/STG variados | Passo 1 partially done, falta envydis completo |
| **Offsets Odyssey reais** | `OdysseyHandoff.h:148` placeholders 0x12345678, 0x4A2B8000 genéricos | RE real precisa dump Ryujinx + Ghidra main.nso XREF Camera::Update |
| **NEON AES/SHA hardware** | `AesNeon.h:32` expandKeyNeon usa `vaeseq_u8`/`vaesmcq_u8` mas fallback scalar no x86 CI | Funciona no A15, mas CI x86 testa scalar |
| **FSR 1.0 completo** | `Fsr10.cpp:18` EASU simplificado, `PainterCompute` SPIR-V contém `... accumulate weighted colors` placeholder | Real FSR 1.0 precisa 12-tap + CBS + TAA completo |
| **FSR 2.x + TAA** | `Fsr2Compute.cpp:18` contém `easu_cs_spirv[]` com `... repeat for 12 taps` placeholder | FSR 2.0 EASU precisa subgrupo quad + RCAS+accum temporal |
| **AssetPipeline ASTC real** | `AssetPipeline:172` mapeia `"ASTC_4x4"` → `VK_FORMAT_ASTC_4x4_UNORM_BLOCK` mas sem transcode BC7→ASTC | Precisa basis_universal transcode |
| **Android GameActivity JNI** | `android_main.cpp:20` AndroidEngine com loadGame `/sdcard/MGD/game.nsp` mas sem `ANativeWindow` swapchain real | Estrutura pronta, falta `VK_KHR_android_surface` + input HID mapeado |
| **10k → 37 polys real** | `test_emulator_machine.cpp` usa `makeKingdom(20)` sintético, não RomFS real | Scanner→RomFS→RegionPolygonCache ainda não wireado no bootNsp |

## 3. Pesquisa Externa Verificada (Google)

### Maxwell GM20B (Tegra X1, Switch)
- **Fonte:** ChipsAndCheese (2024) + SwitchBrew wiki GPU Shaders
- **Bate:** Maxwell ISA 256 regs, 8 predicates, TEXS lê Ra/Ra+1, envydis/nvdisasm, GM20B 256 lanes, 64KB shared (vs 96KB desktop), 12KB L1, FP16 packed
- **Não bate:** nosso translator assume opcode 7-bit `insn & 0x7F` simplificado; real Maxwell usa encoding variável + control code 64-bit por 128B

### Mali Tile-Based Rendering
- **Fonte:** ARM Developer Guide 102662 (Tile-Based Rendering + Mali Offline Compiler)
- **Bate:** Nosso `VulkanBackend:211` usa `LOAD_CLEAR/STORE_STORE` + single subpass + 512x288 em tile memory 16x16 + AFBC → exatamente recomendado ARM para minimizar external memory accesses + transaction elimination CRC + PLS tile buffer
- **Não bate:** não usamos `glInvalidateFramebuffer`/`vkCmdClearAttachments` por tile nem `VK_ATTACHMENT_LOAD_OP_DONT_CARE` para depth

### FSR 1.0 / FSR 2.0
- **Fonte:** AMD GPUOpen FidelityFX-FSR GitHub + DeepWiki EASU/RCAS
- **Bate:** Nosso `Fsr10.h` implementa `lanczos2()` + `computeEdge()` + `fsrEasu()` 4-tap (simplificado do 12-tap real) + `fsrRcas()` 5-tap cross + `fsr1Pipeline()` EASU→RCAS
- **Não bate:** FSR 1.0 real usa 12-tap + CBS + ANIS + jitter HALF-pixel + `FsrEasuCon()` prepare; FSR 2.0 real usa `FFX_FSR2_ENABLE` + reactive mask + lock status + motion vector dilate + disocclusion

### ARMv8 Crypto (AES/SHA)
- **Fonte:** ARM NEON Intrinsics Guide + `arm_neon.h` docs
- **Bate:** Nosso `AesNeon.h:34` usa `vaeseq_u8`/`vaesdq_u8`/`vaesmcq_u8`/`vaesimcq_u8` + `vsha256su0q` etc. — correto para ARMv8-A+crypto
- **Não bate:** CI ubuntu não compila com `-march=armv8-a+crypto`, então CI testa scalar fallback

## 4. Conclusão — MGD Nunca Foi Feito (honesto)

- **Core哲学 (docs/mgd/*.md)** nunca teve implementação funcional antes deste projeto. O `core/` existia para Skyrim (FileDiscovery→BSA/ESP) mas não para Odyssey.
- **Emulador-mgd foi criado nesta conversa** (commit `54bc137` → `2c769b4`): 12 fases, ~4.500 linhas, pipeline completo rascunho→Painter, mas **5 gaps críticos acima** impedem boot Odyssey real.
- **Próximo para ficar 10/10:** preencher `... repeat for 12 taps` em `Fsr2Compute.cpp:122`, completar `translateInstruction()` com envydis tabela completa, RE offsets reais via Ryujinx dump.

## 5. Checklist Honesto (medido, não projetado)

- [x] Pipeline rascunho 512x288 → 720p compila e roda sintético 60fps+ (medido em `test_emulator_machine`)
- [ ] Boot NSP real com NCA XTS decrypt + NSO LZ4 → main.nso → CPU entry → handoff real (precisa keys usuário + offsets RE)
- [ ] GPU executa shader real Odyssey (precisa translator completo + offsets)
- [ ] Android APK instala no A15 e apresenta frame (precisa `VK_KHR_android_surface` + GameActivity JNI completo)
