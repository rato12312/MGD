#pragma once

#include "../common/Vec3.h"
#include "../common/Types.h"
#include <cstdint>
#include <vector>

namespace mgd {
namespace bridge {

// Handoff Eden/Skyline -> MGD (só contrato, sem comportamento).
// O jogo simula e mostra as UIs; o MGD faz o 3D.
// Quando o Odyssey abrir, a captura preenche HandoffFrame e o pipeline
// existente (DNA, cache, incremental) assume a apresentação.

// Câmera entregue pelo emulador no momento do handoff.
struct HandoffCamera {
    Vec3 position{};
    Vec3 forward{0.0f, 0.0f, 1.0f};
    float fov_degrees = 60.0f;
    float aspect = 16.0f / 9.0f;
};

// Um frame entregue: quais regiões/polígonos o jogo diz que importam.
// Só IDs e posições — sem copiar assets (filosofia do MGD).
struct HandoffFrame {
    uint64_t frame_index = 0;
    HandoffCamera camera;
    std::vector<RegionID> visible_regions;
    std::vector<PolygonID> visible_polygons;
    bool ui_visible = false; // jogo mostra UI por cima do 3D do MGD
};

// Fonte de handoff. TODO: implementação Eden (ler estado via hook do renderer).
class IHandoffSource {
public:
    virtual ~IHandoffSource() = default;
    // Retorna false enquanto o jogo não passou do START (sem estado).
    virtual bool poll(HandoffFrame& out) = 0;
};

} // namespace bridge
} // namespace mgd
