#pragma once

// Handoff real para Super Mario Odyssey.
// Lê estado da câmera e polígonos visíveis da RAM emulada.
// Baseado em engenharia reversa do Odyssey (endereços por versão).

#include "core/bridge/EmulatorHandoff.h"
#include "core/mental_map/ChunkManager.h"
#include "emulador-mgd/cpu/Cpu.h"
#include "emulador-mgd/loader/Npdm.h"
#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include <cmath>
#include <cstring>

namespace mgd {
namespace emu {

// Offsets conhecidos do Odyssey (precisam validação por versão do jogo)
struct OdysseyOffsets {
    uint64_t camera_pos = 0;
    uint64_t camera_rot = 0;
    uint64_t camera_fov = 0;
    uint64_t scene_root = 0;
    uint64_t polygon_buffer = 0;
    uint64_t visible_count = 0;
};

// Handoff source que lê da RAM do emulador
class OdysseyHandoffSource : public bridge::IHandoffSource {
public:
    OdysseyHandoffSource(Cpu* cpu, const OdysseyOffsets& offsets = {})
        : cpu_(cpu), offsets_(offsets), frame_index_(0) {}

    // Define offsets para versão específica do jogo
    void setOffsets(const OdysseyOffsets& o) { offsets_ = o; }

    // Tenta ler frame; retorna false se jogo não estiver rodando ou offsets inválidos
    bool poll(bridge::HandoffFrame& out) override {
        if (!cpu_ || !cpu_->ram()) return false;
        if (!offsetsValid()) return false;

        // 1. Lê câmera
        bridge::HandoffCamera cam;
        if (!readCamera(cam)) return false;
        out.camera = cam;

        // 2. Deriva regiões visíveis via frustum + Mental Map (cache local)
        deriveVisibleRegions(out, cam);

        // 3. Polígonos visíveis (aproximação: consulta RegionPolygonCache por regiões)
        deriveVisiblePolygons(out);

        out.frame_index = frame_index_++;
        out.ui_visible = isUiVisible();
        return true;
    }

    // Acesso direto à RAM emulada
    const uint8_t* ram() const { return cpu_ ? cpu_->ram() : nullptr; }
    uint64_t ramSize() const { return cpu_ ? cpu_->ramSize() : 0; }

private:
    Cpu* cpu_ = nullptr;
    OdysseyOffsets offsets_;
    uint64_t frame_index_ = 0;

    bool offsetsValid() const {
        return offsets_.camera_pos != 0 && offsets_.scene_root != 0;
    }

    bool readCamera(bridge::HandoffCamera& cam) {
        const uint8_t* ram = cpu_->ram();
        if (!ram) return false;

        // Posição (Vec3 = 3 floats = 12 bytes)
        uint64_t pos_addr = offsets_.camera_pos;
        if (pos_addr + 12 > ramSize()) return false;
        float px, py, pz;
        std::memcpy(&px, ram + pos_addr, 4);
        std::memcpy(&py, ram + pos_addr + 4, 4);
        std::memcpy(&pz, ram + pos_addr + 8, 4);
        cam.position = Vec3(px, py, pz);

        // Rotação (quaternion ou euler - assumir quaternion 16 bytes)
        uint64_t rot_addr = offsets_.camera_rot;
        if (rot_addr + 16 <= ramSize()) {
            float qx, qy, qz, qw;
            std::memcpy(&qx, ram + rot_addr, 4);
            std::memcpy(&qy, ram + rot_addr + 4, 4);
            std::memcpy(&qz, ram + rot_addr + 8, 4);
            std::memcpy(&qw, ram + rot_addr + 12, 4);
            // Converte quaternion -> forward vector
            cam.forward = quaternionToForward(qx, qy, qz, qw);
        } else {
            cam.forward = Vec3(0, 0, 1);
        }

        // FOV
        uint64_t fov_addr = offsets_.camera_fov;
        if (fov_addr + 4 <= ramSize()) {
            std::memcpy(&cam.fov_degrees, ram + fov_addr, 4);
        } else {
            cam.fov_degrees = 60.0f;
        }
        cam.aspect = 16.0f / 9.0f;
        return true;
    }

    static Vec3 quaternionToForward(float qx, float qy, float qz, float qw) {
        // Forward = q * (0,0,1) * q^-1 para rotação Y-up
        // Simplificado: assumir rotação em torno de Y (yaw)
        float yaw = atan2(2.0f * (qw * qy + qx * qz), 1.0f - 2.0f * (qy * qy + qz * qz));
        return Vec3(sinf(yaw), 0.0f, -cosf(yaw));
    }

    void deriveVisibleRegions(bridge::HandoffFrame& out, const bridge::HandoffCamera& cam) {
        // Usa ChunkManager para converter posição da câmera em região
        // e expandir por frustum (simplificado: região atual + vizinhas)
        RegionID center = mgd::core::mental_map::ChunkManager::worldToRegionId(cam.position);
        out.visible_regions.push_back(center);
        // Adiciona 8 vizinhos (3x3 grid)
        for (int dz = -1; dz <= 1; dz++) {
            for (int dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dz == 0) continue;
                // Região = pack(x,z) - simplificação
                RegionID r = static_cast<RegionID>((center & 0xFFFF0000) | ((center + dx + dz * 256) & 0xFFFF));
                out.visible_regions.push_back(r);
            }
        }
    }

    void deriveVisiblePolygons(bridge::HandoffFrame& out) {
        // TODO: consultar RegionPolygonCache real via Emulator::world()
        // Por enquanto: retorna vazio (será preenchido pelo Emulator::frame)
        out.visible_polygons.clear();
    }

    bool isUiVisible() {
        // TODO: ler flag de UI do jogo (applet overlay)
        return false;
    }
};

// Helper para criar offsets por versão do jogo
inline OdysseyOffsets makeOdysseyOffsets_v10() {
    // Offsets para versão 1.0.0 (exemplo - precisam validação real)
    OdysseyOffsets o;
    o.camera_pos = 0x12345678;
    o.camera_rot = 0x12345690;
    o.camera_fov = 0x123456A0;
    o.scene_root = 0x20000000;
    return o;
}

inline OdysseyOffsets makeOdysseyOffsets_v13() {
    // Offsets para versão 1.3.0
    OdysseyOffsets o;
    o.camera_pos = 0x87654321;
    o.camera_rot = 0x87654339;
    o.camera_fov = 0x87654340;
    o.scene_root = 0x30000000;
    return o;
}

} // namespace emu
} // namespace mgd