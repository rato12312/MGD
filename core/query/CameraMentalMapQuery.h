#pragma once

// CameraMentalMapQuery - query de câmera para mental map

#include <cstdint>
#include <vector>

namespace mgd {
namespace core {
namespace query {

struct Vec3 {
    float x, y, z;
    Vec3() : x(0), y(0), z(0) {}
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
};

class CameraMentalMapQuery {
public:
    CameraMentalMapQuery() = default;
    ~CameraMentalMapQuery() = default;

    // Atualiza posição da câmera
    void updateCamera(float x, float y, float z, float yaw, float pitch, float fov) {}

    // Obtém regiões visíveis
    void getVisibleRegions(uint32_t* out_regions, size_t& out_count) {}

    // Obtém frustum
    struct Frustum {
        float planes[6][4];
    };
    Frustum getFrustum() const { return {}; }
};

} // namespace query
} // namespace core
} // namespace mgd