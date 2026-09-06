#include <iostream>
#include <string>
#include <vector>

#define ASSERT_MSG(cond, msg) do { if (!(cond)) { std::cerr << "FAIL: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; return false; } } while(0)

#include "emulador-mgd/cpu/Cpu.h"
#include "emulador-mgd/ram/GuestRam.h"
#include "emulador-mgd/handoff/CaptureStub.h"
#include "emulador-mgd/runtime/Emulator.h"
#include "emulador-mgd/loader/NroLoader.h"

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
        ASSERT_MSG(cpu.step(0xD4000001u), "svc #0 sai limpo");
        ASSERT_MSG(cpu.stopped() && cpu.exitCode() == 0, "parou exit 0");
        ASSERT_MSG(!cpu.step(0xD4000041u), "svc #2 desconhecido");
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
            0xD4000001u, // SVC #0 (sai limpo)
        };
        ASSERT_MSG(emu.loadProgram(prog, 0), "programa cabe");
        ASSERT_MSG(emu.runCpu(16) == 3, "roda 3 e para no svc");
        ASSERT_MSG(emu.cpu().stopped(), "parou limpo");
        ASSERT_MSG(emu.cpu().exitCode() == 0, "exit 0");
        ASSERT_MSG(emu.cpu().reg(1) == 10, "x1=10");
        bridge::RuntimeFrameStats s = emu.bootWorld(10);
        ASSERT_MSG(s.polygons_fed == 10, "mundo com 10");
        ASSERT_MSG(s.pixels_written > 0, "mundo pinta");
        ASSERT_MSG(emu.world().present("odyssey_boot.ppm"), "painter apresenta");
    }

    // LDR/STR 32-bit e byte.
    {
        emu::Cpu cpu;
        cpu.setReg(2, 0x100);
        cpu.setReg(1, 0xAABBCCDDull);
        ASSERT_MSG(cpu.step(0xB8000041u), "strw x1,[x2]");
        cpu.setReg(1, 0);
        ASSERT_MSG(cpu.step(0xB9400043u), "ldrw x3,[x2]");
        ASSERT_MSG(cpu.reg(3) == 0xAABBCCDDull, "w round-trip");
        cpu.setReg(1, 0xCC);
        ASSERT_MSG(cpu.step(0x38000441u), "strb x1,[x2,#1]");
        ASSERT_MSG(cpu.step(0x39400044u), "ldrb x4,[x2]");
        ASSERT_MSG(cpu.reg(4) == 0xCC, "byte certo");
        cpu.reset();
        ASSERT_MSG(cpu.step(0xD4000021u), "svc #1");
        ASSERT_MSG(cpu.stopped() && cpu.exitCode() == 0, "sai com x0=0");
    }

    // NRO sintético: header + .text (MOVZ X0,#7; SVC #0) roda do entry.
    {
        std::vector<uint8_t> blob(0x80 + 8, 0);
        blob[0x10] = 'N'; blob[0x11] = 'R'; blob[0x12] = 'O'; blob[0x13] = '0';
        // text: mem 0x80, size 8
        blob[0x20] = 0x80; blob[0x24] = 8;
        uint32_t t0 = 0xD28000E0u, t1 = 0xD4000001u;
        for (int i = 0; i < 4; i++) {
            blob[0x80 + i] = static_cast<uint8_t>(t0 >> (8 * i));
            blob[0x84 + i] = static_cast<uint8_t>(t1 >> (8 * i));
        }
        emu::NroImage img = emu::parseNro(blob.data(), blob.size());
        ASSERT_MSG(img.valid, "nro valido");
        ASSERT_MSG(img.text.size == 8, "text tem 8");
        emu::Cpu cpu;
        uint64_t entry = 0;
        ASSERT_MSG(emu::loadNroInto(img, blob.data(), cpu.ram(), cpu.ramSize(), 0, entry), "nro na ram");
        cpu.setPc(entry + 0x80); // pula header (start sintético é zero)
        ASSERT_MSG(cpu.run(8) == 2, "roda text do nro");
        ASSERT_MSG(cpu.reg(0) == 7 && cpu.stopped(), "x0=7 e parou");
        emu::NroImage bad = emu::parseNro(blob.data(), 16);
        ASSERT_MSG(!bad.valid, "curto invalido");
    }

    std::cout << "  Emulator machine tests passed!" << std::endl;
    return true;
}
