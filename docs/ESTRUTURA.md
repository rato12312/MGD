# MGD — Estrutura de Pastas (obra-prima organizada)

```
MGD/
├── .agents/skills/              # Skills (frontend-design)
│   └── frontend-design/         # PS5/GameHub UI (Space Grotesk/Inter)
├── .github/workflows/
│   └── build.yml                # CI: ubuntu g++ + windows msys2 → mgd_tests.exe/mgd_app.exe + launcher
├── core/                        # C++ Core (C++17)
│   ├── cache/                   # FileCache (MGDC v1, FNV-1a 64) — IDs perm
│   ├── camera/                  # Camera + CameraController (guiada por colisão via Raycast)
│   ├── collision/               # CollisionSystem + GridIndex (cell 100) + Raycast
│   ├── common/                  # Mat4, Vec3, AABB, Plane (frustum), Frustum
│   ├── mental_map/              # MentalMap + Region + ChunkManager (4096) + builder
│   ├── painter/                 # Painter (tile), Framebuffer, DepthBuffer, Rasterizer, VertexProcessor
│   ├── renderer/                # Renderer SDL2 + SDL2Presenter
│   ├── scanner/                 # Scanner (cache por IDs, chunks automáticos)
│   │   ├── adapters/            # SkyrimPCAdapter (LE) + SkyrimXbox360Adapter
│   │   ├── analyzers/           # Mesh/Texture/BSA/ESP/Metadata
│   │   └── normalize/           # Infector (Entity/Resource/Collision/Material/Texture)
│   └── visibility/              # BasicVisibility (frustum_aabb + containsAABB)
├── docs/
│   ├── TUTORIAL_PT.md           # Tutorial PT (launcher, LE, chunks, 30 FPS HD4000)
│   ├── TUTORIAL_EN.md           # Tutorial EN
│   └── ESTRUTURA.md             # este arquivo
├── interface/
│   ├── launcher/                # Launcher web PS5 (obra-prima)
│   │   ├── index.html           # Hero Skyrim LE + carousel snap + res-menu
│   │   ├── styles.css           # Tokens PS (void #060A14, PS blue #0070CC)
│   │   └── app.js               # webkitdirectory, scan mock LE, chunks 8×8
│   ├── launch_mgd.py            # Launcher Python
│   └── mgd_interface.py
├── src/
│   ├── main.cpp                 # Headless demo (MentalMap → Camera → Collision → Visibility → Painter → output.ppm)
│   └── mgd_launcher.cpp         # Launcher nativo SDL2 (PS5 cards)
├── tests/                       # 9 suítes (types, entity, mental_map, camera, collision, visibility, framebuffer, rasterizer, painter)
├── CMakeLists.txt               # mgd_core + mgd_prototype + mgd_launcher + mgd_tests
├── build.sh                     # g++ -std=c++17 -I. -DMGD_TESTING (find core/**/*.cpp)
└── README.md
```

- **Fluxo**: `Skyrim LE` → `FileDiscovery` → `BSA/ESPAnalyzer` → `Infector` → `cache/` → `ChunkManager` → `MentalMap` → `Collision` → `Camera` → `Visibility` → `Painter` → `output.ppm` / `SDL2`.
- **Padrão**: cada `chunk` guarda seus `assets` (`RegionID` por `worldToChunk`), `cache` guarda só `IDs`.
