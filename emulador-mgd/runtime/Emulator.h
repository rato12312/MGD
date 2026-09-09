#pragma once

// Emulador MGD: amarra CPU + RAM + mundo Odyssey.
// Fluxo: programa na RAM -> CPU executa -> handoff alimenta o mapa mental.

#include <chrono>
#include <cstdint>
#include <cstring>
#include <vector>
#include <memory>

#include "../cpu/Cpu.h"
#include "../hos/Kernel.h"
#include "../hos/IpcMessage.h"
#include "../loader/NroLoader.h"
#include "../loader/NsoLoader.h"
#include "../loader/Keys.h"
#include "../loader/NcaSections.h"
#include "../loader/Pfs0.h"
#include "../loader/RomFs.h"
#include "../odyssey/OdysseyWorld.h"
#include "../odyssey/OdysseyHandoff.h"
#include "../core/bridge/EmulatorHandoff.h"
#include "../core/bridge/MentalMapRuntime.h"
#include "../config/MgdSwitches.h"

namespace mgd {
namespace emu {

class Emulator {
public:
    Emulator() { applySwitches(); }

    Cpu& cpu() { return cpu_; }
    Mmu& mmu() { return mmu_; }
    hos::Kernel& kernel() { return kernel_; }
    odyssey::OdysseyWorld& world() { return world_; }
    MgdSwitches& switches() { return switches_; }
    emu::KeyManager& keys() { return key_mgr_; }
    odyssey::OdysseyHandoffSource& handoff() { return handoff_; }

    // Aplica as chaves: MMU liga/desliga, barato segue a Mali, painter obedece.
    void applySwitches() {
        cpu_.setSvcHost(&kernel_);
        kernel_.setMmu(&mmu_);
        kernel_.setRam(cpu_.ram(), cpu_.ramSize());
        mmu_.clear();
        if (switches_.mgd_translation) {
            mmu_.map(0x0, 0x0, cpu_.ramSize(), true, true, true);
            cpu_.setMmu(&mmu_);
        } else {
            cpu_.setMmu(nullptr);
        }
        world_.cheap(switches_.cheapForLevel());
        // Inicializa handoff com CPU
        handoff_ = odyssey::OdysseyHandoffSource(&cpu_);
    }

    bool present(const char* path) {
        if (!switches_.mgd_image) return false;
        return world_.present(path);
    }

    // Um frame do sistema: serviços andam, GPU drena, mundo pinta.
    // Mede o próprio tempo (ms): base honesta do fps.
    bool frame(const char* path, uint32_t npolys = 8) {
        auto t0 = std::chrono::steady_clock::now();
        kernel_.pumpServices();
        kernel_.nv().drain(64);
        
        // Handoff real: lê câmera + polígonos do jogo
        bridge::HandoffFrame hf;
        if (handoff_.poll(hf)) {
            // Converte handoff para MentalMapRuntime
            bridge::RuntimeFrameStats stats = world_.runtime().step(hf, {});
            (void)stats;
        } else {
            // Fallback: mundo sintético
            bootWorld(npolys);
        }
        
        bool ok = present(path);
        auto t1 = std::chrono::steady_clock::now();
        last_frame_ms_ =
            std::chrono::duration<double, std::milli>(t1 - t0).count();
        frames_++;
        avg_ms_ = (frames_ == 1) ? last_frame_ms_ : avg_ms_ * 0.9 + last_frame_ms_ * 0.1;
        return ok;
    }
    double lastFrameMs() const { return last_frame_ms_; }
    uint64_t frameCount() const { return frames_; }
    // FPS honesto: média móvel do tempo medido (0 = sem dado).
    double fps() const { return avg_ms_ > 0.0 ? 1000.0 / avg_ms_ : 0.0; }

    // Deposita programa (u32 little-endian) na RAM da CPU.
    bool loadProgram(const std::vector<uint32_t>& prog, uint64_t base = 0) {
        for (size_t i = 0; i < prog.size(); ++i) {
            uint64_t addr = base + i * 4;
            if (addr + 4 > cpu_.ramSize()) return false;
            uint32_t insn = prog[i];
            for (int b = 0; b < 4; b++)
                cpu_.ram()[addr + b] = static_cast<uint8_t>(insn >> (8 * b));
        }
        cpu_.setPc(base);
        return true;
    }

    uint64_t runCpu(uint64_t maxSteps) { return cpu_.run(maxSteps); }
    uint64_t runThreads(uint64_t maxSteps, uint64_t quantum = 4) {
        return kernel_.runThreads(cpu_, maxSteps, quantum);
    }

    // Save state: congela CPU + RAM (kernel/mundo ficam de fora, honesto).
    struct Snapshot {
        Cpu::State cpu;
        std::vector<uint8_t> ram;
    };
    Snapshot snapshot() const {
        Snapshot s;
        s.cpu = cpu_.save();
        s.ram.assign(cpu_.ram(), cpu_.ram() + cpu_.ramSize());
        return s;
    }
    bool restore(const Snapshot& s) {
        if (s.ram.size() != cpu_.ramSize()) return false;
        cpu_.load(s.cpu);
        std::memcpy(cpu_.ram(), s.ram.data(), s.ram.size());
        return true;
    }

    // Boot de NRO: mapeia, aponta SP, pula no entry. Retorna false se inválido.
    bool bootNro(const uint8_t* blob, size_t len, uint64_t base = 0, uint64_t sp = 0x8000) {
        NroImage img = parseNro(blob, len);
        if (!img.valid) return false;
        uint64_t entry = 0;
        if (!loadNroInto(img, blob, cpu_.ram(), cpu_.ramSize(), base, entry)) return false;
        cpu_.setSp(sp);
        cpu_.setPc(entry + 0x80); // pula o header (start sintético)
        return true;
    }

    bool bootNso(const uint8_t* blob, size_t len, uint64_t base = 0, uint64_t sp = 0x8000) {
        NsoImage img = parseNso(blob, len);
        if (!img.valid) return false;
        uint64_t entry = 0;
        if (!loadNsoInto(img, blob, cpu_.ram(), cpu_.ramSize(), base, entry)) return false;
        cpu_.setSp(sp);
        cpu_.setPc(entry);
        return true;
    }

    // Boot NSP real: PFS0 -> NCA -> ExeFS -> main.nso
    bool bootNsp(const uint8_t* nsp, size_t nsp_size) {
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

            loader::KeyManager::NcaDecryptResult dec;
            int header_slot = 0;
            int section_slots[4] = {1, 2, 3, 4};
            if (!key_mgr_.decryptNca(nca_data.data(), nca_data.size(), header_slot, section_slots, dec)) continue;
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

    bridge::RuntimeFrameStats bootWorld(uint32_t n = 20) { return world_.boot(n); }

    // Configura offsets do Odyssey por versão
    void setOdysseyOffsets(const odyssey::OdysseyOffsets& o) { handoff_.setOffsets(o); }

private:
    Cpu cpu_;
    Mmu mmu_;
    hos::Kernel kernel_;
    odyssey::OdysseyWorld world_;
    MgdSwitches switches_;
    emu::KeyManager key_mgr_;
    odyssey::OdysseyHandoffSource handoff_;
    double last_frame_ms_ = 0.0;
    double avg_ms_ = 0.0;
    uint64_t frames_ = 0;
};

} // namespace emu
} // namespace mgd
