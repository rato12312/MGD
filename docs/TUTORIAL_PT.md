# MGD Launcher — Tutorial (Português)

> **MGD — Mental Graphics Driver** • Skyrim é só o detalhe. O MGD pinta a tela total.
> Filosofia: `Mental Map` (mente) = Skyrim nos arquivos, `Câmera` (consciência) vê o que está na frente guiada por colisão, `Painter` (olhos) pinta a tela total. Cada `chunk` guarda seus assets; quanto maior o jogo, mais chunks.

---

## 1. O que você precisa

- **Skyrim Legendary Edition (PC, 2011)** instalado — pasta com `Skyrim.esm`, `Update.esm`, `Dawnguard.esm`, `Hearthfires.esm`, `Dragonborn.esm` e `*.bsa` (`Skyrim - Textures0.bsa` etc.). Funciona também com a pasta do Skyrim no Steam: `.../Steam/steamapps/common/Skyrim/`.
- **Windows 10/11** para o `mgd_launcher.exe` / `mgd_app.exe`. Linux funciona com `build/mgd_app` (headless).
- Não precisa copiar assets — o MGD guarda **só IDs** em `cache/`.

## 2. Baixar o exe (compilado pelo GitHub Actions = Codespace)

1. Abra o repositório no GitHub → aba **Actions** → workflow **Build MGD Engine** → último run verde (commit `mgd_launcher`).
2. Em **Artifacts**, baixe `mgd-exe-windows`. Descompacte — vem:
   - `build/mgd_launcher.exe` — launcher UI estilo PS5/GameHub
   - `build/mgd_app.exe` — demo headless (gera `output.ppm`)
   - `build/mgd_tests.exe` — 9 suítes de teste
   - `interface/launcher/` — UI web (fallback sem SDL2)

> Sem SDL2? Abra `interface/launcher/index.html` direto no navegador — é a mesma UI.

## 3. Usar o Launcher

### Opção A — `mgd_launcher.exe` (nativo)
1. Dê duplo clique em `mgd_launcher.exe`.
2. Tela PS5: hero com **SKYRIM LEGENDARY EDITION** + badges Dawnguard/Hearthfire/Dragonborn + olho MGD pulsando.
3. **Adicionar Skyrim**: clique em **＋ Adicionar Skyrim** e selecione a pasta do Skyrim (a que contém `Skyrim.esm`). O caminho aparece em `path-line`.
4. Clique em **◷ Scan**. O log mostra:
   - `BSAAnalyzer: Skyrim - Textures0.bsa`
   - `ESPAnalyzer: Skyrim.esm → Update.esm → Dawnguard.esm → Hearthfires.esm → Dragonborn.esm`
   - `ChunkManager: 4096×4096` — chunks são criados **automaticamente** (um por região, sem config manual)
   - `Cache: IDs em cache/`
5. Veja os painéis:
   - **Chunks Automáticos**: grid 8×8, azul = ativo, amarelo = sibling. `4096 × 4096 • N chunks`.
   - **Cache por IDs**: `FNV-1a 64` full-file, `N IDs`.
   - **Câmera**: posição, `visíveis / considerados`.
   - **Painter**: draw calls, tris, pixels.
6. Clique em **▶ Rodar MGD**. O headless demo roda e gera `output.ppm` na pasta do exe. Abra o PPM no visualizador de imagens.

### Opção B — UI web (sem compilar)
1. Abra `interface/launcher/index.html` no Chrome/Edge.
2. Mesmo fluxo: **Adicionar Skyrim** (usa `webkitdirectory` — selecione a pasta), **Scan**, **Rodar MGD**.
3. Carousel horizontal com snap (arraste ou use ‹ ›), `Adicionar jogo` cria novo card.

## 4. Como funciona por dentro

```
Skyrim LE arquivos (.nif/.dds/.esp/.esm/.esl/.bsa/.pex)
  → SkyrimPCAdapter (ESP/BSA/RAW, LE-DLC3)
  → BSAAnalyzer / ESPAnalyzer / MeshAnalyzer / TextureAnalyzer
  → RawAnalysisResult {type, metadata, dependency_ids, spatial_bounds}
  → Infector → IDs em cache/ (sem cópia)
  → ChunkManager distribui em chunks 4096 (RegionID = pack x,z)
  → MentalMapBuilder cria Region/Chunk automaticamente
  → CollisionSystem (GridIndex) + RaycastSystem
  → Camera guiada por colisão (não atravessa parede)
  → BasicVisibility (frustum culling)
  → Painter pinta tudo (skybox + raster)
  → output.ppm / SDL2Presenter
```

- **Chunks**: `core/mental_map/ChunkManager.h:30` `worldToChunk()` → `RegionID`. `builder/MentalMapBuilder.cpp:15` cria `Region` automaticamente.
- **Cache**: `core/scanner/HashService.cpp:8` FNV-1a 64 em 64KB chunks; `Scanner.cpp:57` valida `hash == entry.hash` para evitar colisão 32-bit.
- **Câmera**: `core/camera/CameraController.cpp:11` faz `RaycastSystem::raycast` antes de `moveForward/Right/Up` e faz slide.
- **BSA/ESP**: `core/scanner/analyzers/BSAAnalyzer.cpp:11` e `ESPAnalyzer.cpp:11`.

## 5. Controles no jogo (quando rodar com SDL2)

- **WASD / Setas**: mover
- **Mouse**: olhar (segure botão esquerdo para capturar)
- **Q/E**: subir/descer
- **1/2/3/4**: Debug (Normal/Wireframe/Depth/Bounds)
- **ESC**: sair

## 6. Solução de problemas

- **Nenhum arquivo encontrado**: aponte para a pasta que contém `Skyrim.esm`, não para `Data/` sozinha.
- **Scan 0 entities**: BSA/ESP com header inválido — veja log em `ScanReport.errors`; `MeshAnalyzer` valida `NIF\0`.
- **Tela preta**: verifique `output.ppm` — deve ter `P6 800 600 255` + 1.44MB de pixels. Se 0 tris, o scan não achou meshes.
- **Câmera atravessa parede**: `CameraController` precisa de `setCollisionSystem(&collision)` — o `src/main.cpp:176` já faz `cam_ctrl_.setCollisionSystem(&collision_)` no modo SDL2.

## 7. Compilar local (opcional)

```bash
# Linux/WSL (ubuntu)
sudo apt-get install -y g++ libsdl2-dev
chmod +x build.sh && ./build.sh
./build/mgd_tests   # 9/9
./build/mgd_app      # gera output.ppm

# Windows (MinGW)
g++ -std=c++17 -Wall -Wextra -I. -DMGD_TESTING core/**/*.cpp tests/**/*.cpp -o build/mgd_tests.exe
g++ -std=c++17 -Wall -Wextra -I. core/**/*.cpp src/main.cpp -o build/mgd_app.exe
```

---

**Skyrim é o input, MGD é o motor.** Dúvidas? Abra uma issue com o caminho da pasta e o log do Scan.
