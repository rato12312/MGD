#pragma once

// Emulador MGD: amarra CPU + RAM + mundo Odyssey.
// Fluxo: programa na RAM -> CPU executa -> handoff alimenta o mapa mental.

#include <cstdint>
#include <vector>

#include "../cpu/Cpu.h"
#include "../odyssey/OdysseyWorld.h"

namespace mgd {
namespace emu {

class Emulator {
public:
    Emulator() = default;

    Cpu& cpu() { return cpu_; }
    odyssey::OdysseyWorld& world() { return world_; }

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

    bridge::RuntimeFrameStats bootWorld(uint32_t n = 20) { return world_.boot(n); }

private:
    Cpu cpu_;
    odyssey::OdysseyWorld world_;
};

} // namespace emu
} // namespace mgd
