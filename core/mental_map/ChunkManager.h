#pragma once

// ChunkManager - gerencia chunks do mental map (regiões do mundo)

#include <cstdint>
#include <cstddef>

namespace mgd {
namespace core {
namespace mental_map {

using RegionID = uint32_t;

class ChunkManager {
public:
    static constexpr uint32_t CHUNK_SIZE = 64; // metros
    static constexpr uint32_t REGION_SHIFT = 6; // 2^6 = 64
    static constexpr uint32_t REGION_MASK = 0xFFFF;

    // Converte posição do mundo para RegionID
    static RegionID worldToRegionId(float x, float z) {
        int32_t rx = static_cast<int32_t>(x) >> REGION_SHIFT;
        int32_t rz = static_cast<int32_t>(z) >> REGION_SHIFT;
        return static_cast<RegionID>((static_cast<uint32_t>(rx) & 0xFFFF) << 16 |
                                     (static_cast<uint32_t>(rz) & 0xFFFF));
    }

    // Converte Vec3 para RegionID
    struct Vec3 {
        float x, y, z;
        Vec3() : x(0), y(0), z(0) {}
        Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    };

    static RegionID worldToRegionId(const struct Vec3& pos) {
        return worldToRegionId(pos.x, pos.z);
    }

    // Obtém coordenadas da região
    static void regionIdToCoords(uint32_t region_id, int32_t& out_x, int32_t& out_z) {
        out_x = static_cast<int32_t>(region_id >> 16);
        out_z = static_cast<int32_t>(region_id & 0xFFFF);
    }

    // Verifica se região é válida
    static bool isValidRegion(RegionID region_id) {
        return region_id != 0xFFFFFFFF;
    }

    // Obtém vizinhos de uma região (8 vizinhos)
    static void getNeighbors(RegionID region_id, uint32_t* out_neighbors, size_t& out_count) {
        int32_t rx, rz;
        regionIdToCoords(region_id, rx, rz);
        out_count = 0;
        for (int dz = -1; dz <= 1; ++dz) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dz == 0) continue;
                int32_t nrx = rx + dx;
                int32_t nrz = rz + dz;
                uint32_t n = (static_cast<uint32_t>(nrx) & 0xFFFF) << 16 |
                             (static_cast<uint32_t>(nrz) & 0xFFFF);
                out_neighbors[out_count++] = n;
            }
        }
    }
};

// Forward declaration para MentalMapRuntime
class MentalMapRuntime;

} // namespace mental_map
} // namespace core
} // namespace mgd