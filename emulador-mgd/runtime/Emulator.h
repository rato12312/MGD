#pragma once

// Emulador MGD: amarra CPU + RAM + mundo Odyssey.
// Fluxo: programa na RAM -> CPU executa -> handoff alimenta o mapa mental.

#include <cstdint>
#include <vector>

#include "../cpu/Cpu.h"
#include "../loader/NroLoader.h"
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
    bool frame(const char* path, uint32_t npolys = 8) {
        kernel_.pumpServices();
        kernel_.nv().drain(64);
        bootWorld(npolys);
        return present(path);
    }

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

    bridge::RuntimeFrameStats bootWorld(uint32_t n = 20) { return world_.boot(n); }

private:
    Cpu cpu_;
    Mmu mmu_;
    hos::Kernel kernel_;
    odyssey::OdysseyWorld world_;
    MgdSwitches switches_;
};

} // namespace emu
} // namespace mgd
