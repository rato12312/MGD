# MGD Engine - Mental Graphics Driver

An alternative rendering architecture that maintains a logical world representation ("Mental Map") and uses a "Painter" to produce pixels directly, bypassing traditional 3D pipelines.

## Architecture

```
Mental Map
    ↓
Camera
    ↓
Visibility/State
    ↓
Painter
    ↓
Framebuffer (RGBA)
    ↓
SDL2
    ↓
Screen
```

## Core Concepts

### Mental Map
A logical representation of the world containing entities with:
- Unique ID
- 3D Position (X, Y, Z)
- State (Active, Inactive, Hidden)
- Visual Reference (Background, Ground, Cube, Sphere, Player, Custom)
- Visibility flag
- Full RGBA Color (0-255 per channel)

### Camera
2D orthographic camera with:
- Position and target
- Zoom and rotation
- Viewport dimensions
- World↔Screen coordinate conversion
- View frustum culling

### Painter
Converts Mental Map entities to pixels:
- Full RGBA framebuffer (no palette limitation)
- Background and ground rendering
- Entity rendering by visual reference (cube, sphere, player)
- Alpha blending support
- Depth-based scaling

### Renderer
SDL2-based window and presentation:
- Hardware-accelerated texture streaming
- VSync support
- Configurable FPS target
- Callback-based update/render/input

## Project Structure

```
MGD/
├── core/                  # C++ Core
│   ├── mental_map/        # Entity, MentalMap
│   ├── camera/            # Camera
│   ├── painter/           # Painter, Framebuffer
│   └── renderer/          # Renderer (SDL2)
├── interface/             # Python interface
├── tests/                 # Unit tests
├── src/                   # Main executable
├── assets/                # Game assets (future)
├── cache/                 # Runtime cache (future)
├── docs/                  # Documentation (future)
├── tools/                 # Development tools (future)
├── CMakeLists.txt
└── README.md
```

## Dependencies

- **C++17** compatible compiler (MSVC, GCC, Clang)
- **CMake** 3.16+
- **SDL2** development libraries

### Installing SDL2

**Windows (vcpkg):**
```powershell
vcpkg install sdl2:x64-windows
```

**Windows (manual):**
1. Download SDL2 development libraries from https://github.com/libsdl-org/SDL/releases
2. Extract to `C:\SDL2`
3. Add `C:\SDL2\lib` to library path, `C:\SDL2\include` to include path

**Linux (Ubuntu/Debian):**
```bash
sudo apt-get install libsdl2-dev
```

**Linux (Fedora):**
```bash
sudo dnf install SDL2-devel
```

**macOS (Homebrew):**
```bash
brew install sdl2
```

## Building

```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### Build Options

- `-DCMAKE_BUILD_TYPE=Release` (default) - Optimized build
- `-DCMAKE_BUILD_TYPE=Debug` - Debug symbols, no optimization
- `-DCMAKE_INSTALL_PREFIX=<path>` - Custom install location

## Running

```bash
# From build directory
./mgd_prototype

# Or using Python launcher
python interface/launch_mgd.py
```

## Controls

| Key | Action |
|-----|--------|
| WASD / Arrow Keys | Move player entity |
| Q / E | Move player up/down (Z axis) |
| + / - | Zoom camera in/out |
| R / F | Rotate camera |
| ESC | Quit |

## Test Scene

The prototype includes a test scene with:
- **Background** - Dark blue sky
- **Ground** - Gradient floor with grid lines
- **5 Cubes** - Blue cubes at different X positions
- **3 Spheres** - Red/orange spheres at different positions
- **Player** - Yellow square entity (controllable)

## Running Tests

```bash
# From build directory
ctest --output-on-failure

# Or run test executable directly
./mgd_tests
```

## Python Interface

```python
from interface.mgd_interface import MGDInterface

mgd = MGDInterface()
mgd.launch(width=800, height=600)

# Monitor state (requires IPC implementation)
state = mgd.get_state()
print(f"Entities: {len(state.entities)}")
print(f"FPS: {state.fps}")

mgd.shutdown()
```

## Design Decisions

1. **No traditional 3D pipeline** - No vertex shaders, no triangle rasterization. The Painter draws directly to RGBA pixels.

2. **Full RGBA color** - Each channel 0-255, no palette limitations. Colors stored directly in entities.

3. **Modular C++ core** - Clean separation of MentalMap, Camera, Painter, Renderer for future game-specific adapters.

4. **Python for control** - High-level logic, scripting, and tooling in Python; performance-critical rendering in C++.

5. **Entity-component approach** - Entities are simple data containers; behavior added via systems (future).

6. **Immediate mode painting** - Painter processes visible entities each frame; no retained display lists.

## Future Work

- [ ] Skyrim integration adapter
- [ ] Spatial partitioning (quadtree/octree) for MentalMap
- [ ] Asynchronous asset loading
- [ ] Python IPC for real-time state monitoring
- [ ] Shader-based Painter backend (optional)
- [ ] Level-of-detail system
- [ ] Entity serialization/save system

## License

MIT License - See LICENSE file for details.