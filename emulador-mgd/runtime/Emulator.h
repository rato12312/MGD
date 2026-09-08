#pragma once

// Emulador MGD: amarra CPU + RAM + mundo Odyssey.
// Fluxo: programa na RAM -> CPU executa -> handoff alimenta o mapa mental.

#include <cstdint>
#include <vector>

#include <chrono>

#include "../cpu/Cpu.h"
#include "../hos/Kernel.h"
#include "../loader/NroLoader.h"
#include "../loader/NsoLoader.h"
#include "../odyssey/OdysseyWorld.h"
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
        bootWorld(npolys);
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

    bridge::RuntimeFrameStats bootWorld(uint32_t n = 20) { return world_.boot(n); }

private:
    Cpu cpu_;
    Mmu mmu_;
    hos::Kernel kernel_;
    odyssey::OdysseyWorld world_;
    MgdSwitches switches_;
    double last_frame_ms_ = 0.0;
    double avg_ms_ = 0.0;
    uint64_t frames_ = 0;
};

} // namespace emu
} // namespace mgd
