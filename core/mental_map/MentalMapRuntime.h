#pragma once

// MentalMapRuntime - runtime do mental map para previsão de regiões

#include <cstdint>
#include <vector>
#include <memory>

namespace mgd {
namespace core {
namespace mental_map {

class ChunkManager;

class MentalMapRuntime {
public:
    MentalMapRuntime() = default;
    ~MentalMapRuntime() = default;

    // Inicializa o runtime
    bool init() { return true; }

    // Atualiza estado do mental map
    void update(float dt) {}

    // Obtém chunk manager
    ChunkManager* getChunkManager() { return nullptr; }

    // Previsão de regiões visíveis
    void predictVisibleRegions(float x, float z, uint32_t* out_regions, size_t& out_count) {}
};

} // namespace mental_map
} // namespace core
} // namespace mgd