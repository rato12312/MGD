#include <iostream>
#include <string>
#include <vector>

#define ASSERT_MSG(cond, msg) do { if (!(cond)) { std::cerr << "FAIL: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; return false; } } while(0)

#include "emulador-mgd/cpu/Cpu.h"
#include "emulador-mgd/ram/GuestRam.h"
#include "emulador-mgd/handoff/CaptureStub.h"
#include "emulador-mgd/runtime/Emulator.h"
#include "emulador-mgd/loader/NroLoader.h"
#include "emulador-mgd/hos/Kernel.h"
#include "emulador-mgd/hos/Thread.h"
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

    // SDIV / UDIV.
    {
        emu::Cpu cpu;
        cpu.setReg(1, 20);
        cpu.setReg(2, 4);
        ASSERT_MSG(cpu.step(0x9A820C20u), "udiv x0,x1,x2");
        ASSERT_MSG(cpu.reg(0) == 5, "20/4");
        cpu.setReg(1, static_cast<uint64_t>(-20));
        ASSERT_MSG(cpu.step(0x9AC20C23u), "sdiv x3,x1,x2");
        ASSERT_MSG(cpu.reg(3) == static_cast<uint64_t>(-5), "-20/4");
        cpu.setReg(2, 0);
        ASSERT_MSG(cpu.step(0x9A820C20u), "udiv por zero");
        ASSERT_MSG(cpu.reg(0) == 0, "zero sem trap");
    }

    // ADD / SUB registrado com shift.
    {
        emu::Cpu cpu;
        cpu.setReg(1, 10);
        cpu.setReg(2, 3);
        ASSERT_MSG(cpu.step(0x8B020820u), "add x0,x1,x2,lsl#2");
        ASSERT_MSG(cpu.reg(0) == 22, "10+3*4");
        ASSERT_MSG(cpu.step(0xCB020023u), "sub x3,x1,x2");
        ASSERT_MSG(cpu.reg(3) == 7, "10-3");
    }

    // TPIDR_EL0 (TLS) + timer.
    {
        emu::Cpu cpu;
        cpu.setReg(5, 0xCAFE);
        ASSERT_MSG(cpu.step(0xD51BD0A0u), "msr tpidr_el0,x5");
        cpu.setReg(5, 0);
        ASSERT_MSG(cpu.step(0xD53BD040u), "mrs x0,tpidr_el0");
        ASSERT_MSG(cpu.reg(0) == 0xCAFE, "tls certo");
        ASSERT_MSG(cpu.step(0xD53BE040u), "mrs x0,cntvct_el0");
        ASSERT_MSG(cpu.reg(0) == 3, "timer = 3 instr");
    }

    // Barreiras + LDAXR/STLXR (trava single-thread).
    {
        emu::Cpu cpu;
        ASSERT_MSG(cpu.step(0xD5033BBFu), "dmb sy");
        cpu.setReg(1, 0x300);
        cpu.setReg(0, 0x77);
        ASSERT_MSG(cpu.step(0xC8007C22u), "stlxr w2,x0,[x1]");
        ASSERT_MSG(cpu.reg(2) == 0, "venceu");
        ASSERT_MSG(cpu.step(0xC85FFC20u), "ldaxr x0,[x1]");
        ASSERT_MSG(cpu.reg(0) == 0x77, "leu de volta");
    }

    // LDRSW estende o sinal.
    {
        emu::Cpu cpu;
        cpu.setReg(2, 0x100);
        cpu.setReg(1, 0xFFFFFFFFull);
        ASSERT_MSG(cpu.step(0xB8000041u), "strw");
        ASSERT_MSG(cpu.step(0xB8800020u), "ldrsw x0,[x1]");
        ASSERT_MSG(cpu.reg(0) == 0xFFFFFFFFFFFFFFFFull, "sinal estendido");
    }

    // Shifts registrados + PRFM.
    {
        emu::Cpu cpu;
        cpu.setReg(1, 1);
        cpu.setReg(2, 3);
        ASSERT_MSG(cpu.step(0x9AC22020u), "lslv x0,x1,x2");
        ASSERT_MSG(cpu.reg(0) == 8, "1<<3");
        cpu.setReg(1, 0x8000000000000000ull);
        cpu.setReg(2, 4);
        ASSERT_MSG(cpu.step(0x9AC22823u), "asrv x3,x1,x2");
        ASSERT_MSG(cpu.reg(3) == 0xF800000000000000ull, "asr propaga sinal");
        ASSERT_MSG(cpu.step(0xF9800000u), "prfm aceito");
    }

    // Ponto flutuante: int->double->reg->int.
    {
        emu::Cpu cpu;
        cpu.setReg(0, 42);
        ASSERT_MSG(cpu.step(0x1E660000u), "scvtf d0,x0");
        ASSERT_MSG(cpu.step(0x1E604020u), "fmov d1,d0");
        ASSERT_MSG(cpu.step(0x1E620021u), "scvtf x1,d1");
        ASSERT_MSG(cpu.reg(1) == 42, "round-trip double");
    }

    // FADD / FMUL double.
    {
        emu::Cpu cpu;
        cpu.setReg(0, 3);
        cpu.setReg(1, 4);
        ASSERT_MSG(cpu.step(0x1E660000u), "scvtf d0,x0 (3.0)");
        ASSERT_MSG(cpu.step(0x1E660021u), "scvtf d1,x1 (4.0)");
        ASSERT_MSG(cpu.step(0x1EE12002u), "fadd d2,d0,d1");
        ASSERT_MSG(cpu.step(0x1E620043u), "scvtf x3,d2");
        ASSERT_MSG(cpu.reg(3) == 7, "3+4");
        ASSERT_MSG(cpu.step(0x1EE10004u), "fmul d4,d0,d1");
        ASSERT_MSG(cpu.step(0x1E620085u), "scvtf x5,d4");
        ASSERT_MSG(cpu.reg(5) == 12, "3*4");
        ASSERT_MSG(cpu.step(0x1E612020u), "fcmp d0,d1 (3<4)");
        uint64_t pc = cpu.pc();
        ASSERT_MSG(cpu.step(0x54000044u), "b.mi pula (n=1)");
        ASSERT_MSG(cpu.pc() == pc + 8, "fp comparou");
    }

    // REV / REV32 / REV16 / CLZ.
    {
        emu::Cpu cpu;
        cpu.setReg(1, 0x0102030405060708ull);
        ASSERT_MSG(cpu.step(0xDAC00C20u), "rev x0,x1");
        ASSERT_MSG(cpu.reg(0) == 0x0807060504030201ull, "rev certo");
        ASSERT_MSG(cpu.step(0xDAC00820u), "rev32 x0,x1");
        ASSERT_MSG(cpu.reg(0) == 0x0403020108070605ull, "rev32 certo");
        ASSERT_MSG(cpu.step(0xDAC00420u), "rev16 x0,x1");
        ASSERT_MSG(cpu.reg(0) == 0x0201040306050807ull, "rev16 certo");
        cpu.setReg(1, 0x00F0000000000000ull);
        ASSERT_MSG(cpu.step(0xDAC01020u), "clz x0,x1");
        ASSERT_MSG(cpu.reg(0) == 8, "8 zeros");
        cpu.setReg(1, 0);
        ASSERT_MSG(cpu.step(0xDAC01020u), "clz zero");
        ASSERT_MSG(cpu.reg(0) == 64, "zero -> 64");
    }

    // RBIT / EXTR.
    {
        emu::Cpu cpu;
        cpu.setReg(1, 0x8000000000000001ull);
        ASSERT_MSG(cpu.step(0xDAC00020u), "rbit x0,x1");
        ASSERT_MSG(cpu.reg(0) == 0x8000000000000001ull, "palindromo");
        cpu.setReg(1, 0xF0);
        ASSERT_MSG(cpu.step(0xDAC00020u), "rbit 2");
        ASSERT_MSG(cpu.reg(0) == 0x0F00000000000000ull, "rbit certo");
        cpu.setReg(1, 0xFF00);
        cpu.setReg(2, 0x00FF);
        ASSERT_MSG(cpu.step(0xD3821020u), "extr x0,x1,x2,#4");
        ASSERT_MSG(cpu.reg(0) == 0xF000000000000FF0ull, "extr certo");
    }

    // SMADDL / UMADDL.
    {
        emu::Cpu cpu;
        cpu.setReg(1, 0xFFFFFFFEull); // -2 como W
        cpu.setReg(2, 3);
        cpu.setReg(3, 100);
        ASSERT_MSG(cpu.step(0x9B220C20u), "smaddl x0,w1,w2,x3");
        ASSERT_MSG(cpu.reg(0) == 94, "100+(-2*3)");
        cpu.setReg(1, 0xFFFFFFFFull);
        cpu.setReg(2, 2);
        ASSERT_MSG(cpu.step(0x9BA27C24u), "umaddl x4,w1,w2,xzr");
        ASSERT_MSG(cpu.reg(4) == 0x1FFFFFFFEull, "unsigned 32->64");
    }

    // Kernel HOS: heap, saída e stub honesto.
    {
        hos::Kernel k;
        hos::SvcArgs a;
        a.x[1] = 0x1000000;
        ASSERT_MSG(k.call(hos::SVC_SET_HEAP_SIZE, a) == hos::RESULT_OK, "heap ok");
        ASSERT_MSG(k.heapSize() == 0x1000000, "heap guardado");
        ASSERT_MSG(a.out[0] == k.heapBase(), "base devolvida");
        hos::SvcArgs b;
        ASSERT_MSG(k.call(hos::SVC_GET_INFO, b) == hos::RESULT_OK, "info stub ok");
        hos::SvcArgs c;
        ASSERT_MSG(k.call(hos::SVC_EXIT_PROCESS, c) == hos::RESULT_OK, "exit ok");
        ASSERT_MSG(k.exited(), "marcou saida");
        hos::SvcArgs d;
        ASSERT_MSG(k.call(0xFF, d) == hos::RESULT_UNIMPLEMENTED, "desconhecida nao trava");
    }

    // CPU chama o kernel HOS via SVC.
    {
        emu::Cpu cpu;
        hos::Kernel kernel;
        cpu.setKernel(&kernel);
        cpu.setReg(1, 0x2000000); // tamanho do heap pedido
        ASSERT_MSG(cpu.step(0xD4000021u), "svc #1 = SetHeapSize");
        ASSERT_MSG(cpu.reg(0) == kernel.heapBase(), "base do heap em x0");
        ASSERT_MSG(kernel.heapSize() == 0x2000000, "kernel guardou");
        ASSERT_MSG(cpu.step(0xD4000521u), "svc #0x29 = GetInfo");
        ASSERT_MSG(cpu.lastSvc() == hos::RESULT_OK, "info ok");
        ASSERT_MSG(cpu.step(0xD40000C1u), "svc #6 = ExitProcess");
        ASSERT_MSG(cpu.stopped(), "exit parou a cpu");
    }

    // Máquina completa: programa pede heap via SVC e sai via ExitProcess.
    {
        emu::Emulator emu;
        std::vector<uint32_t> prog = {
            0xD2800021u, // MOVZ X1, #1
            0xD4000021u, // SVC #1 = SetHeapSize(1)
            0xD40000C1u, // SVC #6 = ExitProcess
        };
        ASSERT_MSG(emu.loadProgram(prog, 0), "programa hos cabe");
        emu.runCpu(8);
        ASSERT_MSG(emu.cpu().stopped(), "exit parou");
        ASSERT_MSG(emu.kernel().exited(), "kernel marcou");
        ASSERT_MSG(emu.kernel().heapSize() == 1, "heap pedido");
        uint64_t pa = 0;
        ASSERT_MSG(emu.mmu().translate(emu.kernel().heapBase(), 1, false, false, pa), "heap mapeado");
    }

    // QueryMemory responde a partir do mapa real.
    {
        emu::Emulator emu;
        emu.switches().mgd_translation = false; // RAM direta p/ escrita do kernel
        emu.applySwitches();
        hos::SvcArgs a;
        a.x[0] = 0x100; // out no guest
        a.x[2] = 0x200; // consulta endereço mapeado (ram direta)
        // sem mmu não há mapa: mapeia via tradução ligada rapidinho
        emu.switches().mgd_translation = true;
        emu.applySwitches();
        ASSERT_MSG(emu.kernel().call(hos::SVC_QUERY_MEMORY, a) == hos::RESULT_OK, "query ok");
        uint64_t base = 0, size = 0;
        for (int i = 0; i < 8; i++) base |= static_cast<uint64_t>(emu.cpu().ram()[0x100 + i]) << (8 * i);
        for (int i = 0; i < 8; i++) size |= static_cast<uint64_t>(emu.cpu().ram()[0x108 + i]) << (8 * i);
        ASSERT_MSG(base == 0 && size == emu.cpu().ramSize(), "regiao identidade");
        hos::SvcArgs b;
        b.x[0] = 0x100;
        b.x[2] = emu.cpu().ramSize() + 0x1000; // fora de tudo
        ASSERT_MSG(emu.kernel().call(hos::SVC_QUERY_MEMORY, b) == hos::RESULT_OK, "query fora ok");
        uint32_t state = 0;
        for (int i = 0; i < 4; i++)
            state |= static_cast<uint32_t>(emu.cpu().ram()[0x110 + i]) << (8 * i);
        ASSERT_MSG(state == hos::MEM_UNMAPPED, "fora = unmapped");
    }

    // CRC32: X de 8 bytes == 8 passos B.
    {
        emu::Cpu cpu;
        cpu.setReg(1, 0);
        cpu.setReg(2, 0x0102030405060708ull);
        ASSERT_MSG(cpu.step(0x9AC20420u), "crc32x x0,x1,x2");
        uint64_t x = cpu.reg(0);
        cpu.setReg(0, 0);
        for (int i = 0; i < 8; i++) {
            cpu.setReg(1, cpu.reg(0));
            cpu.setReg(2, 0x0102030405060708ull >> (8 * i));
            uint32_t insn = 0x1AC00400u | (2u << 16) | (1u << 5) | 0u;
            ASSERT_MSG(cpu.step(insn), "crc32b passo");
        }
        ASSERT_MSG(cpu.reg(0) == x, "x == 8x b");
        ASSERT_MSG(x != 0, "crc nao trivial");
    }

    // LDR/STR com offset registrado.
    {
        emu::Cpu cpu;
        cpu.setReg(1, 0x100);
        cpu.setReg(2, 0x10);
        cpu.setReg(0, 0x99);
        ASSERT_MSG(cpu.step(0xF8226820u), "str x0,[x1,x2]");
        cpu.setReg(0, 0);
        ASSERT_MSG(cpu.step(0xF8626823u), "ldr x3,[x1,x2]");
        ASSERT_MSG(cpu.reg(3) == 0x99, "offset reg certo");
        cpu.setReg(2, 2);
        ASSERT_MSG(cpu.step(0xF8627823u), "ldr x3,[x1,x2,lsl#3]");
        ASSERT_MSG(cpu.reg(3) == 0x99, "lsl#3 certo");
    }

    // ADCS / SBCS com carry.
    {
        emu::Cpu cpu;
        cpu.setReg(0, 0);
        ASSERT_MSG(cpu.step(0xF100001Fu), "subs xzr,x0,x0 (c=1)");
        cpu.setReg(1, 10);
        cpu.setReg(2, 20);
        ASSERT_MSG(cpu.step(0xFA020023u), "sbcs x3,x1,x2");
        ASSERT_MSG(cpu.reg(3) == 0xFFFFFFFFFFFFFFF6ull, "10-20 com c=1");
        cpu.setReg(1, 10);
        cpu.setReg(2, 20);
        ASSERT_MSG(cpu.step(0xBA020020u), "adcs x0,x1,x2");
        ASSERT_MSG(cpu.reg(0) == 30, "10+20+0 (c=0 do borrow)");
    }

    // BIC / ORN.
    {
        emu::Cpu cpu;
        cpu.setReg(1, 0xFF);
        cpu.setReg(2, 0x0F);
        ASSERT_MSG(cpu.step(0x8A220020u), "bic x0,x1,x2");
        ASSERT_MSG(cpu.reg(0) == 0xF0, "limpa bits");
        ASSERT_MSG(cpu.step(0xAA220023u), "orn x3,x1,x2");
        ASSERT_MSG(cpu.reg(3) == 0xFFFFFFFFFFFFFFFFull, "ou com not");
    }

    // CSEL condicional.
    {
        emu::Cpu cpu;
        cpu.setReg(0, 5);
        ASSERT_MSG(cpu.step(0xF100001Fu), "subs z=1");
        cpu.setReg(1, 11);
        cpu.setReg(2, 22);
        ASSERT_MSG(cpu.step(0x9A820420u), "csel x0,x1,x2,eq");
        ASSERT_MSG(cpu.reg(0) == 11, "eq pega x1");
        ASSERT_MSG(cpu.step(0x9A821423u), "csel x3,x1,x2,ne");
        ASSERT_MSG(cpu.reg(3) == 22, "ne pega x2");
    }

    // FSQRT / FNEG / FABS.
    {
        emu::Cpu cpu;
        ASSERT_MSG(cpu.step(0xD2800200u), "movz x0,#16");
        ASSERT_MSG(cpu.step(0x1E660000u), "scvtf d0,x0");
        ASSERT_MSG(cpu.step(0x1E61C001u), "fsqrt d1,d0");
        ASSERT_MSG(cpu.step(0x1E620021u), "scvtf x1,d1");
        ASSERT_MSG(cpu.reg(1) == 4, "raiz de 16");
        ASSERT_MSG(cpu.step(0x1E614022u), "fneg d2,d1");
        ASSERT_MSG(cpu.step(0x1E620042u), "scvtf x2,d2");
        ASSERT_MSG(cpu.reg(2) == static_cast<uint64_t>(-4), "-4");
        ASSERT_MSG(cpu.step(0x1E60C043u), "fabs d3,d2");
        ASSERT_MSG(cpu.step(0x1E620063u), "scvtf x3,d3");
        ASSERT_MSG(cpu.reg(3) == 4, "abs volta");
    }

    // Float 32-bit: double->float->soma->double->int.
    {
        emu::Cpu cpu;
        cpu.setReg(0, 3);
        cpu.setReg(1, 4);
        ASSERT_MSG(cpu.step(0x1E660000u), "d0=3.0");
        ASSERT_MSG(cpu.step(0x1E660021u), "d1=4.0");
        ASSERT_MSG(cpu.step(0x1E624000u), "fcvt s0,d0");
        ASSERT_MSG(cpu.step(0x1E624021u), "fcvt s1,d1");
        ASSERT_MSG(cpu.step(0x1E212002u), "fadd s2,s0,s1");
        ASSERT_MSG(cpu.step(0x1E22C042u), "fcvt d2,s2");
        ASSERT_MSG(cpu.step(0x1E620043u), "scvtf x3,d2");
        ASSERT_MSG(cpu.reg(3) == 7, "float 3+4");
    }

    // LDR/STR double bit-exato.
    {
        emu::Cpu cpu;
        cpu.setReg(0, 7);
        ASSERT_MSG(cpu.step(0x1E660000u), "d0=7.0");
        cpu.setReg(1, 0x100);
        ASSERT_MSG(cpu.step(0xFD000020u), "str d0,[x1]");
        ASSERT_MSG(cpu.step(0x1E6043E0u), "fmov d0,d31 (limpa)");
        cpu.setReg(1, 0x100);
        ASSERT_MSG(cpu.step(0xFD400022u), "ldr d2,[x1]");
        ASSERT_MSG(cpu.step(0x1E620043u), "scvtf x3,d2");
        ASSERT_MSG(cpu.reg(3) == 7, "double voltou");
    }

    // STP / LDP de par double.
    {
        emu::Cpu cpu;
        cpu.setReg(0, 5);
        ASSERT_MSG(cpu.step(0x1E660000u), "d0=5.0");
        ASSERT_MSG(cpu.step(0x1E604008u), "fmov d8,d0");
        cpu.setReg(0, 9);
        ASSERT_MSG(cpu.step(0x1E660000u), "d0=9.0");
        ASSERT_MSG(cpu.step(0x1E604009u), "fmov d9,d0");
        cpu.setReg(0, 0x300);
        ASSERT_MSG(cpu.step(0x6D012408u), "stp d8,d9,[x0,#16]");
        ASSERT_MSG(cpu.step(0x6D412C0Au), "ldp d10,d11,[x0,#16]");
        ASSERT_MSG(cpu.step(0x1E62014Bu), "scvtf x11,d10");
        ASSERT_MSG(cpu.reg(11) == 5, "d10=5.0");
        ASSERT_MSG(cpu.step(0x1E62016Bu), "scvtf x11,d11");
        ASSERT_MSG(cpu.reg(11) == 9, "d11=9.0");
    }

    // FMAX / FMIN.
    {
        emu::Cpu cpu;
        cpu.setReg(0, 3);
        cpu.setReg(1, 7);
        ASSERT_MSG(cpu.step(0x1E660000u), "d0=3.0");
        ASSERT_MSG(cpu.step(0x1E660021u), "d1=7.0");
        ASSERT_MSG(cpu.step(0x1EE14002u), "fmax d2,d0,d1");
        ASSERT_MSG(cpu.step(0x1E620043u), "scvtf x3,d2");
        ASSERT_MSG(cpu.reg(3) == 7, "max=7");
        ASSERT_MSG(cpu.step(0x1EE15004u), "fmin d4,d0,d1");
        ASSERT_MSG(cpu.step(0x1E620085u), "scvtf x5,d4");
        ASSERT_MSG(cpu.reg(5) == 3, "min=3");
    }

    // FMADD.
    {
        emu::Cpu cpu;
        cpu.setReg(0, 2);
        cpu.setReg(1, 3);
        cpu.setReg(2, 4);
        ASSERT_MSG(cpu.step(0x1E660000u), "d0=2.0");
        ASSERT_MSG(cpu.step(0x1E660021u), "d1=3.0");
        ASSERT_MSG(cpu.step(0x1E660042u), "d2=4.0");
        ASSERT_MSG(cpu.step(0x1FE10803u), "fmadd d3,d0,d1,d2");
        ASSERT_MSG(cpu.step(0x1E620063u), "scvtf x3,d3");
        ASSERT_MSG(cpu.reg(3) == 10, "2*3+4");
    }

    // FCSEL com FCMP.
    {
        emu::Cpu cpu;
        cpu.setReg(0, 3);
        cpu.setReg(1, 4);
        ASSERT_MSG(cpu.step(0x1E660000u), "d0=3.0");
        ASSERT_MSG(cpu.step(0x1E660021u), "d1=4.0");
        ASSERT_MSG(cpu.step(0x1E612020u), "fcmp d0,d1");
        ASSERT_MSG(cpu.step(0x1E614402u), "fcsel d2,d0,d1,mi");
        ASSERT_MSG(cpu.step(0x1E620043u), "scvtf x3,d2");
        ASSERT_MSG(cpu.reg(3) == 3, "mi pega menor");
        ASSERT_MSG(cpu.step(0x1E615403u), "fcsel d3,d0,d1,pl");
        ASSERT_MSG(cpu.step(0x1E620063u), "scvtf x3,d3");
        ASSERT_MSG(cpu.reg(3) == 4, "pl pega maior");
    }

    // ADDW/SUBW 32-bit com wrap.
    {
        emu::Cpu cpu;
        cpu.setReg(0, 0xFFFFFFFFull);
        ASSERT_MSG(cpu.step(0x11000400u), "addw w0,w0,#1");
        ASSERT_MSG(cpu.reg(0) == 0, "wrap 32-bit");
        cpu.setReg(1, 5);
        ASSERT_MSG(cpu.step(0x51002021u), "subw w1,w1,#8");
        ASSERT_MSG(cpu.reg(1) == 0xFFFFFFFDull, "5-8 wrap zero-extend");
    }

    // MOVN + MOVZ 32-bit.
    {
        emu::Cpu cpu;
        ASSERT_MSG(cpu.step(0x92800000u), "movn x0,#0");
        ASSERT_MSG(cpu.reg(0) == 0xFFFFFFFFFFFFFFFFull, "~0");
        ASSERT_MSG(cpu.step(0x52A00021u), "movz w1,#1,lsl#16");
        ASSERT_MSG(cpu.reg(1) == 0x10000, "w certo");
    }

    // ALU registrada 32-bit.
    {
        emu::Cpu cpu;
        cpu.setReg(1, 3);
        cpu.setReg(2, 4);
        ASSERT_MSG(cpu.step(0x0B020420u), "addw w0,w1,w2,lsl#1");
        ASSERT_MSG(cpu.reg(0) == 11, "3+4*2");
        cpu.setReg(1, 0xF0);
        cpu.setReg(2, 0x3C);
        ASSERT_MSG(cpu.step(0x0A020023u), "andw w3,w1,w2");
        ASSERT_MSG(cpu.reg(3) == 0x30, "and 32");
    }

    // MADDW + SDIVW.
    {
        emu::Cpu cpu;
        cpu.setReg(1, 3);
        cpu.setReg(2, 4);
        cpu.setReg(3, 5);
        ASSERT_MSG(cpu.step(0x1B020C20u), "maddw w0,w1,w2,w3");
        ASSERT_MSG(cpu.reg(0) == 17, "5+3*4");
        cpu.setReg(1, 20);
        cpu.setReg(2, 4);
        ASSERT_MSG(cpu.step(0x1AC20C20u), "sdivw w0,w1,w2");
        ASSERT_MSG(cpu.reg(0) == 5, "20/4");
    }

    // Sistema inteiro: NRO pede heap, soma em FP, salva na pilha, sai, mundo pinta.
    {
        // .text: MOVZ X1,#16 | SVC#1 SetHeapSize | SCVTF D0,X1 | FMOV D1,D0 |
        //        FADD D2,D0,D1 | STP X0,X1,[SP,#-16]! | LDP X3,X4,[SP],#16 |
        //        SVC#6 ExitProcess
        std::vector<uint32_t> text = {
            0xD2800201u, // MOVZ X1, #16
            0xD4000021u, // SVC #1 SetHeapSize(16)
            0x1E660020u, // SCVTF D0, X1 (16.0)
            0x1E604001u, // FMOV D1, D0
            0x1EE12002u, // FADD D2, D0, D1 (32.0)
            0xA9BF07E0u, // STP X0, X1, [SP, #-16]!
            0xA8C10FE2u, // LDP X2, X3, [SP], #16
            0xD40000C1u, // SVC #6 ExitProcess
        };
        std::vector<uint8_t> blob(0x80 + text.size() * 4, 0);
        blob[0x10] = 'N'; blob[0x11] = 'R'; blob[0x12] = 'O'; blob[0x13] = '0';
        blob[0x20] = 0x80; // text em 0x80
        blob[0x24] = static_cast<uint8_t>(text.size() * 4);
        for (size_t i = 0; i < text.size(); ++i)
            for (int b = 0; b < 4; b++)
                blob[0x80 + i * 4 + b] = static_cast<uint8_t>(text[i] >> (8 * b));
        emu::Emulator emu;
        emu::NroImage img = emu::parseNro(blob.data(), blob.size());
        ASSERT_MSG(img.valid, "nro sistema valido");
        uint64_t entry = 0;
        ASSERT_MSG(emu::loadNroInto(img, blob.data(), emu.cpu().ram(), emu.cpu().ramSize(), 0, entry),
                   "nro mapeado");
        emu.cpu().setSp(0x8000);
        emu.cpu().setPc(entry + 0x80);
        emu.runCpu(64);
        ASSERT_MSG(emu.cpu().stopped(), "sistema parou limpo");
        ASSERT_MSG(emu.kernel().heapSize() == 16, "heap 16");
        ASSERT_MSG(emu.cpu().reg(2) == 0 && emu.cpu().reg(3) == 16, "pilha round-trip");
        ASSERT_MSG(emu.cpu().sp() == 0x8000, "sp voltou");
        bridge::RuntimeFrameStats s = emu.bootWorld(8);
        ASSERT_MSG(s.polygons_fed == 8 && s.pixels_written > 0, "mundo pintou");
        ASSERT_MSG(emu.present("sistema_boot.ppm"), "sistema apresenta");
    }

    // Duas threads intercalam contadores (round-robin).
    {
        emu::Cpu cpu;
        // thread A em 0x0: loop X0++ 3x e SVC#0; thread B em 0x40: loop X1++ 2x e SVC#0
        auto poke = [&](uint64_t addr, uint32_t insn) {
            for (int i = 0; i < 4; i++)
                cpu.ram()[addr + i] = static_cast<uint8_t>(insn >> (8 * i));
        };
        // A: X0=3, grava na RAM e sai ; B: X1=2 e sai
        poke(0x00, 0x91000400u);
        poke(0x04, 0x91000400u);
        poke(0x08, 0x91000400u);
        poke(0x0C, 0xD2804002u); // MOVZ X2, #0x200
        poke(0x10, 0xF8000040u); // STR X0, [X2]
        poke(0x14, 0xD4000001u); // SVC#0
        poke(0x40, 0x91000421u);
        poke(0x44, 0x91000421u);
        poke(0x48, 0xD4000001u);
        hos::Scheduler sched;
        ASSERT_MSG(sched.spawn(0x0, 0x8000) == 1, "thread 1");
        ASSERT_MSG(sched.spawn(0x40, 0x9000) == 2, "thread 2");
        uint64_t done = sched.run(cpu, 64, 2);
        ASSERT_MSG(done == 9, "9 instr no total");
        ASSERT_MSG(sched.pending() == 0, "fila esvaziou");
        uint64_t ramv = 0;
        for (int i = 0; i < 8; i++)
            ramv |= static_cast<uint64_t>(cpu.ram()[0x200 + i]) << (8 * i);
        ASSERT_MSG(ramv == 3, "A contou 3");
        ASSERT_MSG(cpu.reg(1) == 2, "B contou 2");
    }

    // CreateThread via SVC cria thread executável.
    {
        emu::Cpu cpu;
        hos::Kernel kernel;
        cpu.setSvcHost(&kernel);
        cpu.setReg(1, 0x40); // entry
        cpu.setReg(2, 0x9000); // sp
        // programa da thread em 0x40: ADD X0,X0,#1 ; SVC#0
        cpu.ram()[0x40] = 0x00; cpu.ram()[0x41] = 0x04;
        cpu.ram()[0x42] = 0x00; cpu.ram()[0x43] = 0x91;
        cpu.ram()[0x44] = 0x01; cpu.ram()[0x45] = 0x00;
        cpu.ram()[0x46] = 0x00; cpu.ram()[0x47] = 0xD4;
        ASSERT_MSG(cpu.step(0xD4000101u), "svc #8 = CreateThread");
        ASSERT_MSG(cpu.reg(0) == 1, "id da thread em x0");
        uint64_t done = kernel.runThreads(cpu, 16, 4);
        ASSERT_MSG(done == 2, "thread rodou 2");
        ASSERT_MSG(kernel.scheduler().pending() == 0, "thread saiu");
        ASSERT_MSG(cpu.reg(0) == 1, "x0=1 na thread");
    }

    // SetMemoryPermission troca e o acesso obedece.
    {
        emu::Emulator emu; // tradução ligada: identidade 0..64K rw-
        emu.cpu().setReg(0, 0);
        emu.cpu().setReg(1, emu.cpu().ramSize());
        emu.cpu().setReg(2, 1); // r--
        ASSERT_MSG(emu.cpu().step(0xD4000041u), "svc #2 = SetMemoryPermission");
        ASSERT_MSG(emu.cpu().lastSvc() == hos::RESULT_OK, "trocou");
        emu.cpu().setReg(2, 0x100);
        emu.cpu().setReg(0, 0x77);
        ASSERT_MSG(!emu.cpu().step(0xF8000020u), "str sem w nega");
        ASSERT_MSG(emu.cpu().step(0xF9400020u), "ldr sem w passa");
        ASSERT_MSG(emu.cpu().reg(0) == 0, "leu zero");
    }

    std::cout << "  Emulator machine tests passed!" << std::endl;
    return true;
}
