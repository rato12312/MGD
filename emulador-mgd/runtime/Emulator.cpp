// Emulator.cpp - Implementation

#include "Emulator.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace mgd {
namespace emu {

bool Emulator::loadKeys(const std::string& keys_dir) {
    std::string prod_path = keys_dir + "/prod.keys";
    std::string title_path = keys_dir + "/title.keys";
    
    bool ok = key_mgr_.loadProdKeys(prod_path);
    if (!ok) {
        std::cerr << "[WARN] Failed to load prod.keys from " << prod_path << std::endl;
    }
    
    bool ok2 = key_mgr_.loadTitleKeys(title_path);
    if (!ok2) {
        std::cerr << "[WARN] Failed to load title.keys from " << title_path << std::endl;
    }
    
    return ok || ok2;
}

bool Emulator::bootNspWithKeys(const uint8_t* nsp, size_t nsp_size, int header_key_slot, const int section_key_slots[4]) {
    // 1. Parse PFS0 container
    std::vector<uint8_t> pfs0_data(nsp, nsp + nsp_size);
    loader::Pfs0Reader pfs0(pfs0_data);
    if (!pfs0.valid()) return false;

    for (size_t i = 0; i < pfs0.entryCount(); i++) {
        auto entry = pfs0.entry(i);
        if (!entry.valid) continue;
        if (entry.name.find(".nca") == std::string::npos) continue;

        std::vector<uint8_t> nca_data(entry.size);
        if (!pfs0.readEntry(i, nca_data.data(), nca_data.size())) continue;

        loader::NcaProbe probe(nca_data.data(), nca_data.size());
        if (!probe.valid() || probe.type() != loader::NcaType::PROGRAM) continue;

        // Decrypt with provided keys
        loader::KeyManager::NcaDecryptResult dec;
        int header_slot = 0; // header key slot
        int section_slots[4] = {1, 2, 3, 4}; // section key slots
        
        if (!key_mgr_.decryptNca(nca_data.data(), nca_data.size(), header_key_slot, section_key_slots, dec)) continue;
        if (!dec.valid) continue;

        loader::NcaSections sections(dec.header.data(), dec.header.size());
        if (!sections.valid()) continue;

        for (int s = 0; s < 4; s++) {
            if (sections.section(s).size == 0) continue;
            auto& sec = dec.sections[s];
            if (sec.data.empty()) continue;

            loader::Pfs0Reader exefs(sec.data);
            if (!exefs.valid()) continue;

            for (size_t j = 0; j < exefs.entryCount(); j++) {
                auto e = exefs.entry(j);
                if (!e.valid || e.name != "main") continue;
                if (e.name.find(".nso") == std::string::npos && e.name != "main") continue;

                std::vector<uint8_t> nso_data(e.size);
                if (!exefs.readEntry(j, nso_data.data(), nso_data.size())) continue;

                if (bootNso(nso_data.data(), nso_data.size())) {
                    kernel_.services().publish("appletOE");
                    hos::IpcMessage req{2, {}};
                    hos::IpcMessage rep;
                    kernel_.applet().dispatch(req, rep);
                    return true;
                }
            }
        }
    }
    return false;
}

bool Emulator::loadOdysseyOffsets(const std::string& offsets_file) {
    // Try to load JSON offsets file
    std::ifstream file(offsets_file);
    if (!file.is_open()) {
        std::cerr << "[WARN] Could not open offsets file: " << offsets_file << std::endl;
        return false;
    }
    
    // Parse JSON (simplified - in reality use a JSON library)
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    
    // For now, use built-in v1.5.0 offsets as fallback
    odyssey::OdysseyOffsets offsets = re::makeOdysseyOffsets(re::OdysseyVersion::V150);
    handoff_.setOffsets(re::CameraOffsets{
        .camera_system_ptr = 0,
        .active_camera_ptr = 0,
        .pos_offset = 0,
        .rot_offset = 0,
        .fov_offset = 0,
        .aspect_offset = 0,
        .near_plane_offset = 0,
        .far_plane_offset = 0,
        .view_mode_offset = 0,
        .camera_type_offset = 0,
        .ortho_scale_offset = 0,
        .scene_graph_ptr = 0,
        .node_count_offset = 0,
        .root_node_offset = 0,
        .visible_count_offset = 0,
        .frustum_planes_offset = 0,
        .culling_system_offset = 0,
        .max_visible_nodes = 0,
        .culling_flags_offset = 0,
        .lod_distances_offset = 0,
        .validated = false
    });
    
    std::cout << "[INFO] Using default Odyssey v1.5.0 offsets" << std::endl;
    return true;
}

} // namespace emu
} // namespace mgd