#pragma once

#include <cstdint>
#include <string>

namespace mgd {

using EntityID = uint32_t;
using ResourceID = uint32_t;
using MeshID = uint32_t;
using TextureID = uint32_t;
using MaterialID = uint32_t;
using CollisionID = uint32_t;
using RegionID = uint32_t;
using AnimationID = uint32_t;
using PolygonID = uint32_t;
using AssetID = ResourceID;

constexpr EntityID INVALID_ENTITY_ID = 0;
constexpr MeshID INVALID_MESH_ID = 0;
constexpr TextureID INVALID_TEXTURE_ID = 0;
constexpr MaterialID INVALID_MATERIAL_ID = 0;
constexpr CollisionID INVALID_COLLISION_ID = 0;
constexpr RegionID INVALID_REGION_ID = 0;
constexpr PolygonID INVALID_POLYGON_ID = 0;
constexpr AssetID INVALID_ASSET_ID = 0;

enum class EntityState : uint8_t {
    ACTIVE = 0,
    INACTIVE = 1,
    HIDDEN = 2,
    DISABLED = 3,
    DIRTY = 4
};

enum class VisibilityState : uint8_t {
    UNCHECKED = 0,
    VISIBLE = 1,
    OCCLUDED = 2,
    FRUSTUM_CULLED = 3
};

enum class RegionLoadState : uint8_t {
    UNLOADED = 0,
    LOADING = 1,
    LOADED = 2,
    UNLOADING = 3
};

enum class ShapeType : uint8_t {
    AABB = 0,
    SPHERE = 1,
    OBB = 2,
    CAPSULE = 3
};

enum class ScanMode : uint8_t {
    SCAN_ONLY = 0,
    FULL_ANALYSIS = 1
};

enum class AlphaMode : uint8_t {
    OPAQUE = 0,
    MASK = 1,
    BLEND = 2
};

enum class TextureFilter : uint8_t {
    NEAREST = 0,
    BILINEAR = 1
};

enum class WrapMode : uint8_t {
    REPEAT = 0,
    CLAMP = 1
};

enum class LightType : uint8_t {
    AMBIENT = 0,
    DIRECTIONAL = 1,
    POINT = 2,
    SPOT = 3
};

enum class RecordType : uint8_t {
    ENTITY = 0,
    RESOURCE = 1,
    COLLISION = 2,
    WORLD = 3,
    MATERIAL = 4,
    TEXTURE = 5,
    MESH = 6
};

enum class DebugMode : uint8_t {
    NORMAL = 0,
    WIREFRAME = 1,
    DEPTH = 2,
    NORMALS = 3,
    UV = 4,
    BOUNDS = 5,
    COLLISION = 6,
    OVERDRAW = 7
};

} // namespace mgd
