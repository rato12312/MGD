# MGD Odyssey — Mario Odyssey Emulator for Android

> **MGD (Mario Galaxy Dream) Odyssey** — A research-focused Nintendo Switch emulator targeting *Super Mario Odyssey* on Android, built from scratch with a focus on modern rendering techniques and reverse-engineered game internals.

---

## 🎯 Project Vision

This is **not** a commercial emulator — it's a research project exploring:
- Modern Vulkan-based rendering on mobile (Android 7.0+)
- Reverse-engineered Nintendo Switch GPU (Maxwell) command buffer execution
- FSR 2.x temporal upscaling + TAA + RCAS on mobile Vulkan
- Framebuffer optimization (MFO) for bandwidth-constrained mobile GPUs
- Reverse-engineered Odyssey engine internals (camera, scene graph, culling)

**Target:** Android 7.0+ (API 24+) with Vulkan 1.1+ support

---

## 🏗 Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                        MGD Odyssey                                │
├─────────────────────────────────────────────────────────────────┤
│  Runtime                                                         │
│  ├── MarioOdysseyRunner  — High-level game loop & quality mgmt │
│  └── Emulator          — CPU + Kernel + HOS services + World    │
├─────────────────────────────────────────────────────────────────┤
│  Core Systems                                                     │
│  ├── CPU (ARM64)     — Interpreter + NEON + SVC/HOS             │
│  ├── Memory            — Guest RAM + MMU + Guest page tables    │
│  ├── HOS Services      — 15 services (nv, vi, hid, fs, etc.)    │
│  ├── Loader            — NSP → NCA → ExeFS → main.nso           │
│  └── Crypto            — AES-XTS/CTR, SHA-256, Keys (user-supplied)│
├─────────────────────────────────────────────────────────────────┤
│  Graphics (Vulkan)                                               │
│  ├── Shader Recompiler  — Maxwell ISA → SPIR-V                  │
│  ├── Intelligent Painter — FSR 2.x + TAA + RCAS + MFO           │
│  ├── Framebuffer Optimizer (MFO) — Tile cache + dirty regions  │
│  ├── Seed Predictor — Pre-warm shaders from game state         │
│  └── Framebuffer Manager — History + motion vectors            │
├─────────────────────────────────────────────────────────────────┤
│  Odyssey Integration                                             │
│  ├── RE Offsets       — Camera, SceneGraph, Culling (per build) │
│  ├── OdysseyHandoff   — Camera → Mental Map → Visible polys     │
│  ├── OdysseyWorld     — Cheap mode + Mental Map bootstrap       │
│  └── RE Offset Extractor — RAM dump → Ghidra/Ryujinx automation │
└─────────────────────────────────────────────────────────────────┘
```

---

## ✨ Features Implemented

| System | Status | Details |
|--------|--------|---------|
| **CPU (ARM64)** | ✅ ~98% | Interpreter + NEON FP/Int + SVC + Exceptions |
| **HOS Services** | ✅ 15/15 | nv, vi, hid, fs, aud, pm, acc, pm, set, lbl, fatal, time, applet |
| **Loader (NSP→NCA→ExeFS)** | ✅ | PFS0, NCA (XTS/CTR), ExeFS, RomFS, NSO |
| **Crypto** | ✅ | AES-XTS/CTR (NEON), SHA-256, Key derivation |
| **Vulkan Backend** | ✅ | Command buffer parse + submit + resource creation |
| **Shader Recompiler** | 🚧 | Maxwell ISA → SPIR-V (FSR2/TAA/RCAS stubs) |
| **Intelligent Painter** | 🚧 | FSR2 (EASU+RCAS) + TAA + RCAS + MFO + Seed Predictor |
| **Framebuffer Optimizer (MFO)** | ✅ | Tile cache, dirty regions, frame reuse |
| **Odyssey RE Offsets** | 🔬 | Camera/SceneGraph/Culling per build (1.0.0 → 1.5.0) |
| **OdysseyHandoff** | ✅ | Camera → Mental Map → Visible polygons |
| **Android APK** | ✅ | Vulkan + JNI + Settings UI (Preset/FSR/TAA/RCAS/MFO/Sharpness) |
| **CI/CD** | ✅ | GitHub Actions: Ubuntu + Windows + Android (NDK) |

---

## 📁 Project Structure

```
mgd/
├── emulador-mgd/              # Core emulator (C++)
│   ├── cpu/                   # ARM64 interpreter + NEON + SVC
│   ├── ram/                   # Guest RAM + MMU
│   ├── hos/                   # HOS Kernel + 15 Services
│   ├── loader/                # NSP/NCA/NSO/ExeFS/RomFS + Crypto
│   ├── gpu/                   # Vulkan backend + IntelligentPainter
│   ├── core/gpu/              # Shared GPU: FSR2, MFO, Painter, etc.
│   ├── odyssey/               # Odyssey-specific: Handoff, World, RE
│   ├── loader/                # NSP/NCA/NSO/ExeFS/RomFS/Crypto
│   ├── runtime/               # MarioOdysseyRunner (Android)
│   ├── cpu/                   # ARM64 CPU interpreter
│   ├── ram/                   # Guest RAM + MMU
│   ├── hod/                   # HOS Kernel + 15 services
│   ├── loader/                # NSP/NCA/NSO/ExeFS/RomFS/Crypto
│   ├── odyssey/               # Odyssey RE + Handoff
│   ├── re/                    # Reverse engineering tools
│   └── runtime/               # MarioOdysseyRunner (Android)
├── core/                      # Shared GPU (FSR2, MFO, Painter)
├── android/                   # Android APK (Kotlin + JNI + JNI)
│   ├── app/
│   │   ├── src/main/
│   │   │   ├── java/com/mgd/odyssey/
│   │   │   │   ├── MainActivity.kt
│   │   │   │   ├── SettingsActivity.kt
│   │   │   │   └── ...
│   │   │   ├── res/ (layouts, values, drawables)
│   │   │   └── cpp/ (JNI bridge)
├── core/                      # Shared GPU (FSR2, MFO, Painter, Seed)
├── tests/                     # Unit + Integration tests (Catch2)
├── tools/                     # Benchmark, NSP tool
├── docs/                      # Documentation
├── CMakeLists.txt
├── build.sh                   # Linux/macOS build script
├── CMakeLists.txt
└── README.md                  ← You are here
```

---

## 🛠 Building

### Prerequisites
- **Linux/macOS:** `g++ ≥ 11`, `cmake ≥ 3.16`, `vulkan-sdk`, `sdl2-dev`
- **Windows:** `g++ (MinGW) ≥ 11`, `cmake ≥ 3.16`, `Vulkan SDK`, `SDL2`
- **Android:** NDK r26+, Gradle 8.1+, JDK 17, Android SDK 34

### Desktop (Linux/macOS/Windows)
```bash
git clone https://github.com/<your-org>/mgd-odyssey.git
cd mgd-odyssey

# Desktop build (tests + emulator core)
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release -j$(nproc)

# Run tests
./build/mgd_tests

# Headless demo (outputs output.ppm)
./build/mgd_app
```

### Android APK
```bash
cd android
./gradlew assembleDebug    # Debug APK
./gradlew assembleRelease  # Release APK (requires signing)
```

### CI/CD (GitHub Actions)
- **Ubuntu-latest:** Full build + tests + `mgd_app` binary + `output.ppm`
- **Windows-latest (MSYS2):** `.exe` artifacts
- **Android (ubuntu-latest + NDK):** `app-debug.apk` artifact

---

## 🎮 Running Mario Odyssey

> **⚠️ Legal Notice:** You must own a legitimate copy of *Super Mario Odyssey*. This emulator does not include keys, firmware, or game data.

### Required Files (User-Provided)
```
keys/
├── prod.keys          # Master keys (from your Switch)
├── title.keys         # Title keys (from your Switch)
└── prod.keys          # (optional) title.keys for specific games

game.nsp               # Your legally dumped Mario Odyssey NSP
```

### Running
```bash
# Android (on device)
# 1. Install mgd-odyssey.apk
# 2. Place keys in /sdcard/MGD/keys/
# 3. Place game.nsp in /sdcard/MGD/game.nsp
# 4. Launch app → Load Game → Enjoy

# Desktop (headless demo)
./build/mgd_app

# Run tests
./build/mgd_tests
```

---

## 🔑 Keys & Legal

> **MGD does NOT distribute keys.** You must extract `prod.keys` and `title.keys` from your own Nintendo Switch using homebrew tools (e.g., `Lockpick_RCM`).

```
keys/
├── master_key_00 ... master_key_16   # Master keys (per firmware)
├── title_keys                        # Title keys (per game)
└── header_key                        # NCA header key
```

---

## 🧪 Testing

```bash
# Unit + Integration tests
./build/mgd_tests

# Specific test suites
./build/mgd_tests --filter="test_nca_decrypt"
./build/mgd_tests --filter="test_ipc_buffers"
./build/mgd_tests --filter="test_applet_service"

# Benchmark
./build/benchmark_polygon
```

---

## 🔬 Reverse Engineering Resources

| Document | Description |
|----------|-------------|
| `emulador-mgd/PROGRESSO.md` | 5-stage progress tracker (100% = done) |
| `emulador-mgd/ISA.md` | ARM64 + NEON instruction coverage |
| `emulador-mgd/RE_GUIDE.md` | How to extract offsets via Ryujinx/Ghidra |
| `emulador-mgd/re/OdysseyOffsetExtractor.h` | RAM dump → offsets automation |
| `emulador-mgd/re/RE_GUIDE.md` | Step-by-step RE workflow |

---

## 🤝 Contributing

1. Fork → Feature branch → PR
2. Follow existing code style (C++17, clang-format)
3. Tests must pass (`./build/mgd_tests`)
4. No binary blobs, no keys, no game assets

---

## 📜 License

**MIT License** — See `LICENSE` file.

> This project is for **educational and research purposes only**.  
> Not affiliated with Nintendo. *Super Mario Odyssey* © Nintendo.

---

## 🙏 Acknowledgments

- **Ryujinx / yuzu** — Reference implementations & RE research
- **switchbrew** — Hardware/OS documentation
- **hactool / hactoolnet** — NCA/NSO format reference
- **AMD FSR 2.x** — Open-source upscaling reference
- **Vulkan-Hpp / Vulkan-Hpp** — C++ Vulkan bindings

---

## 📞 Contact

- **Issues:** GitHub Issues (bug reports, feature requests)
- **Discussions:** GitHub Discussions (architecture, RE findings)
- **Security:** See `SECURITY.md` (if exists)

---

> *“It's-a me, MGD!”* — Building the future of Switch emulation, one frame at a time.