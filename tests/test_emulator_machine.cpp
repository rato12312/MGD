#include <iostream>
#include <string>
#include <vector>

#define ASSERT_MSG(cond, msg) do { if (!(cond)) { std::cerr << "FAIL: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; return false; } } while(0)

#include "emulador-mgd/cpu/Cpu.h"
#include "emulador-mgd/ram/GuestRam.h"
#include "emulador-mgd/handoff/CaptureStub.h"
#include "emulador-mgd/runtime/Emulator.h"

using namespace mgd;

bool run_emulator_machine_tests() {
    // CPU: MOVZ X0,#7 + ADD X1,X0,#3 + STR/LDR round-trip pela RAM.
    {
        emu::Cpu cpu;
        ASSERT_MSG(cpu.step(0xD28000E0u), "movz");
        ASSERT_MSG(cpu.reg(0) == 7, "x0=7");
        ASSERT_MSG(cpu.step(0x91000C01u), "add");
        ASSERT_MSG(cpu.reg(1) == 10, "x1=10");
        cpu.setReg(2, 0x200);
        ASSERT_MSG(cpu.step(0xF8000041u), "str x1,[x2]");
        cpu.setReg(1, 0);
        ASSERT_MSG(cpu.step(0xF9400043u), "ldr x3,[x2]");
        ASSERT_MSG(cpu.reg(3) == 10, "x3=10");
        ASSERT_MSG(!cpu.step(0xD4000001u), "svc para");
    }

    // GuestRam: escrita/leitura + falha fechada fora do limite.
    {
        emu::GuestRam ram(64);
        ASSERT_MSG(ram.write32(0, 0xDEADBEEFu), "w32");
        uint32_t v32 = 0;
        ASSERT_MSG(ram.read32(0, v32) && v32 == 0xDEADBEEFu, "r32");
        ASSERT_MSG(ram.write64(8, 0x1122334455667788ull), "w64");
        uint64_t v64 = 0;
        ASSERT_MSG(ram.read64(8, v64) && v64 == 0x1122334455667788ull, "r64");
        ASSERT_MSG(!ram.write32(64, 1), "fora nega escrita");
        ASSERT_MSG(!ram.read64(60, v64), "fora nega leitura");
    }

    // Handoff: null sem estado, scripted entrega fila.
    {
        emu::NullHandoff nullh;
        bridge::HandoffFrame f;
        ASSERT_MSG(!nullh.poll(f), "null sem estado");
        emu::ScriptedHandoff script;
        bridge::HandoffFrame f1;
        f1.frame_index = 7;
        script.push(f1);
        ASSERT_MSG(script.pending() == 1, "fila tem 1");
        ASSERT_MSG(script.poll(f) && f.frame_index == 7, "entrega frame 7");
        ASSERT_MSG(!script.poll(f), "fila esvaziou");
    }

    // Emulador: programa depositado roda pelo PC e mundo boota junto.
    {
        emu::Emulator emu;
        std::vector<uint32_t> prog = {
            0xD28000E0u, // MOVZ X0, #7
            0x91000C01u, // ADD X1, X0, #3
            0xD4000001u, // SVC (para)
        };
        ASSERT_MSG(emu.loadProgram(prog, 0), "programa cabe");
        ASSERT_MSG(emu.runCpu(16) == 2, "roda 2 e para no svc");
        ASSERT_MSG(emu.cpu().reg(1) == 10, "x1=10");
        bridge::RuntimeFrameStats s = emu.bootWorld(10);
        ASSERT_MSG(s.polygons_fed == 10, "mundo com 10");
        ASSERT_MSG(s.pixels_written > 0, "mundo pinta");
    }

    std::cout << "  Emulator machine tests passed!" << std::endl;
    return true;
}
