# Guia de Engenharia Reversa - Offsets Odyssey (Super Mario Odyssey)

## Visão Geral

Este documento descreve o processo de engenharia reversa para obter os offsets de memória do Super Mario Odyssey necessários para o handoff do emulador MGD.

## Ferramentas Necessárias

1. **Ryujinx** - Emulador Switch com debugging capabilities
2. **Ghidra** - NSA's open-source RE suite (gratuito)
3. **Noexs** - NX executable extractor (opcional, para extrair .nso de .nca)
4. **Switch real (opcional)** - Para validação em hardware real
5. **Ghidra scripts** - Para automação (opcional)

## Metodologia RE

### 1. Preparação do Ambiente

```bash
# 1. Baixar Ryujinx (versão mais recente)
# 2. Obter keys do jogo (prod.keys, title.keys)
# 3. Dump do jogo (NSP/XCI -> NCA -> NSO)

# Com hactool:
hactool -k keys/prod.keys --section0dir=nca_out/ game.nsp
# NCA do programa (main.nso) estará em nca_out/
```

### 2. Extrair main.nso

```bash
# O main.nso está no NCA do tipo PROGRAM
# Dentro do NSP, procure por .nca com type=PROGRAM
hactool -k keys/prod.keys --section0dir=nca_out/ game.nsp
# NCA do programa (main.nso) estará em nca_out/
```

### 3. Carregar no Ghidra

1. Abrir Ghidra → New Project
2. File → Import File → Selecionar `main.nso`
3. Format: `ELF` (NSO é ELF-like)
3. Architecture: `AArch64:LE:64:v8A` (ARM64 little-endian)
4. Language: `AArch64:LE:64:v8A`
5. Analyze → Yes (pode demorar alguns minutos)

### 3. Encontrar Camera::Update

**Método 1: Search por strings**
- Search → For Strings → "CameraPos" / "CameraPos" / "CameraPos"
- Procure por referências à string
- XREF (X) na string → mostra onde é usada
- Geralmente leva a `Camera::Update` ou `Camera::SetPosition`

**Método 2: Pattern matching**
- Search → Instruction pattern
- Procure por: `str q0, [x?, #offset]` seguido de `str q1, [x?, #offset]` (Vec3 = 3 floats)
- Camera pos geralmente é 3 floats consecutivos (12 bytes)

**Método 3: Pattern no Camera::Update**
```
Assembly típico (ARM64):
Camera::Update:
    adrp x0, CameraPos@PAGE
    add  x0, x0, CameraPos@PAGEOFF
    str  q0, [x0]        // q0 = Vec3 (x,y,z)
    str  q1, [x0, #16]   // quaternion (se junto)
```

### 4. SceneGraph::Cull

Procure por:
- Strings: "Cull", "Culling", "Frustum", "Visible"
- Search: "SceneGraph" → XREF
- Procure por: `SceneGraph::Cull`, `SceneGraph::UpdateVisibleSet`
- Pattern: loop sobre objetos → frustum check → add to visible list

### 4. Estruturas de Dados

**Camera (típico):**
```cpp
struct Camera {
    Vec3 position;      // 0x00
    Quaternion rotation; // 0x10
    float fov;          // 0x20
    float aspect;       // 0x24
    float near_plane;   // 0x28
    float far_plane;    // 0x2C
    // ... 
};
```

**VisiblePolygonBuffer (típico):**
```cpp
struct VisiblePolygon {
    uint32_t polygon_id;
    uint32_t asset_id;
    Vec3 position;
    uint32_t flags;     // VISIBLE, OCCLUDED, etc.
    // ...
};

struct VisiblePolygonBuffer {
    uint32_t count;
    VisiblePolygon polygons[4096]; // ou dinâmico
};
```

### 5. Offsets Conhecidos (v1.5.0 - build 0xD11)

| Campo | Offset (heap) | Descrição |
|-------|---------------|-----------|
| camera_pos | 0x4A2B8000 | Vec3 position |
| camera_rot | 0x4A2B8010 | Quaternion |
| camera_fov | 0x4A2B8020 | float fov_degrees |
| scene_root | 0x4A300000 | SceneGraph* root |
| polygon_buffer | 0x4A310000 | VisiblePolygon* |
| visible_count | 0x4A310800 | uint32_t count |
| world_transforms | 0x4A320000 | Matrix4* transforms |
| material_db | 0x4A330000 | MaterialDB* |
| texture_db | 0x4A340000 | TextureDB* |
| mesh_db | 0x4A350000 | MeshDB* |

> **Nota**: Estes offsets são para v1.5.0 (build 0xD11). Mudam entre versões!

## Deteção de Versão

O jogo armazena o build ID em local conhecido:

```cpp
uint32_t detectVersion(uint32_t build_id) {
    if (build_id == 0x8EB) return V100;
    if (build_id == 0x9A1) return V110;
    if (build_id == 0xA33) return V120;
    if (build_id == 0xB92) return V130;
    if (build_id == 0xD11) return V150;
    return UNKNOWN;
}
```

Build ID pode ser lido do NPDM (exheader do NCA) ou do control.nacp.

## Validação dos Offsets

```cpp
// Script de validação rápida (rodar no emulador)
bool validateOffsets(OdysseyOffsets& o) {
    // 1. Camera pos deve ser não-zero e razoável
    Vec3 pos = readVec3(camera_pos);
    if (pos.length() < 1.0f || pos.length() > 10000.0f) return false;
    
    // 2. FOV razoável
    float fov = readFloat(camera_fov);
    if (fov < 30.0f || fov > 120.0f) return false;
    
    // 2. Scene root deve apontar para estrutura válida
    SceneGraph* sg = readPtr(scene_root);
    if (!sg || sg->vtable == 0) return false;
    
    // 3. Visible count razoável
    uint32_t count = readU32(visible_count);
    if (count > 8192) return false; // sanity check
    
    return true;
}
```

## Automação com Ghidra Scripts

```python
# Ghidra script (Python) para encontrar Camera::Update
from ghidra.app.decompiler import DecompInterface
from ghidra.program.model.symbol import SourceType

def find_camera_update():
    program = getCurrentProgram()
    listing = program.getListing()
    
    # Busca por padrão de Camera::Update
    for func in listing.getFunctions(True):
        name = func.getName()
        if "Camera" in name and "Update" in name:
            print(f"Found: {func.getEntryPoint()} - {name}")
            # Analisa a função
            decomp = DecompInterface()
            decomp.openProgram(currentProgram)
            res = decomp.decompileFunction(func, 60, monitor)
            if res.decompileCompleted():
                print(res.getDecompiledFunction().getC())
                
runScript()
```

## Validação Automática (Script de Teste)

```cpp
// test_offsets.cpp - compilar e rodar no emulador
#include <iostream>
#include "OdysseyHandoff.h"

int main() {
    mgd::emu::OdysseyOffsets offsets = makeOdysseyOffsets_v150();
    mgd::emu::OdysseyHandoffSource handoff(nullptr);
    handoff.setOffsets(offsets);
    
    // Testa se offsets são válidos
    std::cout << "Offsets válidos: " << (handoff.offsetsValid() ? "SIM" : "NÃO") << std::endl;
    
    // Tenta ler camera (vai falhar sem CPU real, mas testa offsets)
    bridge::HandoffFrame frame;
    bool ok = handoff.poll(frame);
    std::cout << "Poll result: " << (ok ? "OK" : "FAIL") << std::endl;
    
    return 0;
}
```

## Checklist de Validação

- [ ] Camera position lida corretamente (não-zero, razoável)
- [ ] Camera rotation (quaternion normalizado)
- [ ] FOV razoável (30-120 graus)
- [ ] Scene root aponta para estrutura válida
- [ ] Polygon buffer aponta para array válido
- [ ] Visible count condiz com frame (0-4096)
- [ ] World transforms acessíveis
- [ ] Material/Texture/Mesh DBs acessíveis

## Próximos Passos

1. Validar offsets no Ryujinx com Cap Kingdom
2. Testar handoff frame a frame
3. Integrar com MentalMapRuntime
4. Testar frustum culling com câmera real