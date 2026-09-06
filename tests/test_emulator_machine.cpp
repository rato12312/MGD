#include <iostream>
#include <string>
#include <vector>

#define ASSERT_MSG(cond, msg) do { if (!(cond)) { std::cerr << "FAIL: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; return false; } } while(0)

#include "emulador-mgd/cpu/Cpu.h"
#include "emulador-mgd/ram/GuestRam.h"
#include "emulador-mgd/handoff/CaptureStub.h"
#include "emulador-mgd/runtime/Emulator.h"
#include "emulador-mgd/loader/NroLoader.h"
#include "emulador-mgd/ram/Mmu.h"
#include "core/query/RegionPolygonCache.h"

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

    // Orçamento de RAM: teto de 10 polígonos, região antiga cai.
    {
        RegionPolygonCache cache;
        cache.setBudget(10);
        for (uint32_t r = 0; r < 3; ++r) {
            for (uint32_t i = 0; i < 5; ++i) {
                Polygon p;
                p.position = Vec3(static_cast<float>(r * 100 + i), 0.0f, 0.0f);
                p.polygon_id = 100 + r * 10 + i;
                p.asset_id = 1;
                cache.insert(r, p);
            }
        }
        ASSERT_MSG(cache.polygonCount() <= 10, "teto respeitado");
        ASSERT_MSG(cache.evictions() >= 1, "eviccao aconteceu");
        ASSERT_MSG(!cache.findPolygon(100).has_value(), "regiao antiga caiu");
    }

    // ADRP + STP/LDP + CBZ/CBNZ (prólogo real de função).
    {
        emu::Cpu cpu;
        cpu.setPc(0x1000);
        ASSERT_MSG(cpu.step(0x90000000u), "adrp x0,page");
        ASSERT_MSG(cpu.reg(0) == 0x1000, "x0=page");
        cpu.setReg(0, 11);
        cpu.setReg(1, 22);
        cpu.setReg(2, 0x200);
        ASSERT_MSG(cpu.step(0xA9010440u), "stp x0,x1,[x2,#16]");
        ASSERT_MSG(cpu.step(0xA9411063u), "ldp x3,x4,[x2,#16]");
        ASSERT_MSG(cpu.reg(3) == 11 && cpu.reg(4) == 22, "par ok");
        cpu.setReg(0, 0);
        uint64_t pc = cpu.pc();
        ASSERT_MSG(cpu.step(0xB4000040u), "cbz x0,+8 pula");
        ASSERT_MSG(cpu.pc() == pc + 8, "pulou");
        cpu.setReg(1, 5);
        pc = cpu.pc();
        ASSERT_MSG(cpu.step(0xB5000041u), "cbnz x1,+8 pula");
        ASSERT_MSG(cpu.pc() == pc + 8, "pulou 2");
        cpu.setReg(1, 0);
        pc = cpu.pc();
        ASSERT_MSG(cpu.step(0xB5000041u), "cbnz x1 nao pula");
        ASSERT_MSG(cpu.pc() == pc + 4, "seguiu");
    }

    // MMU: mapa + permissão + fault fechado.
    {
        emu::Mmu mmu;
        mmu.map(0x1000, 0x1000, 0x1000, true, false, true); // r-x
        uint64_t pa = 0;
        ASSERT_MSG(mmu.translate(0x1400, 8, false, false, pa) && pa == 0x1400, "le ok");
        ASSERT_MSG(mmu.translate(0x1400, 8, false, true, pa), "exec ok");
        ASSERT_MSG(!mmu.translate(0x1400, 8, true, false, pa), "escrita negada");
        ASSERT_MSG(!mmu.translate(0x5000, 8, false, false, pa), "fora fault");
        ASSERT_MSG(!mmu.translate(0x1FFC, 8, false, false, pa), "corta regiao fault");
        mmu.unmap(0x1000);
        ASSERT_MSG(mmu.regionCount() == 0, "desmapeou");
        ASSERT_MSG(!mmu.translate(0x1400, 8, false, false, pa), "sem regiao fault");
    }

    // CPU ligada na MMU: acesso passa por translate.
    {
        emu::Cpu cpu;
        emu::Mmu mmu;
        mmu.map(0x0, 0x0, 0x1000, true, true, true);    // código rwx
        mmu.map(0x1000, 0x1000, 0x1000, true, true, false); // dados rw-
        mmu.map(0x2000, 0x2000, 0x1000, true, false, false); // const r--
        cpu.setMmu(&mmu);
        cpu.setReg(2, 0x1000);
        cpu.setReg(0, 0x1234);
        ASSERT_MSG(cpu.step(0xF8000020u), "str via mmu ok");
        cpu.setReg(0, 0);
        ASSERT_MSG(cpu.step(0xF9400020u), "ldr via mmu ok");
        ASSERT_MSG(cpu.reg(0) == 0x1234, "dado certo");
        // escrita em região r-- nega
        cpu.setReg(2, 0x2000);
        ASSERT_MSG(!cpu.step(0xF8000020u), "str sem w nega");
        // remapeamento: VA 0x3000 enxerga o PA 0x1000
        mmu.map(0x3000, 0x1000, 0x1000, true, true, false);
        cpu.setReg(2, 0x3000);
        cpu.setReg(0, 0);
        ASSERT_MSG(cpu.step(0xF9400020u), "ldr via alias ok");
        ASSERT_MSG(cpu.reg(0) == 0x1234, "alias le o mesmo fisico");
        // fetch sem x para o run
        emu::Cpu cpu2;
        emu::Mmu mmu2;
        mmu2.map(0x0, 0x0, 0x1000, true, true, false); // sem exec
        cpu2.setMmu(&mmu2);
        ASSERT_MSG(cpu2.run(4) == 0, "sem exec nao busca");
    }

    // Chaves MGD: usuário decide onde o MGD assume.
    {
        emu::Emulator emu;
        ASSERT_MSG(emu.switches().mgd_translation, "traducao on");
        ASSERT_MSG(emu.switches().mgd_mali == emu::MaliLevel::Cheap, "mali cheap");
        ASSERT_MSG(emu.world().cheap().resolution_factor == 0.5f, "mundo 0.5x");
        // desliga tradução: acesso direto volta a valer
        emu.switches().mgd_translation = false;
        emu.applySwitches();
        emu.cpu().setReg(2, 0x100);
        emu.cpu().setReg(0, 0x55);
        ASSERT_MSG(emu.cpu().step(0xF8000020u), "direto ok");
        // mali Edge: mundo 0.4x
        emu.switches().mgd_mali = emu::MaliLevel::Edge;
        emu.applySwitches();
        ASSERT_MSG(emu.world().cheap().resolution_factor == 0.4f, "mundo 0.4x");
        // imagem off: painter não apresenta
        emu.switches().mgd_image = false;
        ASSERT_MSG(!emu.present("off.ppm"), "imagem off nega");
        emu.switches().mgd_image = true;
        emu.bootWorld(5);
        ASSERT_MSG(emu.present("switches_boot.ppm"), "imagem on apresenta");
        ASSERT_MSG(emu.world().present("switches_boot_x1.ppm", 1), "upscale x1 ok");
    }

    // Flags + B.cond + MOVK + RET.
    {
        emu::Cpu cpu;
        cpu.setReg(0, 7);
        ASSERT_MSG(cpu.step(0xF1000FE0u), "subs xzr,x0,#7");
        ASSERT_MSG(cpu.reg(0) == 7, "subs nao escreve xzr");
        uint64_t pc = cpu.pc();
        ASSERT_MSG(cpu.step(0x54000040u), "b.eq pula (z=1)");
        ASSERT_MSG(cpu.pc() == pc + 8, "eq tomou");
        pc = cpu.pc();
        ASSERT_MSG(cpu.step(0x54000041u), "b.ne nao pula");
        ASSERT_MSG(cpu.pc() == pc + 4, "ne seguiu");
        cpu.setReg(0, 0x1234);
        ASSERT_MSG(cpu.step(0xF2B579A0u), "movk x0,#0xabcd,lsl#16");
        ASSERT_MSG(cpu.reg(0) == 0xABCD1234ull, "movk manteu baixo");
        cpu.setReg(2, 0x400);
        ASSERT_MSG(cpu.step(0xD65F0040u), "ret x2");
        ASSERT_MSG(cpu.pc() == 0x400, "pc=x2");
    }

    // BL + RET (chamada e retorno) + LDR literal.
    {
        emu::Cpu cpu;
        // pc=0: BL +8 (para 0x8); 0x4: MOVZ X0,#1; 0x8: MOVZ X0,#2; RET X30
        ASSERT_MSG(cpu.step(0x94000002u), "bl +8");
        ASSERT_MSG(cpu.pc() == 8 && cpu.reg(30) == 4, "x30=retorno");
        ASSERT_MSG(cpu.step(0xD2800040u), "movz x0,#2");
        ASSERT_MSG(cpu.reg(0) == 2, "pulou o movz #1");
        ASSERT_MSG(cpu.step(0xD65F03C0u), "ret x30");
        ASSERT_MSG(cpu.pc() == 4, "voltou");
        // LDR X1,[PC,#8]: pc=4 -> lê de 4+8=12
        emu::Cpu cpu2;
        cpu2.setPc(4);
        uint64_t blob = 0x1122334455667788ull;
        for (int i = 0; i < 8; i++) cpu2.ram()[12 + i] = static_cast<uint8_t>(blob >> (8 * i));
        ASSERT_MSG(cpu2.step(0x58000041u), "ldr x1,[pc,#8]");
        ASSERT_MSG(cpu2.reg(1) == blob, "literal certo");
        ASSERT_MSG(cpu2.pc() == 8, "pc andou");
    }

    // SP de verdade + LSL/LSR imediato.
    {
        emu::Cpu cpu;
        cpu.setSp(0x1000);
        ASSERT_MSG(cpu.step(0xD10003FFu), "sub sp,sp,#16");
        ASSERT_MSG(cpu.sp() == 0xFF0, "sp desceu");
        cpu.setReg(1, 0xFF);
        ASSERT_MSG(cpu.step(0xD378DC20u), "lsl x0,x1,#8");
        ASSERT_MSG(cpu.reg(0) == 0xFF00, "shift esq");
        cpu.setReg(1, 0xFF0);
        ASSERT_MSG(cpu.step(0xD344FC22u), "lsr x2,x1,#4");
        ASSERT_MSG(cpu.reg(2) == 0xFF, "shift dir");
    }

    // Pilha de verdade: STP pré-index + LDP pós-index + MADD.
    {
        emu::Cpu cpu;
        cpu.setSp(0x1000);
        cpu.setReg(0, 0xAA);
        cpu.setReg(1, 0xBB);
        ASSERT_MSG(cpu.step(0xA9BF07E0u), "stp x0,x1,[sp,#-16]!");
        ASSERT_MSG(cpu.sp() == 0xFF0, "sp desceu 16");
        ASSERT_MSG(cpu.step(0xA8C10FE2u), "ldp x2,x3,[sp],#16");
        ASSERT_MSG(cpu.reg(2) == 0xAA && cpu.reg(3) == 0xBB, "par da pilha");
        ASSERT_MSG(cpu.sp() == 0x1000, "sp voltou");
        cpu.setReg(1, 3);
        cpu.setReg(2, 4);
        cpu.setReg(3, 5);
        ASSERT_MSG(cpu.step(0x9B020C20u), "madd x0,x1,x2,x3");
        ASSERT_MSG(cpu.reg(0) == 17, "5+3*4");
    }

    // NOP/HINT não travam o run.
    {
        emu::Cpu cpu;
        uint64_t pc = cpu.pc();
        ASSERT_MSG(cpu.step(0xD503201Fu), "nop");
        ASSERT_MSG(cpu.pc() == pc + 4, "nop anda");
        ASSERT_MSG(cpu.step(0xD503203Fu), "hint aceita");
    }

    // AND / EOR com shift.
    {
        emu::Cpu cpu;
        cpu.setReg(1, 0xF0);
        cpu.setReg(2, 0x3C);
        ASSERT_MSG(cpu.step(0x8A020025u), "and x5,x1,x2");
        ASSERT_MSG(cpu.reg(5) == 0x30, "and certo");
        ASSERT_MSG(cpu.step(0xCA020426u), "eor x6,x1,x2,lsl#1");
        ASSERT_MSG(cpu.reg(6) == (0xF0ull ^ (0x3Cull << 1)), "eor+shift certo");
    }

    // TBZ / TBNZ.
    {
        emu::Cpu cpu;
        cpu.setReg(0, 0);
        uint64_t pc = cpu.pc();
        ASSERT_MSG(cpu.step(0x36180040u), "tbz x0,#3 pula (bit limpo)");
        ASSERT_MSG(cpu.pc() == pc + 8, "tbz tomou");
        cpu.setReg(0, 8);
        pc = cpu.pc();
        ASSERT_MSG(cpu.step(0x37180040u), "tbnz x0,#3 pula (bit set)");
        ASSERT_MSG(cpu.pc() == pc + 8, "tbnz tomou");
        pc = cpu.pc();
        ASSERT_MSG(cpu.step(0x36180040u), "tbz nao pula (bit set)");
        ASSERT_MSG(cpu.pc() == pc + 4, "tbz seguiu");
    }

    std::cout << "  Emulator machine tests passed!" << std::endl;
    return true;
}
