// OdysseyOffsetExtractor.cpp
// Implementation of RE Offset Extractor for Super Mario Odyssey

#include "OdysseyOffsetExtractor.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>
#include <iostream>

namespace mgd {
namespace re {

OdysseyOffsetExtractor::OdysseyOffsetExtractor(const std::vector<uint8_t>& ram_dump)
    : ram_(ram_dump), ram_size_(ram_dump.size()) {
}

bool OdysseyOffsetExtractor::extractOffsets(OdysseyOffsets& out) {
    // 1. Detect version
    out.version = detectVersion(ram_);
    
    // 2. Find CameraSystem
    uint64_t camera_system_ptr = 0;
    if (!findCameraSystem(ram_.data(), ram_size_, camera_system_ptr)) {
        std::cerr << "[WARN] CameraSystem not found in RAM dump\n";
        return false;
    }
    out.camera.camera_system_ptr = camera_system_ptr;
    
    // 3. Find active camera
    uint64_t active_camera_ptr = 0;
    if (!findActiveCamera(camera_system_ptr, active_camera_ptr)) {
        std::cerr << "[WARN] Active camera not found\n";
        return false;
    }
    out.camera.active_camera_ptr = active_camera_ptr;
    
    // 4. Find camera state offsets
    if (!findCameraStateOffsets(active_camera_ptr, out.camera)) {
        std::cerr << "[WARN] Camera state offsets not found\n";
        return false;
    }
    
    // 5. Find SceneGraph
    uint64_t scene_graph_ptr = 0;
    if (!findSceneGraph(camera_system_ptr, scene_graph_ptr)) {
        std::cerr << "[WARN] SceneGraph not found\n";
        return false;
    }
    out.camera.scene_graph_ptr = scene_graph_ptr;
    out.scene_graph.scene_graph_ptr = scene_graph_ptr;
    
    // 6. Find SceneGraph fields
    findSceneGraphFields(scene_graph_ptr, out.camera);
    out.scene_graph.node_count_offset = out.camera.node_count_offset;
    out.scene_graph.root_node_ptr = out.camera.root_node_offset;
    out.scene_graph.visible_count_offset = out.camera.visible_count_offset;
    out.scene_graph.frustum_planes_offset = out.camera.frustum_planes_offset;
    
    // 7. Find CullingSystem
    findCullingSystem(out.camera);
    out.culling.culling_system_offset = out.camera.culling_system_offset;
    out.culling.max_visible_nodes = out.camera.max_visible_nodes;
    out.culling.culling_flags_offset = out.camera.culling_flags_offset;
    out.culling.lod_distances_offset = out.camera.lod_distances_offset;
    
    // 8. Detect version
    out.version = detectVersion(ram_);
    out.version_build = static_cast<uint32_t>(out.version);
    
    // 9. Validate
    out.validated = validateOffsets(out.camera);
    
    // 9. Set some defaults for fields not found
    out.scene_root = scene_graph_ptr;
    out.polygon_buffer = 0; // Would need deeper RE
    out.visible_count = 0;
    out.world_transforms = 0;
    out.material_db = 0;
    out.texture_db = 0;
    out.mesh_db = 0;
    
    out.validated = validateOffsets(out.camera);
    return out.validated;
}

bool OdysseyOffsetExtractor::findCameraSystem(const uint8_t* ram, size_t size, uint64_t& out_ptr) {
    // Search for "Camera" string
    for (size_t i = 0; i + sizeof(CAMERA_SIG) <= size; i += 4) {
        if (std::memcmp(ram + i, CAMERA_SIG, sizeof(CAMERA_SIG)) == 0) {
            // Found "Camera" string, check if this is a CameraSystem vtable or object
            uint64_t candidate = i;
            if (isValidCameraSystem(candidate)) {
                out_ptr = candidate;
                return true;
            }
        }
    }
    
    // Fallback: search for CameraSystem vtable pattern
    // Look for virtual function table pattern
    static constexpr uint8_t VTABLE_SIG[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    
    return false;
}

bool OdysseyOffsetExtractor::findActiveCamera(uint64_t camera_system_ptr, uint64_t& out_ptr) {
    if (!camera_system_ptr || camera_system_ptr + 0x100 > ram_size_) return false;
    
    // CameraSystem usually has active_camera at offset 0x0, 0x8, or 0x10
    for (uint32_t off = 0; off < 0x30; off += 8) {
        if (camera_system_ptr + off + 8 > ram_size_) break;
        uint64_t candidate = read64(ram_.data() + camera_system_ptr + off);
        if (isValidCameraPtr(candidate)) {
            out_ptr = candidate;
            return true;
        }
    }
    return false;
}

bool OdysseyOffsetExtractor::findSceneGraph(uint64_t camera_system_ptr, uint64_t& out_ptr) {
    if (!camera_system_ptr || camera_system_ptr + 0x100 > ram_size_) return false;
    
    // SceneGraph usually at offset 0x10, 0x18, 0x20, 0x28 from CameraSystem
    for (uint32_t off = 0x10; off < 0x80; off += 8) {
        if (camera_system_ptr + off + 8 > ram_size_) break;
        uint64_t candidate = read64(ram_.data() + camera_system_ptr + off);
        if (isValidSceneGraphPtr(candidate)) {
            out_ptr = candidate;
            return true;
        }
    }
    return false;
}

bool OdysseyOffsetExtractor::findActiveCamera(uint64_t camera_system_ptr, uint64_t& out_ptr) {
    return findActiveCamera(camera_system_ptr, out_ptr);
}

void OdysseyOffsetExtractor::findCameraStateOffsets(uint64_t camera_ptr, CameraOffsets& out) {
    if (!camera_ptr || camera_ptr + 0x200 > ram_size_) return;
    
    // Search for FOV (float ~1.0) and aspect ratio (~1.77)
    for (uint32_t off = 0x1C; off < 0x100; off += 4) {
        if (camera_ptr + off + 4 > ram_size_) break;
        float fov = readFloat(ram_.data() + camera_ptr + off);
        if (fov > 0.5f && fov < 2.0f) {
            // Found potential FOV
            float aspect = readFloat(ram_.data() + camera_ptr + off + 4);
            if (aspect > 1.0f && aspect < 2.5f) {
                out.fov_offset = off;
                out.aspect_offset = off + 4;
                out.pos_offset = (off >= 0x20) ? off - 0x20 : 0; // position usually before FOV
                out.rot_offset = off + 0x10;
                out.near_plane_offset = off + 0x08;
                out.far_plane_offset = off + 0x0C;
                out.view_mode_offset = off + 0x14;
                out.camera_type_offset = off + 0x18;
                out.ortho_scale_offset = off + 0x1C;
                return;
            }
        }
    }
    
    // Fallback: common offsets for Switch games
    out.pos_offset = 0x00;
    out.rot_offset = 0x10;
    out.fov_offset = 0x20;
    out.aspect_offset = 0x24;
    out.near_plane_offset = 0x28;
    out.far_plane_offset = 0x2C;
    out.view_mode_offset = 0x30;
    out.camera_type_offset = 0x34;
    out.ortho_scale_offset = 0x38;
}

void OdysseyOffsetExtractor::findSceneGraphFields(uint64_t scene_graph_ptr, CameraOffsets& out) {
    if (!scene_graph_ptr || scene_graph_ptr + 0x200 > ram_size_) return;
    
    // SceneGraph layout (typical):
    // 0x00: node_count (u32)
    // 0x04: flags
    // 0x08: root_node_ptr
    // 0x10: visible_count
    // 0x14: frame_index
    // 0x20: frustum_planes[6][4] (6 vec4 = 96 bytes)
    
    uint32_t node_count = read32(ram_.data() + scene_graph_ptr);
    if (node_count > 0 && node_count < 100000) {
        out.node_count_offset = 0;
        out.root_node_offset = 0x8;
        out.visible_count_offset = 0x10;
        out.frustum_planes_offset = 0x20;
    }
}

void OdysseyOffsetExtractor::findCullingSystem(CameraOffsets& out) {
    // CullingSystem often at fixed offset from SceneGraph
    out.culling_system_offset = 0x40;
    out.max_visible_nodes = 0x1000; // 4096 typical
    out.culling_flags_offset = 0x04;
    out.lod_distances_offset = 0x20;
}

OdysseyVersion OdysseyOffsetExtractor::detectVersion(const std::vector<uint8_t>& ram) {
    // Search for build ID in .note.gnu.build-id section
    static constexpr uint8_t NOTE_SIG[] = {0x4E, 0x4F, 0x54, 0x45}; // "NOTE"
    
    for (size_t i = 0; i + 4 <= ram.size(); i += 4) {
        if (std::memcmp(ram.data() + i, NOTE_SIG, 4) == 0) {
            if (i + 32 <= ram.size()) {
                uint32_t build_id = read32(ram.data() + i + 4);
                switch (build_id) {
                    case 0x8EB: return OdysseyVersion::V100;
                    case 0x9A1: return OdysseyVersion::V110;
                    case 0xA33: return OdysseyVersion::V120;
                    case 0xB92: return OdysseyVersion::V130;
                    case 0xD11: return OdysseyVersion::V150;
                }
            }
        }
    }
    return OdysseyVersion::UNKNOWN;
}

bool OdysseyOffsetExtractor::validateOffsets(const CameraOffsets& offsets) {
    return offsets.camera_system_ptr != 0 && 
           offsets.active_camera_ptr != 0 &&
           offsets.fov_offset != 0 &&
           offsets.aspect_offset != 0 &&
           offsets.scene_graph_ptr != 0;
}

bool OdysseyOffsetExtractor::isValidCameraPtr(uint64_t ptr) const {
    if (ptr == 0) return false;
    if (ptr < 0x10000000 || ptr > 0xFFFFFFFF) return false; // Reasonable heap range
    if (ptr + 0x200 > ram_size_) return false;
    
    // Check if it looks like a CameraState
    float fov = readFloat(ram_.data() + ptr + 0x20);
    float aspect = readFloat(ram_.data() + ptr + 0x24);
    return fov > 0.5f && fov < 2.0f && aspect > 1.0f && aspect < 2.5f;
}

bool OdysseyOffsetExtractor::isValidCameraSystem(uint64_t ptr) const {
    if (ptr == 0) return false;
    if (ptr + 0x100 > ram_size_) return false;
    
    // Check if it has active_camera pointer at reasonable offset
    for (uint32_t off = 0; off < 0x30; off += 8) {
        uint64_t candidate = read64(ram_.data() + ptr + off);
        if (isValidCameraPtr(candidate)) {
            return true;
        }
    }
    return false;
}

bool OdysseyOffsetExtractor::isValidSceneGraph(uint64_t ptr) const {
    if (ptr == 0) return false;
    if (ptr + 0x200 > ram_size_) return false;
    
    uint32_t node_count = read32(ram_.data() + ptr);
    return node_count > 0 && node_count < 100000;
}

bool OdysseyOffsetExtractor::isValidSceneGraphPtr(uint64_t ptr) const {
    if (ptr == 0) return false;
    if (ptr + 0x200 > ram_size_) return false;
    
    // Check for SceneGraph signature
    uint32_t node_count = read32(ram_.data() + ptr);
    uint32_t flags = read32(ram_.data() + ptr + 4);
    uint64_t root_ptr = read64(ram_.data() + ptr + 8);
    
    return node_count > 0 && node_count < 100000 && root_ptr != 0;
}

bool OdysseyOffsetExtractor::exportToHeader(const CameraOffsets& offsets, const std::string& output_path) {
    std::ofstream out(output_path);
    if (!out) return false;
    
    out << "// Auto-generated Odyssey offsets\n";
    out << "#pragma once\n\n";
    out << "#include <cstdint>\n\n";
    out << "namespace mgd {\nnamespace re {\n\n";
    out << "struct CameraOffsets {\n";
    out << "    uint64_t camera_system_ptr = 0;\n";
    out << "    uint64_t active_camera_ptr = 0;\n";
    out << "    uint32_t pos_offset = 0;\n";
    out << "    uint32_t rot_offset = 0;\n";
    out << "    uint32_t fov_offset = 0;\n";
    out << "    uint32_t aspect_offset = 0;\n";
    out << "    uint32_t near_plane_offset = 0;\n";
    out << "    uint32_t far_plane_offset = 0;\n";
    out << "    uint32_t view_mode_offset = 0;\n";
    out << "    uint32_t camera_type_offset = 0;\n";
    out << "    uint32_t ortho_scale_offset = 0;\n";
    out << "    uint64_t scene_graph_ptr = 0;\n";
    out << "    uint32_t node_count_offset = 0;\n";
    out << "    uint64_t root_node_offset = 0;\n";
    out << "    uint32_t visible_count_offset = 0;\n";
    out << "    uint32_t frustum_planes_offset = 0;\n";
    out << "    uint32_t culling_system_offset = 0;\n";
    out << "    uint32_t max_visible_nodes = 0;\n";
    out << "    uint32_t culling_flags_offset = 0;\n";
    out << "    uint32_t lod_distances_offset = 0;\n";
    out << "    bool validated = false;\n";
    out << "};\n\n";
    out << "} // namespace re\n";
    out << "} // namespace mgd\n";
    return true;
}

bool OdysseyOffsetExtractor::exportToJson(const OdysseyOffsets& offsets, const std::string& output_path) {
    std::ofstream out(output_path);
    if (!out) return false;
    
    out << "{\n";
    out << "  \"version\": " << static_cast<int>(offsets.version) << ",\n";
    out << "  \"camera\": {\n";
    out << "    \"camera_system_ptr\": \"0x" << std::hex << offsets.camera.camera_system_ptr << "\",\n";
    out << "    \"active_camera_ptr\": \"0x" << std::hex << offsets.camera.active_camera_ptr << "\",\n";
    out << "    \"pos_offset\": " << std::dec << offsets.camera.pos_offset << ",\n";
    out << "    \"rot_offset\": " << offsets.camera.rot_offset << ",\n";
    out << "    \"fov_offset\": " << offsets.camera.fov_offset << ",\n";
    out << "    \"aspect_offset\": " << offsets.camera.aspect_offset << ",\n";
    out << "    \"near_plane_offset\": " << offsets.camera.near_plane_offset << ",\n";
    out << "    \"far_plane_offset\": " << offsets.camera.far_plane_offset << ",\n";
    out << "    \"view_mode_offset\": " << offsets.camera.view_mode_offset << ",\n";
    out << "    \"camera_type_offset\": " << offsets.camera.camera_type_offset << ",\n";
    out << "    \"ortho_scale_offset\": " << offsets.camera.ortho_scale_offset << ",\n";
    out << "    \"scene_graph_ptr\": \"0x" << std::hex << offsets.camera.scene_graph_ptr << "\",\n";
    out << "    \"node_count_offset\": " << std::dec << offsets.camera.node_count_offset << ",\n";
    out << "    \"root_node_offset\": \"0x" << std::hex << offsets.camera.root_node_offset << "\",\n";
    out << "    \"visible_count_offset\": " << offsets.camera.visible_count_offset << ",\n";
    out << "    \"frustum_planes_offset\": " << offsets.camera.frustum_planes_offset << ",\n";
    out << "    \"culling_system_offset\": " << offsets.camera.culling_system_offset << ",\n";
    out << "    \"max_visible_nodes\": " << offsets.camera.max_visible_nodes << ",\n";
    out << "    \"culling_flags_offset\": " << offsets.camera.culling_flags_offset << ",\n";
    out << "    \"lod_distances_offset\": " << offsets.camera.lod_distances_offset << ",\n";
    out << "    \"validated\": " << (offsets.validated ? "true" : "false") << "\n";
    out << "  },\n";
    out << "  \"scene_graph\": {\n";
    out << "    \"scene_graph_ptr\": \"0x" << std::hex << offsets.scene_graph.scene_graph_ptr << "\",\n";
    out << "    \"node_count_offset\": " << std::dec << offsets.scene_graph.node_count_offset << ",\n";
    out << "    \"root_node_offset\": \"0x" << std::hex << offsets.scene_graph.root_node_offset << "\",\n";
    out << "    \"visible_count_offset\": " << offsets.scene_graph.visible_count_offset << ",\n";
    out << "    \"frustum_planes_offset\": " << offsets.scene_graph.frustum_planes_offset << "\n";
    out << "  },\n";
    out << "  \"culling\": {\n";
    out << "    \"culling_system_offset\": " << std::dec << offsets.culling.culling_system_offset << ",\n";
    out << "    \"max_visible_nodes\": " << offsets.culling.max_visible_nodes << ",\n";
    out << "    \"culling_flags_offset\": " << offsets.culling.culling_flags_offset << ",\n";
    out << "    \"lod_distances_offset\": " << offsets.culling.lod_distances_offset << "\n";
    out << "  },\n";
    out << "  \"scene_root\": \"0x" << std::hex << offsets.scene_root << "\",\n";
    out << "  \"polygon_buffer\": \"0x" << std::hex << offsets.polygon_buffer << "\",\n";
    out << "  \"visible_count\": " << std::dec << offsets.visible_count << ",\n";
    out << "  \"world_transforms\": \"0x" << std::hex << offsets.world_transforms << "\",\n";
    out << "  \"material_db\": \"0x" << std::hex << offsets.material_db << "\",\n";
    out << "  \"texture_db\": \"0x" << std::hex << offsets.texture_db << "\",\n";
    out << "  \"mesh_db\": \"0x" << std::hex << offsets.mesh_db << "\",\n";
    out << "  \"version_build\": \"0x" << std::hex << offsets.version_build << "\",\n";
    out << "  \"validated\": " << (offsets.validated ? "true" : "false") << "\n";
    out << "}\n";
    return true;
}

bool OdysseyOffsetExtractor::findSceneGraph(uint64_t camera_system_ptr, uint64_t& out_ptr) {
    return findSceneGraph(camera_system_ptr, out_ptr);
}

bool OdysseyOffsetExtractor::isValidSceneGraphPtr(uint64_t ptr) const {
    return isValidSceneGraphPtr(ptr);
}

} // namespace re
} // namespace mgd