# MGD Launcher — Tutorial (English)

> **MGD — Mental Graphics Driver** • Skyrim is just the detail. MGD paints the whole screen.
> Philosophy: `Mental Map` (mind) = Skyrim in files, `Camera` (consciousness) sees what's in front guided by collision, `Painter` (eyes) paints the total screen. Each `chunk` holds its assets; the bigger the game, the more chunks.

---

## 1. What you need

- **Skyrim Legendary Edition (PC, 2011)** installed — folder with `Skyrim.esm`, `Update.esm`, `Dawnguard.esm`, `Hearthfires.esm`, `Dragonborn.esm` and `*.bsa` (`Skyrim - Textures0.bsa` etc.). The Steam folder `.../Steam/steamapps/common/Skyrim/` also works.
- **Windows 10/11** for `mgd_launcher.exe` / `mgd_app.exe`. Linux works with `build/mgd_app` (headless).
- No asset copy needed — MGD stores **IDs only** in `cache/`.

## 2. Download the exe (built by GitHub Actions = Codespace)

1. Open the repo on GitHub → **Actions** tab → **Build MGD Engine** workflow → latest green run.
2. Under **Artifacts**, download `mgd-exe-windows`. Unzip — you get:
   - `build/mgd_launcher.exe` — PS5/GameHub style launcher UI
   - `build/mgd_app.exe` — headless demo (writes `output.ppm`)
   - `build/mgd_tests.exe` — 9 test suites
   - `interface/launcher/` — web UI (fallback without SDL2)

> No SDL2? Just open `interface/launcher/index.html` in your browser — same UI.

## 3. Using the Launcher

### Option A — `mgd_launcher.exe` (native)
1. Double-click `mgd_launcher.exe`.
2. PS5 hero shows **SKYRIM LEGENDARY EDITION** + Dawnguard/Hearthfire/Dragonborn badges + pulsing MGD eye.
3. **Add Skyrim**: click **＋ Add Skyrim** and pick your Skyrim folder (the one with `Skyrim.esm`). The path appears in the path line.
4. Click **◷ Scan**. The log shows:
   - `BSAAnalyzer: Skyrim - Textures0.bsa`
   - `ESPAnalyzer: Skyrim.esm → Update.esm → Dawnguard.esm → Hearthfires.esm → Dragonborn.esm`
   - `ChunkManager: 4096×4096` — chunks are created **automatically** (one per region, no manual config)
   - `Cache: IDs in cache/`
5. Check the panels:
   - **Automatic Chunks**: 8×8 grid, blue = active, yellow = sibling. `4096 × 4096 • N chunks`.
   - **Cache by IDs**: `FNV-1a 64` full-file, `N IDs`.
   - **Camera**: position, `visible / considered`.
   - **Painter**: draw calls, tris, pixels.
6. Click **▶ Run MGD**. The headless demo runs and writes `output.ppm` next to the exe. Open the PPM in any image viewer.

### Option B — Web UI (no build)
1. Open `interface/launcher/index.html` in Chrome/Edge.
2. Same flow: **Add Skyrim** (uses `webkitdirectory` — pick the folder), **Scan**, **Run MGD**.
3. Horizontal snap carousel (drag or ‹ ›), **Add game** creates a new card.

## 4. How it works inside

```
Skyrim LE files (.nif/.dds/.esp/.esm/.esl/.bsa/.pex)
  → SkyrimPCAdapter (ESP/BSA/RAW, LE-DLC3)
  → BSAAnalyzer / ESPAnalyzer / MeshAnalyzer / TextureAnalyzer
  → RawAnalysisResult {type, metadata, dependency_ids, spatial_bounds}
  → Infector → IDs in cache/ (no copy)
  → ChunkManager splits into 4096 chunks (RegionID = pack x,z)
  → MentalMapBuilder auto-creates Region/Chunk
  → CollisionSystem (GridIndex) + RaycastSystem
  → Camera guided by collision (won't clip through walls)
  → BasicVisibility (frustum culling)
  → Painter paints everything (skybox + raster)
  → output.ppm / SDL2Presenter
```

- **Chunks**: `core/mental_map/ChunkManager.h:30` `worldToChunk()` → `RegionID`. `builder/MentalMapBuilder.cpp:15` auto-creates `Region`.
- **Cache**: `core/scanner/HashService.cpp:8` FNV-1a 64 streaming in 64KB chunks; `Scanner.cpp:57` validates `hash == entry.hash` to avoid 32-bit collision.
- **Camera**: `core/camera/CameraController.cpp:11` does `RaycastSystem::raycast` before `moveForward/Right/Up` and slides.
- **BSA/ESP**: `core/scanner/analyzers/BSAAnalyzer.cpp:11` and `ESPAnalyzer.cpp:11`.

## 5. In-game controls (when running with SDL2)

- **WASD / Arrows**: move
- **Mouse**: look (hold left button to capture)
- **Q/E**: up/down
- **1/2/3/4**: Debug (Normal/Wireframe/Depth/Bounds)
- **ESC**: quit

## 6. Troubleshooting

- **No files found**: point to the folder that contains `Skyrim.esm`, not just `Data/` alone.
- **Scan 0 entities**: BSA/ESP with invalid header — see `ScanReport.errors`; `MeshAnalyzer` validates `NIF\0`.
- **Black screen**: check `output.ppm` — should be `P6 800 600 255` + 1.44MB of pixels. If 0 tris, the scan found no meshes.
- **Camera clips through wall**: `CameraController` needs `setCollisionSystem(&collision)` — `src/main.cpp:176` already does `cam_ctrl_.setCollisionSystem(&collision_)` in SDL2 mode.

## 7. Build locally (optional)

```bash
# Linux/WSL (ubuntu)
sudo apt-get install -y g++ libsdl2-dev
chmod +x build.sh && ./build.sh
./build/mgd_tests   # 9/9
./build/mgd_app      # writes output.ppm

# Windows (MinGW)
g++ -std=c++17 -Wall -Wextra -I. -DMGD_TESTING core/**/*.cpp tests/**/*.cpp -o build/mgd_tests.exe
g++ -std=c++17 -Wall -Wextra -I. core/**/*.cpp src/main.cpp -o build/mgd_app.exe
```

---

**Skyrim is the input, MGD is the engine.** Questions? Open an issue with your folder path and the Scan log.
