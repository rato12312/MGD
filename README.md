# MGD Engine - Mental Graphics Driver

Uma arquitetura de renderização alternativa que mantém uma representação lógica do mundo ("Mapa Mental") e usa um "Pintor" para produzir pixels diretamente, sem o pipeline 3D tradicional.

Filosofia: o jogo simula e entrega estado; o MGD identifica, referencia, verifica mudanças e reutiliza o máximo possível — descobre uma vez, usa sempre.

## Arquitetura

```
Jogo / Emulador (estado e simulação)
    ↓
Mapa Mental (chunks, regiões, entidades)
    ↓
Câmera (consciência, guiada por colisão)
    ↓
Visibilidade (só o que a câmera vê)
    ↓
Consultador por polígonos (Região → PolygonID → AssetID)
    ↓
DNA (identidade compacta + índices + LOD)
    ↓
Mapa de pixels + Código de cor (LUT)
    ↓
Cache incremental (só o que mudou)
    ↓
Pintor → Framebuffer (RGBA) → Tela
```

## Conceitos principais

### Mapa Mental
Representação lógica do mundo com entidades contendo:
- ID único
- Posição 3D (X, Y, Z)
- Estado (Active, Inactive, Hidden)
- Referência visual e cor RGBA completa (0-255 por canal)
- Região/chunk (partição espacial automática estilo Minecraft)

### Câmera
- Posição, orientação, FOV, near/far
- Conversão mundo↔tela e frustum culling
- Guiada por colisão (não atravessa paredes)

### Consultador por polígonos
- `Região → polígonos → PolygonID → AssetID → asset`, sem varredura global
- `0 = polígono`, `1 = linha` (BitSpace), cada asset com IDs próprios

### DNA / Seed Provider
- DNA compacto por polígono (posição via índice XYZ, cor, LOD, pixel map)
- Seed prevê regiões prováveis e aquece shaders só do que vai aparecer
- Detecção de mudanças mundo → objeto → polígono

### Pintor
- Framebuffer RGBA completo, skybox, rasterização com depth test
- Cache de shaders por `AssetID + PolygonID + LOD`, com warmup e persistência
- Framebuffer incremental: só reescreve o pixel alterado

## Estrutura do projeto

```
MGD/
├── core/                  # Núcleo C++
│   ├── mental_map/        # Entity, MentalMap, ChunkManager, Region
│   ├── camera/            # Camera + CameraController
│   ├── collision/         # CollisionSystem, GridIndex, Raycast
│   ├── visibility/        # BasicVisibility
│   ├── query/             # RegionPolygonCache, PolygonConsultant, dna/, seed/
│   ├── painter/           # Painter, Framebuffer, Rasterizer, shader/
│   ├── scanner/           # Scanner Skyrim LE (BSA/ESP), cache por IDs
│   ├── bridge/            # Handoff Eden -> MGD (contrato)
│   └── renderer/          # Renderer (SDL2)
├── interface/launcher/    # Launcher web estilo GameHub
├── docs/                  # Tutoriais PT/EN (TUTORIAL_PT.md, TUTORIAL_EN.md)
├── tools/                 # benchmark_polygon
├── tests/                 # 16 suítes de teste
├── src/                   # main (headless), mgd_launcher
├── CMakeLists.txt
└── README.md
```

## Dependências

- Compilador **C++17** (MSVC, GCC, Clang)
- **CMake** 3.16+
- **SDL2** (opcional, só para o launcher/janela)

## Compilando

```bash
chmod +x build.sh && ./build.sh
./build/mgd_tests     # 16 suítes
./build/mgd_app        # demo headless -> output.ppm
```

Ou via CMake:

```bash
mkdir build && cd build
cmake .. && cmake --build . --config Release
```

## Benchmark do consultador

```bash
g++ -std=c++17 -Wall -Wextra -I. $(find core -name '*.cpp' | tr '\n' ' ') tools/benchmark_polygon.cpp -o build/benchmark_polygon
./build/benchmark_polygon
```

Mede 1k/10k/100k/1M de consultas mais a tabela TESTE 0..8 (baseline tradicional vs pipeline completo, com FPS médio e 1% low).

## Launcher

- Web: abra `interface/launcher/index.html` no navegador (importa a pasta do jogo, scan, resolução, chunks, cache).
- Nativo: `src/mgd_launcher.cpp` (SDL2) gera `mgd_launcher.exe` no CI.
- Exes Windows saem no GitHub Actions (`mgd-exe-windows`).

## Integração com emuladores

O diretório `core/bridge/` define o contrato `Eden -> MGD` (`HandoffFrame` com câmera, regiões e polígonos visíveis). O jogo simula e mostra as UIs; o MGD faz o 3D. Detalhes em `docs/`.

## Licença

MIT License - veja o arquivo LICENSE para detalhes.
