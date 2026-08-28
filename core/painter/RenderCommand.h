#pragma once

#include <cstdint>

namespace mgd {

enum class RenderCommandType : uint8_t {
    DrawMesh,
    DrawSky,
    DrawBounds,
    DrawDebugLine
};

struct RenderCommand {
    RenderCommandType type;
    uint32_t entity_index;
};

} // namespace mgd
