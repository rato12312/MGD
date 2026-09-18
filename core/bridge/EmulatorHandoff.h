#pragma once

// Bridge interfaces para handoff entre emulador e sistemas de renderização

#include <cstdint>
#include <vector>
#include <optional>

namespace mgd {
namespace bridge {

// Tipos básicos
struct Vec3 {
    float x, y, z;
    Vec3() : x(0), y(0), z(0) {}
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
};

struct Vec2 {
    float x, y;
    Vec2() : x(0), y(0) {}
    Vec2(float x_, float y_) : x(x_), y(y_) {}
};

struct Quat {
    float x, y, z, w;
    Quat() : x(0), y(0), z(0), w(1) {}
    Quat(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
};

// Frame de handoff completo
struct HandoffFrame {
    uint64_t frame_index = 0;
    struct HandoffCamera {
        Vec3 position;
        Vec3 forward;
        float fov_degrees = 60.0f;
        float aspect = 16.0f / 9.0f;
    } camera;
    std::vector<uint32_t> visible_regions;
    std::vector<uint32_t> visible_polygons;
    bool ui_visible = false;
};

struct HandoffCamera {
    Vec3 position;
    Vec3 forward;
    float fov_degrees = 60.0f;
    float aspect = 16.0f / 9.0f;
};

// Interface base para sources de handoff
class IHandoffSource {
public:
    virtual ~IHandoffSource() = default;
    virtual bool poll(HandoffFrame& out) = 0;
};

} // namespace bridge
} // namespace mgd