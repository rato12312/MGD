// Odyssey RE Offset Extractor - Camera::Update & SceneGraph::Cull
// Practical tool for extracting real offsets from main.nso via Ryujinx/Ghidra

#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <iomanip>

namespace mgd {
namespace re {

// Odyssey Version mapping
enum class OdysseyVersion : uint32_t {
    V100 = 0x8EB,
    V110 = 0x9A1,
    V120 = 0xA33,
    V130 = 0xB92,
    V150 = 0xD11,
    UNKNOWN = 0xFFFFFFFF
};

// Known structures for Odyssey (reverse engineered)
struct CameraState {
    float position[3];      // 0x00
    float rotation[4];      // 0x10 - quaternion
    float fov_y;            // 0x20
    float aspect_ratio;     // 0x24
    float near_plane;       // 0x28
    float far_plane;        // 0x2C
    uint32_t view_mode;     // 0x30
    uint32_t camera_type;   // 0x34
    float ortho_scale;      // 0x38
    uint8_t reserved[56];   // padding
};

struct SceneGraphNode {
    uint64_t node_id;
    uint64_t parent_id;
    float transform[16];    // 4x4 matrix
    uint32_t mesh_id;
    uint32_t material_id;
    uint32_t collision_id;
    uint32_t flags;
    float bounds_min[3];
    float bounds_max[3];
    uint32_t lod_level;
    uint32_t visibility_mask;
    uint64_t user_data;
    uint8_t reserved[32];
};

struct SceneGraph {
    uint32_t node_count;
    uint32_t flags;
    uint64_t root_node_ptr;
    uint32_t visible_count;
    uint32_t frame_index;
    float frustum_planes[6][4];
    uint8_t reserved[64];
};

struct CullingSystem {
    uint32_t max_visible_nodes;
    uint32_t culling_flags;
    float lod_distances[4];
    uint64_t hi_z_pyramid_ptr;
    uint8_t reserved[48];
};

struct CameraOffsets {
    uint64_t camera_system_ptr;
    uint64_t active_camera_ptr;
    uint32_t pos_offset;
    uint32_t rot_offset;
    uint32_t fov_offset;
    uint32_t aspect_offset;
    uint32_t near_plane_offset;
    uint32_t far_plane_offset;
    uint32_t view_mode_offset;
    uint32_t camera_type_offset;
    uint32_t ortho_scale_offset;
    uint64_t scene_graph_ptr;
    uint32_t node_count_offset;
    uint64_t root_node_offset;
    uint32_t visible_count_offset;
    uint32_t frustum_planes_offset;
    uint32_t culling_system_offset;
    uint32_t max_visible_nodes;
    uint32_t culling_flags_offset;
    uint32_t lod_distances_offset;
    bool validated = false;
};

struct SceneGraphOffsets {
    uint64_t scene_graph_ptr;
    uint32_t node_count_offset;
    uint64_t root_node_ptr;
    uint32_t visible_count_offset;
    uint32_t frustum_planes_offset;
};

struct CullingOffsets {
    uint32_t culling_system_offset;
    uint32_t max_visible_nodes;
    uint32_t culling_flags_offset;
    uint32_t lod_distances_offset;
};

struct OdysseyOffsets {
    enum class OdysseyVersion : uint32_t {
        V100 = 0x8EB,
        V110 = 0x9A1,
        V120 = 0xA33,
        V130 = 0xB92,
        V150 = 0xD11,
        UNKNOWN = 0xFFFFFFFF
    } version;

    struct CameraOffsets {
        uint64_t camera_system_ptr;
        uint64_t active_camera_ptr;
        uint32_t pos_offset;
        uint32_t rot_offset;
        uint32_t fov_offset;
        uint32_t aspect_offset;
        uint32_t near_plane_offset;
        uint32_t far_plane_offset;
        uint32_t view_mode_offset;
        uint32_t camera_type_offset;
        uint32_t ortho_scale_offset;
        uint64_t scene_graph_ptr;
        uint32_t node_count_offset;
        uint64_t root_node_offset;
        uint32_t visible_count_offset;
        uint32_t frustum_planes_offset;
        uint32_t culling_system_offset;
        uint32_t max_visible_nodes;
        uint32_t culling_flags_offset;
        uint32_t lod_distances_offset;
        bool validated = false;
    } camera;

    struct SceneGraphOffsets {
        uint64_t scene_graph_ptr;
        uint32_t node_count_offset;
        uint64_t root_node_ptr;
        uint32_t visible_count_offset;
        uint32_t frustum_planes_offset;
    } scene_graph;

    struct CullingOffsets {
        uint32_t culling_system_offset;
        uint32_t max_visible_nodes;
        uint32_t culling_flags_offset;
        uint32_t lod_distances_offset;
    } culling;

    uint64_t scene_root = 0;
    uint64_t polygon_buffer = 0;
    uint64_t visible_count = 0;
    uint64_t world_transforms = 0;
    uint64_t material_db = 0;
    uint64_t texture_db = 0;
    uint64_t mesh_db = 0;
    uint32_t version_build = 0;
    bool validated = false;
};

class OdysseyOffsetExtractor {
public:
    OdysseyOffsetExtractor(const std::vector<uint8_t>& ram_dump);
    OdysseyOffsets extractOffsets();

    bool exportToHeader(const CameraOffsets& offsets, const std::string& output_path);
    bool exportToJson(const std::string& output_path);

private:
    const std::vector<uint8_t>& ram_;
    size_t ram_size_;

    static constexpr uint8_t CAMERA_SIG[] = {0x43, 0x61, 0x6D, 0x65, 0x72, 0x61};
    static constexpr uint8_t SCENE_SIG[] = {0x53, 0x63, 0x65, 0x6E, 0x65, 0x47, 0x72, 0x61, 0x70, 0x68};
    static constexpr uint8_t NOTE_SIG[] = {0x4E, 0x4F, 0x54, 0x45};

    static uint32_t read32(const uint8_t* ptr) { uint32_t v; std::memcpy(&v, ptr, 4); return v; }
    static uint64_t read64(const uint8_t* ptr) { uint64_t v; std::memcpy(&v, ptr, 8); return v; }
    static float readFloat(const uint8_t* ptr) { float v; std::memcpy(&v, ptr, 4); return v; }

    bool findCameraSystem(const uint8_t* ram, size_t size, uint64_t& out_ptr);
    bool findSceneGraph(uint64_t camera_system_ptr, uint64_t& out_ptr);
    void findCameraStateOffsets(uint64_t camera_ptr, CameraOffsets& out);
    void findSceneGraphFields(uint64_t scene_graph_ptr, CameraOffsets& out);
    void findCullingSystem(CameraOffsets& out);
    OdysseyVersion detectVersion(const std::vector<uint8_t>& ram);
    bool validateOffsets(const CameraOffsets& offsets);
    bool exportToHeader(const CameraOffsets& offsets, const std::string& output_path);
    bool exportToJson(const std::string& output_path);

private:
    const std::vector<uint8_t>& ram_;
    size_t ram_size_;

    static uint32_t read32(const uint8_t* ptr) { uint32_t v; std::memcpy(&v, ptr, 4); return v; }
    static uint64_t read64(const uint8_t* ptr) { uint64_t v; std::memcpy(&v, ptr, 8); return v; }
    static float readFloat(const uint8_t* ptr) { float v; std::memcpy(&v, ptr, 4); return v; }

    bool findCameraSystem(const uint8_t* ram, size_t size, uint64_t& out_ptr);
    bool findSceneGraph(uint64_t camera_system_ptr, uint64_t& out_ptr);
    void findCameraStateOffsets(uint64_t camera_ptr, CameraOffsets& out);
    void findSceneGraphFields(uint64_t scene_graph_ptr, CameraOffsets& out);
    void findCullingSystem(CameraOffsets& out);
    OdysseyVersion detectVersion(const std::vector<uint8_t>& ram);
    bool validateOffsets(const CameraOffsets& offsets);
    bool exportToHeader(const CameraOffsets& offsets, const std::string& output_path);
    bool exportToJson(const std::string& output_path);

private:
    const std::vector<uint8_t>& ram_;
    size_t ram_size_;
};

} // namespace re
} // namespace mgd