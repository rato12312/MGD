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

OdysseyOffsets OdysseyOffsetExtractor::extractOffsets() {
    CameraOffsets offsets;
    offsets.validated = false;

    // 1. Detect version
    OdysseyVersion version = detectVersion(ram_);
    
    // 2. Find CameraSystem
    uint64_t camera_system_ptr = 0;
    if (!findCameraSystem(ram_.data(), ram_size_, camera_system_ptr_)) {
        std::cerr << "[WARN] CameraSystem not found, using fallback offsets\n";
    }

    // 3. Find active camera
    uint64_t active_camera_ptr = 0;
    if (camera_system_ptr_) {
        // CameraSystem typically has active_camera at offset 0x0 or 0x8
        for (uint32_t off = 0; off < 0x20; off += 8) {
            uint64_t candidate = read64(camera_system_ptr_ + off);
            if (isValidCameraPtr(candidate)) {
                active_camera_ptr_ = candidate;
                break;
            }
        }
    }

    // 4. Find camera state offsets
    if (active_camera_ptr_) {
        findCameraStateOffsets(active_camera_ptr_, camera_offsets_);
    }

    // 5. Find SceneGraph
    uint64_t scene_graph_ptr = 0;
    if (camera_system_ptr_) {
        findSceneGraph(camera_system_ptr_, scene_graph_ptr_);
    }

    // 6. Find SceneGraph fields
    if (scene_graph_ptr_) {
        findSceneGraphFields(camera_offsets_);
    }

    // 7. Find CullingSystem
    findCullingSystem(camera_offsets_);

    // 5. Detect version
    OdysseyVersion version = detectVersion(ram_);

    CameraOffsets offsets;
    offsets.validated = true;
    return offsets;
}

bool OdysseyOffsetExtractor::findCameraSystem(const uint8_t* ram, size_t size, uint64_t& out_ptr) {
    static constexpr uint8_t CAMERA_SIG[] = {0x43, 0x61, 0x6D, 0x65, 0x72, 0x61}; // "Camera"
    
    for (size_t i = 0; i + sizeof(CAMERA_SIG) <= size; i += 4) {
        if (std::memcmp(ram + i, CAMERA_SIG, sizeof(CAMERA_SIG)) == 0) {
            // Found "Camera" string, check if this is a CameraSystem
            uint64_t candidate = i;
            if (isValidCameraSystem(candidate)) {
                out_ptr = candidate;
                return true;
            }
        }
    }
    return false;
}

bool OdysseyOffsetExtractor::findSceneGraph(uint64_t camera_system_ptr, uint64_t& out_ptr) {
    if (!camera_system_ptr || camera_system_ptr + 0x30 > ram_size_) return false;
    
    // SceneGraph usually at offset 0x10 or 0x18 from CameraSystem
    for (uint32_t off = 0x8; off < 0x30; off += 8) {
        uint64_t candidate = read64(camera_system_ptr + off);
        if (isValidSceneGraph(candidate)) {
            out_ptr = candidate;
            return true;
        }
    }
    return false;
}

void OdysseyOffsetExtractor::findCameraStateOffsets(uint64_t camera_ptr, CameraOffsets& out) {
    if (!camera_ptr || camera_ptr + 0x100 > ram_size_) return;
    
    // Search for FOV (float ~1.0) and aspect ratio (~1.77)
    for (uint32_t off = 0x1C; off < 0x80; off += 4) {
        if (camera_ptr + off + 4 > ram_size_) break;
        float fov = readFloat(camera_ptr + off);
        if (fov > 0.5f && fov < 2.0f) {
            // Found potential FOV
            float aspect = readFloat(camera_ptr + off + 4);
            if (aspect > 1.0f && aspect < 2.5f) {
                out.fov_offset = off;
                out.aspect_offset = off + 4;
                out.pos_offset = off - 0x20; // position usually before FOV
                out.rot_offset = off + 0x10;
                out.fov_offset = off;
                out.aspect_offset = off + 4;
                return;
            }
        }
    }
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
    
    uint32_t node_count = read32(scene_graph_ptr);
    if (node_count > 0 && node_count < 100000) {
        scene_graph_fields_.node_count_offset = 0;
        scene_graph_fields_.root_node_offset = 0x8;
        scene_graph_fields_.visible_count_offset = 0x10;
        scene_graph_fields_.frustum_planes_offset = 0x20;
    }
}

void OdysseyOffsetExtractor::findCullingSystem(CameraOffsets& out) {
    // CullingSystem often at fixed offset from SceneGraph
    culling_system_offset_ = 0x40;
    max_visible_nodes_ = 0x10;
    culling_flags_offset_ = 0x14;
    lod_distances_offset_ = 0x20;
}

OdysseyVersion OdysseyOffsetExtractor::detectVersion(const std::vector<uint8_t>& ram) {
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

bool OdysseyOffsetExtractor::exportToJson(const std::string& output_path) {
    std::ofstream out(output_path);
    if (!out) return false;
    
    out << "{\n";
    out << "  \"camera_system_ptr\": \"0x" << std::hex << camera_system_ptr_ << "\",\n";
    out << "  \"active_camera_ptr\": \"0x" << std::hex << active_camera_ptr_ << "\",\n";
    out << "  \"pos_offset\": " << std::dec << pos_offset_ << ",\n";
    out << "  \"rot_offset\": " << rot_offset_ << ",\n";
    out << "  \"fov_offset\": " << fov_offset_ << ",\n";
    out << "  \"aspect_offset\": " << aspect_offset_ << ",\n";
    out << "  \"near_plane_offset\": " << near_plane_offset_ << ",\n";
    out << "  \"far_plane_offset\": " << far_plane_offset_ << ",\n";
    out << "  \"view_mode_offset\": " << view_mode_offset_ << ",\n";
    out << "  \"camera_type_offset\": " << camera_type_offset_ << ",\n";
    out << "  \"ortho_scale_offset\": " << ortho_scale_offset_ << ",\n";
    out << "  \"scene_graph_ptr\": \"0x" << std::hex << scene_graph_ptr_ << "\",\n";
    out << "  \"node_count_offset\": " << std::dec << node_count_offset_ << ",\n";
    out << "  \"root_node_offset\": \"0x" << std::hex << root_node_offset_ << "\",\n";
    out << "  \"visible_count_offset\": " << std::dec << visible_count_offset_ << ",\n";
    out << "  \"frustum_planes_offset\": \"0x" << std::hex << frustum_planes_offset_ << "\",\n";
    out << "  \"culling_system_offset\": \"0x" << std::hex << culling_system_offset_ << "\",\n";
    out << "  \"max_visible_nodes\": " << std::dec << max_visible_nodes_ << ",\n";
    out << "  \"culling_flags_offset\": \"0x" << std::hex << culling_flags_offset_ << "\",\n";
    out << "  \"lod_distances_offset\": \"0x" << std::hex << lod_distances_offset_ << "\"\n";
    out << "}\n";
    return true;
}

} // namespace re
} // namespace mgd