#pragma once

#include "../common/Vec3.h"
#include "../common/Types.h"
#include <cstdint>
#include <cmath>

namespace mgd {

// Chunk = Minecraft-like spatial partition (4096x4096 Skyrim cell).
// Cada chunk guarda seus assets; chunks vizinhos conectam.
// Quanto maior o jogo (LE + 3 DLCs), mais chunks. Base para puxar
// informações rápido — cache por IDs distribuído em chunks.

struct ChunkCoord {
    int32_t x = 0;
    int32_t z = 0;
    bool operator==(const ChunkCoord& o) const { return x==o.x && z==o.z; }
};

struct ChunkCoordHash {
    size_t operator()(ChunkCoord const& c) const noexcept {
        return (static_cast<size_t>(c.x) * 73856093u) ^ (static_cast<size_t>(c.z) * 19349663u);
    }
};

class ChunkManager {
public:
    static constexpr float CHUNK_SIZE = 4096.0f; // Skyrim exterior cell

    static ChunkCoord worldToChunk(const Vec3& pos) {
        int32_t cx = static_cast<int32_t>(std::floor(pos.x / CHUNK_SIZE));
        int32_t cz = static_cast<int32_t>(std::floor(pos.z / CHUNK_SIZE));
        return {cx, cz};
    }

    static RegionID chunkToRegionId(const ChunkCoord& c) {
        // Pack x,z into 32-bit RegionID (16 bits each, bias 32768)
        uint32_t ux = static_cast<uint32_t>(c.x + 32768);
        uint32_t uz = static_cast<uint32_t>(c.z + 32768);
        return static_cast<RegionID>((ux << 16) | (uz & 0xFFFF));
    }

    static RegionID worldToRegionId(const Vec3& pos) {
        return chunkToRegionId(worldToChunk(pos));
    }

    static Vec3 chunkCenter(const ChunkCoord& c) {
        return Vec3{(c.x + 0.5f)*CHUNK_SIZE, 0.0f, (c.z + 0.5f)*CHUNK_SIZE};
    }
};

} // namespace mgd
