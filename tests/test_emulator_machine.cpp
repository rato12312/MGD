#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#define ASSERT_MSG(cond, msg) do { if (!(cond)) { std::cerr << "FAIL: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; return false; } } while(0)

#include "emulador-mgd/cpu/Cpu.h"
#include "emulador-mgd/ram/GuestRam.h"
#include "emulador-mgd/handoff/CaptureStub.h"
#include "emulador-mgd/runtime/Emulator.h"
#include "emulador-mgd/loader/Lz4.h"
#include "emulador-mgd/loader/NsoLoader.h"
#include "emulador-mgd/loader/Aes.h"
#include "emulador-mgd/loader/NcaProbe.h"
#include "emulador-mgd/loader/Sha256.h"
#include "emulador-mgd/loader/Pfs0.h"
#include "emulador-mgd/loader/RomFs.h"
#include "emulador-mgd/loader/NroLoader.h"
#include "emulador-mgd/hos/Kernel.h"
#include "emulador-mgd/hos/Session.h"
#include "emulador-mgd/hos/PortRegistry.h"
#include "emulador-mgd/hos/NvService.h"
#include "emulador-mgd/hos/ViService.h"
#include "emulador-mgd/hos/AudService.h"
#include "emulador-mgd/hos/ApmService.h"
#include "emulador-mgd/hos/FatalService.h"
#include "emulador-mgd/hos/FsService.h"
#include "emulador-mgd/hos/Event.h"
#include "emulador-mgd/hos/Mutex.h"
#include "emulador-mgd/hos/HidService.h"
#include "emulador-mgd/hos/LblService.h"
#include "emulador-mgd/hos/PmService.h"
#include "emulador-mgd/hos/PsmService.h"
#include "emulador-mgd/hos/SetService.h"
#include "emulador-mgd/hos/TimeService.h"
#include "emulador-mgd/hos/ServiceManager.h"
#include "emulador-mgd/hos/Thread.h"

static void poke32(emu::Cpu& cpu, uint64_t addr, uint32_t insn) {
    for (int i = 0; i < 4; i++)
        cpu.ram()[addr + i] = static_cast<uint8_t>(insn >> (8 * i));
}
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
        ASSERT_MSG(cpu.step(0x39000441u), "strb x1,[x2,#1]");
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
        ASSERT_MSG(cpu.step(0xD40000E1u), "svc #7 = ExitProcess");
        ASSERT_MSG(cpu.stopped(), "exit parou a cpu");
    }

    // Máquina completa: programa pede heap via SVC e sai via ExitProcess.
    {
        emu::Emulator emu;
        std::vector<uint32_t> prog = {
            0xD2800021u, // MOVZ X1, #1
            0xD4000021u, // SVC #1 = SetHeapSize(1)
            0xD40000E1u, // SVC #7 = ExitProcess
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
            0xD40000E1u, // SVC #7 ExitProcess
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

    // Map/Unmap: alias lê o mesmo físico; unmap derruba.
    {
        emu::Emulator emu; // identidade 0..64K
        // escreve marcador em [0x10]
        emu.cpu().setReg(0, 0x10);
        emu.cpu().setReg(1, 0xABCD);
        ASSERT_MSG(emu.cpu().step(0xF8000001u), "str x1,[x0]");
        // MapMemory(dst=0x2000, src=0x0, size=0x1000): SVC #4
        hos::SvcArgs m;
        m.x[0] = 0x2000;
        m.x[1] = 0x0;
        m.x[2] = 0x1000;
        ASSERT_MSG(emu.kernel().call(hos::SVC_MAP_MEMORY, m) == hos::RESULT_OK, "map ok");
        emu.cpu().setReg(2, 0x2010);
        ASSERT_MSG(emu.cpu().step(0xF9400040u), "ldr x0,[x2] via alias");
        ASSERT_MSG(emu.cpu().reg(0) == 0xABCD, "alias le fisico");
        // UnmapMemory: SVC #5
        hos::SvcArgs u;
        u.x[0] = 0x2000;
        u.x[1] = 0x1000;
        ASSERT_MSG(emu.kernel().call(hos::SVC_UNMAP_MEMORY, u) == hos::RESULT_OK, "unmap ok");
        ASSERT_MSG(!emu.cpu().step(0xF9400040u), "alias caiu");
    }

    // SleepThread volta OK e acumula.
    {
        emu::Cpu cpu;
        hos::Kernel kernel;
        cpu.setSvcHost(&kernel);
        cpu.setReg(0, 1000000);
        ASSERT_MSG(cpu.step(0xD4000161u), "svc #0xB = SleepThread");
        ASSERT_MSG(kernel.sleptNs() == 1000000, "somou ns");
    }

    // CCMN/CCMP.
    {
        emu::Cpu cpu;
        cpu.setReg(0, 5);
        cpu.setReg(1, 5);
        ASSERT_MSG(cpu.step(0xBAC1E000u), "ccmp x0,x1,#0,al");
        uint64_t pc = cpu.pc();
        ASSERT_MSG(cpu.step(0x54000040u), "b.eq (z=1)");
        ASSERT_MSG(cpu.pc() == pc + 8, "comparou igual");
        ASSERT_MSG(cpu.step(0xBAC11008u), "ccmp x0,x1,#8,ne (falso, injeta n)");
        pc = cpu.pc();
        ASSERT_MSG(cpu.step(0x54000044u), "b.mi (n=1)");
        ASSERT_MSG(cpu.pc() == pc + 8, "injetou flags");
    }

    // STLR / LDAXR.
    {
        emu::Cpu cpu;
        cpu.setReg(1, 0x100);
        cpu.setReg(0, 0x5A);
        ASSERT_MSG(cpu.step(0xC800FC20u), "stlr x0,[x1]");
        ASSERT_MSG(cpu.step(0xC85F7C22u), "ldaxr x2,[x1]");
        ASSERT_MSG(cpu.reg(2) == 0x5A, "aquire certo");
    }

    // SWPAL troca.
    {
        emu::Cpu cpu;
        cpu.setReg(2, 0x100);
        cpu.setReg(1, 0xAA);
        cpu.setReg(0, 0xBB);
        ASSERT_MSG(cpu.step(0xF8000041u), "str x1,[x2] base");
        cpu.setReg(1, 0xCC);
        ASSERT_MSG(cpu.step(0xC8ECFC40u), "swpal x0,x1,[x2]");
        ASSERT_MSG(cpu.reg(0) == 0xAA, "velho em x0");
        ASSERT_MSG(cpu.step(0xF9400041u), "ldr x1,[x2]");
        ASSERT_MSG(cpu.reg(1) == 0xCC, "novo na ram");
    }

    // Pilha FP: STP pré + LDP pós de par double.
    {
        emu::Cpu cpu;
        cpu.setSp(0x1000);
        cpu.setReg(0, 5);
        ASSERT_MSG(cpu.step(0x1E660000u), "d8? d0=5.0");
        ASSERT_MSG(cpu.step(0x1E604008u), "fmov d8,d0");
        cpu.setReg(0, 9);
        ASSERT_MSG(cpu.step(0x1E660000u), "d0=9.0");
        ASSERT_MSG(cpu.step(0x1E604009u), "fmov d9,d0");
        ASSERT_MSG(cpu.step(0x6DBF27E8u), "stp d8,d9,[sp,#-16]!");
        ASSERT_MSG(cpu.sp() == 0xFF0, "sp desceu");
        ASSERT_MSG(cpu.step(0x6CC12FEAu), "ldp d10,d11,[sp],#16");
        ASSERT_MSG(cpu.sp() == 0x1000, "sp voltou");
        ASSERT_MSG(cpu.step(0x1E62014Bu), "scvtf x11,d10");
        ASSERT_MSG(cpu.reg(11) == 5, "fp da pilha 5");
        ASSERT_MSG(cpu.step(0x1E62016Bu), "scvtf x11,d11");
        ASSERT_MSG(cpu.reg(11) == 9, "fp da pilha 9");
    }

    // Desconhecido é registrado, não sumido.
    {
        emu::Cpu cpu;
        ASSERT_MSG(!cpu.step(0xFFFFFFFFu), "desconhecido nega");
        ASSERT_MSG(cpu.unknownCount() == 1, "contou 1");
        ASSERT_MSG(cpu.lastUnknown(0) == 0xFFFFFFFFu, "guardou opcode");
        ASSERT_MSG(!cpu.step(0x12345678u), "outro nega");
        ASSERT_MSG(cpu.unknownCount() == 2, "contou 2");
    }

    // STP / LDP de par 32-bit.
    {
        emu::Cpu cpu;
        cpu.setReg(0, 0xAAAAAAAAull);
        cpu.setReg(1, 0xBBBBBBBBull);
        cpu.setReg(2, 0x100);
        ASSERT_MSG(cpu.step(0x29010440u), "stp w0,w1,[x2,#8]");
        ASSERT_MSG(cpu.step(0x29411443u), "ldp w3,w4,[x2,#8]");
        ASSERT_MSG(cpu.reg(3) == 0xAAAAAAAAull && cpu.reg(4) == 0xBBBBBBBBull, "par w certo");
    }

    // SXTB / SXTH / SXTW.
    {
        emu::Cpu cpu;
        cpu.setReg(1, 0xFF);
        ASSERT_MSG(cpu.step(0x93401C20u), "sxtb x0,x1");
        ASSERT_MSG(cpu.reg(0) == 0xFFFFFFFFFFFFFFFFull, "byte->-1");
        cpu.setReg(1, 0xFFFF);
        ASSERT_MSG(cpu.step(0x93403C22u), "sxth x2,x1");
        ASSERT_MSG(cpu.reg(2) == 0xFFFFFFFFFFFFFFFFull, "half->-1");
        cpu.setReg(1, 0xFFFFFFFFull);
        ASSERT_MSG(cpu.step(0x93407C23u), "sxtw x3,x1");
        ASSERT_MSG(cpu.reg(3) == 0xFFFFFFFFFFFFFFFFull, "word->-1");
        cpu.setReg(1, 0x7F);
        ASSERT_MSG(cpu.step(0x93401C20u), "sxtb positivo");
        ASSERT_MSG(cpu.reg(0) == 0x7F, "mantem");
    }

    // UBFM 32-bit.
    {
        emu::Cpu cpu;
        cpu.setReg(1, 0xFF);
        ASSERT_MSG(cpu.step(0x53185C20u), "lsl w0,w1,#8");
        ASSERT_MSG(cpu.reg(0) == 0xFF00, "lsl w");
        cpu.setReg(1, 0xFF0);
        ASSERT_MSG(cpu.step(0x53047C22u), "lsr w2,w1,#4");
        ASSERT_MSG(cpu.reg(2) == 0xFF, "lsr w");
    }

    // Fibonacci(10)=55: loop, branch, flags, call nada — só CPU.
    {
        emu::Cpu cpu;
        std::vector<uint32_t> prog = {
            0xD2800140u, // MOVZ X0, #10
            0xD2800020u, // MOVZ X1, #0
            0xD2800022u, // MOVZ X2, #1
            0xB40000C0u, // CBZ X0, done
            0xF1000400u, // SUBS X0, X0, #1
            0x8B020023u, // ADD X3, X1, X2
            0xAA0203E1u, // ORR X1, XZR, X2
            0xAA0303E2u, // ORR X2, XZR, X3
            0x17FFFFFBu, // B loop
            0xAA0103E0u, // done: ORR X0, XZR, X1
            0xD4000001u, // SVC #0
        };
        for (size_t i = 0; i < prog.size(); ++i)
            for (int b = 0; b < 4; b++)
                cpu.ram()[i * 4 + b] = static_cast<uint8_t>(prog[i] >> (8 * b));
        uint64_t done = cpu.run(256);
        ASSERT_MSG(cpu.stopped(), "fib parou limpo");
        ASSERT_MSG(cpu.reg(0) == 55, "fib(10)=55");
        ASSERT_MSG(done == 66, "3 movz + 10x6 loop + cbz+orr+svc");
    }

    // Fatorial(5)=120 recursivo: frame de pilha + BL + RET.
    {
        emu::Cpu cpu;
        auto poke = [&](uint64_t addr, uint32_t insn) {
            for (int i = 0; i < 4; i++)
                cpu.ram()[addr + i] = static_cast<uint8_t>(insn >> (8 * i));
        };
        poke(0x00, 0xD28000A0u); // MOVZ X0, #5
        poke(0x04, 0x94000007u); // BL fact
        poke(0x08, 0xD4000001u); // SVC#0
        poke(0x20, 0xA9BF7BFDu); // STP X29,X30,[SP,#-16]!
        poke(0x24, 0x910003FDu); // ADD X29,SP,#0
        poke(0x28, 0xF100041Fu); // SUBS XZR,X0,#1
        poke(0x2C, 0x14000006u); // B.EQ base
        poke(0x30, 0xAA0003E1u); // ORR X1,XZR,X0
        poke(0x34, 0xD1000400u); // SUB X0,X0,#1
        poke(0x38, 0x97FFFFFAu); // BL fact
        poke(0x3Cu, 0x9B01FC00u); // MUL X0,X0,X1
        poke(0x40, 0x14000002u); // B end
        poke(0x44, 0xD2800020u); // base: MOVZ X0,#1
        poke(0x48, 0xA8C17BFDu); // LDP X29,X30,[SP],#16
        poke(0x4C, 0xD65F03C0u); // RET
        cpu.setSp(0x8000);
        uint64_t done = cpu.run(512);
        ASSERT_MSG(cpu.stopped(), "fat parou limpo");
        ASSERT_MSG(cpu.reg(0) == 120, "fat(5)=120");
        ASSERT_MSG(cpu.sp() == 0x8000, "pilha zerada");
        (void)done;
    }

    // Main cria worker; worker aloca heap e grava marcador.
    {
        emu::Cpu cpu;
        hos::Kernel kernel;
        cpu.setSvcHost(&kernel);
        // main @0: X1=0x40 entry, X2=0x9000 sp, CreateThread, SVC#0
        poke32(cpu, 0x00, 0xD2800801u); // MOVZ X1, #0x40
        poke32(cpu, 0x04, 0xD2920002u); // MOVZ X2, #0x9000
        poke32(cpu, 0x08, 0xD4000101u); // SVC #8 CreateThread
        poke32(cpu, 0x0C, 0xD4000001u); // SVC#0
        // worker @0x40: heap=64, marcador 0xB em [0x100], SVC#0
        poke32(cpu, 0x40, 0xD2800801u); // MOVZ X1, #64
        poke32(cpu, 0x44, 0xD4000021u); // SVC #1 SetHeapSize
        poke32(cpu, 0x48, 0xD2802000u); // MOVZ X0, #0x100
        poke32(cpu, 0x4C, 0xD2800162u); // MOVZ X2, #0xB
        poke32(cpu, 0x50, 0xF8000020u); // STR X2, [X0]
        poke32(cpu, 0x54, 0xD4000001u); // SVC#0
        cpu.setSp(0x8000);
        kernel.setRam(cpu.ram(), cpu.ramSize());
        cpu.setPc(0);
        uint64_t d1 = cpu.run(8);
        ASSERT_MSG(d1 == 4, "main criou e saiu");
        ASSERT_MSG(kernel.scheduler().pending() == 1, "worker na fila");
        uint64_t d2 = kernel.runThreads(cpu, 32, 8);
        ASSERT_MSG(d2 == 6, "worker rodou 6");
        ASSERT_MSG(kernel.heapSize() == 64, "heap 64");
        ASSERT_MSG(kernel.scheduler().pending() == 0, "fila vazia");
        uint64_t mark = 0;
        for (int i = 0; i < 8; i++)
            mark |= static_cast<uint64_t>(cpu.ram()[0x100 + i]) << (8 * i);
        ASSERT_MSG(mark == 0xB, "marcador do worker");
    }

    // Handles: cria, lê, fecha, inválido nega.
    {
        hos::Kernel kernel;
        uint32_t h1 = kernel.createHandle(0xA);
        uint32_t h2 = kernel.createHandle(0xB);
        ASSERT_MSG(h1 != h2, "handles únicos");
        uint32_t tag = 0;
        ASSERT_MSG(kernel.getHandle(h1, tag) && tag == 0xA, "etiqueta certa");
        ASSERT_MSG(kernel.closeHandle(h1), "fechou");
        ASSERT_MSG(!kernel.getHandle(h1, tag), "fechado nega");
        ASSERT_MSG(!kernel.closeHandle(0xFFFF), "inexistente nega");
        ASSERT_MSG(kernel.handleCount() == 1, "resta 1");
    }

    // Sessão IPC: pedido e resposta nos dois sentidos.
    {
        hos::Session s;
        hos::IpcMessage req;
        req.cmd = 1;
        req.payload = {10, 20, 30};
        ASSERT_MSG(s.sendRequest(req), "pedido foi");
        ASSERT_MSG(s.pendingRequests() == 1, "1 pendente");
        hos::IpcMessage got;
        ASSERT_MSG(s.recvRequest(got), "servidor leu");
        ASSERT_MSG(got.cmd == 1 && got.payload.size() == 3, "pedido intacto");
        hos::IpcMessage rep;
        rep.cmd = 2;
        ASSERT_MSG(s.sendReply(rep), "resposta foi");
        hos::IpcMessage back;
        ASSERT_MSG(s.recvReply(back) && back.cmd == 2, "cliente leu");
        ASSERT_MSG(!s.recvReply(back), "fila vazia nega");
    }

    // Porta nomeada vira sessão ligada.
    {
        hos::PortRegistry ports;
        ASSERT_MSG(ports.registerPort("sm:"), "registrou sm:");
        ASSERT_MSG(!ports.registerPort("sm:"), "duplicada nega");
        std::shared_ptr<hos::Session> cli, srv;
        ASSERT_MSG(ports.connect("sm:", cli, srv), "conectou");
        std::shared_ptr<hos::Session> bad_c, bad_s;
        ASSERT_MSG(!ports.connect("nope:", bad_c, bad_s), "inexistente nega");
        hos::IpcMessage req;
        req.cmd = 99;
        ASSERT_MSG(cli->sendRequest(req), "cliente pede");
        hos::IpcMessage got;
        ASSERT_MSG(srv->recvRequest(got) && got.cmd == 99, "servidor recebe");
    }

    // sm: GetService entrega sessão do serviço publicado.
    {
        hos::ServiceManager sm;
        ASSERT_MSG(sm.publish("nvdrv:a"), "publicou nvdrv");
        hos::IpcMessage req;
        req.cmd = 1;
        const char* nm = "nvdrv:a";
        req.payload = std::vector<uint8_t>(nm, nm + 7);
        hos::IpcMessage rep;
        ASSERT_MSG(sm.dispatch(req, rep), "entendeu cmd 1");
        ASSERT_MSG(rep.cmd == 1 && rep.payload.size() == 4, "devolveu id");
        uint32_t id = static_cast<uint32_t>(rep.payload[0]) |
                      (static_cast<uint32_t>(rep.payload[1]) << 8) |
                      (static_cast<uint32_t>(rep.payload[2]) << 16) |
                      (static_cast<uint32_t>(rep.payload[3]) << 24);
        std::shared_ptr<hos::Session> cli;
        ASSERT_MSG(sm.session(id, cli), "sessao existe");
        hos::IpcMessage bad;
        bad.cmd = 1;
        const char* bn = "nope:x";
        bad.payload = std::vector<uint8_t>(bn, bn + 6);
        hos::IpcMessage badrep;
        ASSERT_MSG(sm.dispatch(bad, badrep) && badrep.cmd == 0, "inexistente = 0");
        hos::IpcMessage weird;
        weird.cmd = 77;
        hos::IpcMessage wrep;
        ASSERT_MSG(!sm.dispatch(weird, wrep), "cmd estranho nega");
    }

    // Boot publica serviços e GetService acha a GPU.
    {
        hos::Kernel kernel;
        kernel.bootServices();
        hos::IpcMessage req;
        req.cmd = 1;
        const char* nm = "nvdrv:a";
        req.payload = std::vector<uint8_t>(nm, nm + 7);
        hos::IpcMessage rep;
        ASSERT_MSG(kernel.services().dispatch(req, rep) && rep.cmd == 1, "gpu achada");
    }

    // nvdrv abre e fecha canal.
    {
        hos::NvService nv;
        hos::IpcMessage open;
        open.cmd = 1;
        open.payload = {0x3D}; // /dev/nvhost-ctrl-gpu (etiqueta nossa)
        hos::IpcMessage opened;
        ASSERT_MSG(nv.dispatch(open, opened) && opened.cmd == 1, "canal abriu");
        ASSERT_MSG(nv.channelCount() == 1, "1 canal");
        // submit enfileira e devolve fence
        hos::IpcMessage sub;
        sub.cmd = 3;
        sub.payload = opened.payload;
        sub.payload.insert(sub.payload.end(), {0xDE, 0xAD, 0xBE, 0xEF});
        hos::IpcMessage subrep;
        ASSERT_MSG(nv.dispatch(sub, subrep) && subrep.cmd == 1, "submit ok");
        ASSERT_MSG(nv.pendingCount() == 1, "1 pendente");
        hos::IpcMessage q;
        q.cmd = 4;
        q.payload = subrep.payload;
        hos::IpcMessage qrep;
        ASSERT_MSG(nv.dispatch(q, qrep) && qrep.cmd == 0, "fence ainda na fila");
        uint32_t f = static_cast<uint32_t>(subrep.payload[0]) |
                     (static_cast<uint32_t>(subrep.payload[1]) << 8) |
                     (static_cast<uint32_t>(subrep.payload[2]) << 16) |
                     (static_cast<uint32_t>(subrep.payload[3]) << 24);
        nv.completeUpTo(f);
        ASSERT_MSG(nv.pendingCount() == 0, "fila andou");
        ASSERT_MSG(nv.dispatch(q, qrep) && qrep.cmd == 1, "fence pronto");
        // dreno nulo: 3 submits, drena 2, resta 1
        for (int i = 0; i < 3; i++) {
            hos::IpcMessage s;
            s.cmd = 3;
            s.payload = opened.payload;
            hos::IpcMessage sr;
            ASSERT_MSG(nv.dispatch(s, sr) && sr.cmd == 1, "submit fila");
        }
        ASSERT_MSG(nv.drain(2) == 2, "drenou 2");
        ASSERT_MSG(nv.pendingCount() == 1, "resta 1");
        ASSERT_MSG(nv.drain(8) == 1, "drena o resto");
        hos::IpcMessage close;
        close.cmd = 2;
        close.payload = opened.payload;
        hos::IpcMessage closed;
        ASSERT_MSG(nv.dispatch(close, closed) && closed.cmd == 1, "canal fechou");
        ASSERT_MSG(nv.channelCount() == 0, "0 canais");
        hos::IpcMessage ioctl;
        ioctl.cmd = 99;
        hos::IpcMessage irep;
        ASSERT_MSG(!nv.dispatch(ioctl, irep), "ioctl futuro nega");

    // Bomba: pedido na sessão chega na GPU sozinho.
    {
        hos::Kernel kernel;
        kernel.bootServices();
        hos::IpcMessage get;
        get.cmd = 1;
        const char* nm = "nvdrv:a";
        get.payload = std::vector<uint8_t>(nm, nm + 7);
        hos::IpcMessage got;
        ASSERT_MSG(kernel.services().dispatch(get, got) && got.cmd == 1, "sessao gpu");
        uint32_t id = static_cast<uint32_t>(got.payload[0]) |
                      (static_cast<uint32_t>(got.payload[1]) << 8) |
                      (static_cast<uint32_t>(got.payload[2]) << 16) |
                      (static_cast<uint32_t>(got.payload[3]) << 24);
        std::shared_ptr<hos::Session> cli;
        ASSERT_MSG(kernel.services().session(id, cli), "ponta cliente");
        hos::IpcMessage open;
        open.cmd = 1;
        open.payload = {0x3D};
        ASSERT_MSG(cli->sendRequest(open), "cliente pede open");
        ASSERT_MSG(kernel.pumpServices() == 1, "bomba atendeu 1");
        ASSERT_MSG(kernel.nv().channelCount() == 1, "canal abriu sozinho");
        hos::IpcMessage back;
        ASSERT_MSG(cli->recvReply(back) && back.cmd == 1, "resposta voltou");
        ASSERT_MSG(kernel.pumpServices() == 0, "fila vazia, bomba parada");
    }

    // Display: abre, cria layer, apresenta via bomba.
    {
        hos::Kernel kernel;
        kernel.bootServices();
        hos::IpcMessage get;
        get.cmd = 1;
        const char* nm = "vi:u";
        get.payload = std::vector<uint8_t>(nm, nm + 4);
        hos::IpcMessage got;
        ASSERT_MSG(kernel.services().dispatch(get, got) && got.cmd == 1, "sessao vi");
        uint32_t id = static_cast<uint32_t>(got.payload[0]) |
                      (static_cast<uint32_t>(got.payload[1]) << 8) |
                      (static_cast<uint32_t>(got.payload[2]) << 16) |
                      (static_cast<uint32_t>(got.payload[3]) << 24);
        std::shared_ptr<hos::Session> cli;
        ASSERT_MSG(kernel.services().session(id, cli), "ponta cliente");
        hos::IpcMessage mk;
        mk.cmd = 2;
        ASSERT_MSG(cli->sendRequest(mk), "pede layer");
        ASSERT_MSG(kernel.pumpServices() == 1, "bomba criou");
        hos::IpcMessage layerrep;
        ASSERT_MSG(cli->recvReply(layerrep) && layerrep.cmd == 1, "layer id veio");
        hos::IpcMessage present;
        present.cmd = 3;
        present.payload = layerrep.payload;
        ASSERT_MSG(cli->sendRequest(present), "pede present");
        ASSERT_MSG(kernel.pumpServices() == 1, "bomba apresentou");
        ASSERT_MSG(kernel.vi().presented() == 1, "1 frame contado");
    }

    // Áudio: abre, start, enfileira, stop.
    {
        hos::AudService aud;
        hos::IpcMessage m;
        hos::IpcMessage r;
        m.cmd = 1;
        ASSERT_MSG(aud.dispatch(m, r) && r.cmd == 1, "audio abriu");
        m.cmd = 2;
        ASSERT_MSG(aud.dispatch(m, r) && aud.started(), "audio start");
        m.cmd = 4;
        m.payload = {1, 2, 3, 4};
        ASSERT_MSG(aud.dispatch(m, r) && aud.queuedBytes() == 4, "4 bytes");
        m.cmd = 3;
        m.payload.clear();
        ASSERT_MSG(aud.dispatch(m, r) && !aud.started(), "audio stop");
    }

    // FS virtual: adiciona, abre, lê, fecha.
    {
        hos::FsService fs;
        hos::IpcMessage add;
        add.cmd = 1;
        add.payload = {'r', 'o', 'm', 0, 10, 20, 30, 40};
        hos::IpcMessage r;
        ASSERT_MSG(fs.dispatch(add, r) && r.cmd == 1, "arquivo entrou");
        hos::IpcMessage open;
        open.cmd = 2;
        open.payload = {'r', 'o', 'm'};
        hos::IpcMessage opened;
        ASSERT_MSG(fs.dispatch(open, opened) && opened.cmd == 1, "abriu");
        hos::IpcMessage read;
        read.cmd = 3;
        read.payload = opened.payload;
        for (int i = 0; i < 8; i++) read.payload.push_back(0); // offset 0
        read.payload.push_back(2);                             // size 2
        for (int i = 0; i < 7; i++) read.payload.push_back(0);
        hos::IpcMessage data;
        ASSERT_MSG(fs.dispatch(read, data) && data.cmd == 1, "leu");
        ASSERT_MSG(data.payload.size() == 2 && data.payload[0] == 10 && data.payload[1] == 20,
                   "bytes certos");
        hos::IpcMessage close;
        close.cmd = 4;
        close.payload = opened.payload;
        hos::IpcMessage closed;
        ASSERT_MSG(fs.dispatch(close, closed) && closed.cmd == 1, "fechou");
        hos::IpcMessage missing;
        missing.cmd = 2;
        missing.payload = {'x'};
        hos::IpcMessage mrep;
        ASSERT_MSG(fs.dispatch(missing, mrep) && mrep.cmd == 0, "inexistente = 0");
    }

    // Input: aperta A+B, lê, solta A.
    {
        hos::HidService hid;
        hos::IpcMessage p;
        hos::IpcMessage r;
        p.cmd = 1;
        uint64_t ab = hos::BTN_A | hos::BTN_B;
        p.payload.resize(8);
        for (int i = 0; i < 8; i++) p.payload[i] = static_cast<uint8_t>(ab >> (8 * i));
        ASSERT_MSG(hid.dispatch(p, r) && r.cmd == 1, "apertou");
        hos::IpcMessage g;
        g.cmd = 3;
        hos::IpcMessage s;
        ASSERT_MSG(hid.dispatch(g, s) && s.cmd == 1, "leu");
        uint64_t m = 0;
        for (int i = 0; i < 8; i++) m |= static_cast<uint64_t>(s.payload[i]) << (8 * i);
        ASSERT_MSG(m == ab, "mascara certa");
        hos::IpcMessage rel;
        rel.cmd = 2;
        rel.payload.resize(8);
        for (int i = 0; i < 8; i++)
            rel.payload[i] = static_cast<uint8_t>(hos::BTN_A >> (8 * i));
        ASSERT_MSG(hid.dispatch(rel, r), "soltou A");
        ASSERT_MSG(hid.buttons() == hos::BTN_B, "resta B");
    }

    // Relógio: lê, ajusta, lê de novo.
    {
        hos::TimeService t;
        hos::IpcMessage g;
        hos::IpcMessage r;
        g.cmd = 1;
        ASSERT_MSG(t.dispatch(g, r) && r.cmd == 1, "hora veio");
        uint64_t h0 = 0;
        for (int i = 0; i < 8; i++) h0 |= static_cast<uint64_t>(r.payload[i]) << (8 * i);
        ASSERT_MSG(h0 == 1700000000ull, "hora inicial");
        hos::IpcMessage s;
        hos::IpcMessage sr;
        s.cmd = 2;
        s.payload.resize(8);
        for (int i = 0; i < 8; i++) s.payload[i] = static_cast<uint8_t>(1800000000ull >> (8 * i));
        ASSERT_MSG(t.dispatch(s, sr) && t.now() == 1800000000ull, "hora ajustada");
    }

    // Processos: cria, lê, mata.
    {
        hos::Kernel kernel;
        uint64_t game = kernel.createProcess("odyssey");
        uint64_t applet = kernel.createProcess("miiEdit");
        ASSERT_MSG(game != applet, "pids únicos");
        hos::Process p;
        ASSERT_MSG(kernel.getProcess(game, p) && p.name == "odyssey", "jogo achado");
        ASSERT_MSG(kernel.killProcess(applet), "applet morreu");
        ASSERT_MSG(!kernel.getProcess(applet, p), "morto nega");
        ASSERT_MSG(kernel.processCount() == 1, "resta o jogo");
    }

    // Eventos: espera falha, sinaliza, espera passa e consome.
    {
        hos::EventTable ev;
        uint32_t e = ev.create();
        ASSERT_MSG(!ev.wait(e), "sem sinal nega");
        ASSERT_MSG(ev.signal(e), "sinalizou");
        ASSERT_MSG(ev.wait(e), "passou");
        ASSERT_MSG(!ev.wait(e), "consumiu (auto-clear)");
        ASSERT_MSG(ev.signal(e) && ev.clear(e), "limpou");
        ASSERT_MSG(!ev.wait(e, false), "limpo nega");
        ASSERT_MSG(!ev.signal(999), "inexistente nega");
        ASSERT_MSG(ev.close(e) && ev.count() == 0, "fechou");
    }

    // Mutex: dono entra, outro espera, destrava libera.
    {
        hos::MutexTable mx;
        uint32_t m = mx.create();
        ASSERT_MSG(mx.lock(m, 1), "thread 1 trava");
        ASSERT_MSG(!mx.lock(m, 2), "thread 2 espera");
        ASSERT_MSG(mx.lock(m, 1), "dono retrava");
        ASSERT_MSG(!mx.unlock(m, 2), "outro nao destrava");
        ASSERT_MSG(mx.unlock(m, 1), "dono destrava");
        ASSERT_MSG(mx.lock(m, 2), "livre, thread 2 entra");
        ASSERT_MSG(!mx.lock(999, 1), "inexistente nega");
    }

    // Prioridade: B (prio 0) roda antes de A (prio 1).
    {
        emu::Cpu cpu;
        hos::Scheduler sched;
        // A @0 escreve 0xA00 em [0x300]
        poke32(cpu, 0x00, 0xD2814000u);
        poke32(cpu, 0x04, 0xD2806001u);
        poke32(cpu, 0x08, 0xF8000020u);
        poke32(cpu, 0x0C, 0xD4000001u);
        // B @0x40 escreve 0xB00 em [0x300]
        poke32(cpu, 0x40, 0xD2816000u);
        poke32(cpu, 0x44, 0xD2806001u);
        poke32(cpu, 0x48, 0xF8000020u);
        poke32(cpu, 0x4C, 0xD4000001u);
        sched.spawn(0x0, 0x8000, 1);  // A: menos importante
        sched.spawn(0x40, 0x9000, 0); // B: mais importante
        sched.run(cpu, 32, 8);
        uint64_t v = 0;
        for (int i = 0; i < 8; i++)
            v |= static_cast<uint64_t>(cpu.ram()[0x300 + i]) << (8 * i);
        ASSERT_MSG(v == 0xA00, "A escreveu por último (B foi primeiro)");
    }

    // Frame inteiro: bomba + dreno + mundo + PPM.
    {
        emu::Emulator emu;
        emu.kernel().bootServices();
        ASSERT_MSG(emu.frame("frame_boot.ppm", 8), "frame saiu");
        ASSERT_MSG(emu.frame("frame_idle.ppm", 8), "frame parado saiu");
        ASSERT_MSG(emu.frameCount() == 2, "2 frames");
        ASSERT_MSG(emu.lastFrameMs() >= 0.0, "tempo medido");
        ASSERT_MSG(emu.fps() > 0.0, "fps existe");
    }

    // Boot de NRO pelo Emulador: heap via SVC, saída limpa.
    {
        std::vector<uint32_t> text = {
            0xD2800201u, // MOVZ X1, #16
            0xD4000021u, // SVC #1 SetHeapSize
            0xD40000E1u, // SVC #7 ExitProcess
        };
        std::vector<uint8_t> blob(0x80 + text.size() * 4, 0);
        blob[0x10] = 'N'; blob[0x11] = 'R'; blob[0x12] = 'O'; blob[0x13] = '0';
        blob[0x20] = 0x80;
        blob[0x24] = static_cast<uint8_t>(text.size() * 4);
        for (size_t i = 0; i < text.size(); ++i)
            for (int b = 0; b < 4; b++)
                blob[0x80 + i * 4 + b] = static_cast<uint8_t>(text[i] >> (8 * b));
        emu::Emulator emu;
        ASSERT_MSG(emu.bootNro(blob.data(), blob.size()), "nro bootou");
        emu.runCpu(16);
        ASSERT_MSG(emu.cpu().stopped(), "nro saiu limpo");
        ASSERT_MSG(emu.kernel().heapSize() == 16, "heap do nro");
        ASSERT_MSG(emu.kernel().exited(), "exit marcado");
        std::vector<uint8_t> lixo = {1, 2, 3};
        ASSERT_MSG(!emu.bootNro(lixo.data(), lixo.size()), "lixo nega boot");
    }

    // LZ4: só-literais e com match, mais truncado que nega.
    {
        std::vector<uint8_t> out;
        const uint8_t lit[] = {0x30, 'H', 'i', '!'};
        ASSERT_MSG(emu::lz4::decompressBlock(lit, sizeof(lit), out), "literais ok");
        ASSERT_MSG(out.size() == 3 && out[0] == 'H' && out[2] == '!', "Hi!");
        const uint8_t m[] = {0x20, 'A', 'B', 0x02, 0x00};
        ASSERT_MSG(emu::lz4::decompressBlock(m, sizeof(m), out), "match ok");
        ASSERT_MSG(out.size() == 6, "ABABAB tem 6");
        ASSERT_MSG(out[0] == 'A' && out[2] == 'A' && out[5] == 'B', "match certo");
        const uint8_t bad[] = {0xF0, 'A'};
        ASSERT_MSG(!emu::lz4::decompressBlock(bad, sizeof(bad), out), "truncado nega");
        const uint8_t badoff[] = {0x10, 'A', 0x05, 0x00};
        ASSERT_MSG(!emu::lz4::decompressBlock(badoff, sizeof(badoff), out), "offset ruim nega");
    }

    // NSO sintético: .text comprimido roda do entry.
    {
        std::vector<uint8_t> blob(0x100, 0);
        blob[0] = 'N'; blob[1] = 'S'; blob[2] = 'O'; blob[3] = '0';
        blob[0x0C] = 1; // text comprimido
        // text: file 0x80, mem 0, decomp 8
        blob[0x10] = 0x80;
        blob[0x18] = 8;
        blob[0x60] = 9; // comp 9
        // bloco LZ4: token 0x80 + MOVZ X0,#7 + SVC#0
        blob[0x80] = 0x80;
        blob[0x81] = 0xE0; blob[0x82] = 0x00; blob[0x83] = 0x80; blob[0x84] = 0xD2;
        blob[0x85] = 0x01; blob[0x86] = 0x00; blob[0x87] = 0x00; blob[0x88] = 0xD4;
        emu::NsoImage img = emu::parseNso(blob.data(), blob.size());
        ASSERT_MSG(img.valid && img.comp_text, "nso valido");
        emu::Cpu cpu;
        uint64_t entry = 0;
        ASSERT_MSG(emu::loadNsoInto(img, blob.data(), cpu.ram(), cpu.ramSize(), 0, entry),
                   "nso mapeado");
        ASSERT_MSG(entry == 0, "entry no text");
        cpu.setPc(entry);
        ASSERT_MSG(cpu.run(8) == 2, "roda text descomprimido");
        ASSERT_MSG(cpu.reg(0) == 7 && cpu.stopped(), "x0=7 e parou");
        emu::NsoImage bad = emu::parseNso(blob.data(), 16);
        ASSERT_MSG(!bad.valid, "curto invalido");
    }

    // Trace registra PCs na ordem.
    {
        emu::Cpu cpu;
        cpu.traceEnable(true);
        ASSERT_MSG(cpu.step(0xD28000E0u), "movz pc=0");
        ASSERT_MSG(cpu.step(0xD2800021u), "movz pc=4");
        ASSERT_MSG(cpu.tracePc(0) == 4 && cpu.tracePc(1) == 0, "trace ordem");
        cpu.traceEnable(false);
    }

    // Performance: handheld padrão.
    {
        hos::ApmService apm;
        hos::IpcMessage g;
        hos::IpcMessage r;
        g.cmd = 1;
        ASSERT_MSG(apm.dispatch(g, r) && r.cmd == 1, "modo veio");
        ASSERT_MSG(!r.payload.empty() && r.payload[0] == 0, "handheld");
        ASSERT_MSG(!apm.docked(), "sem dock");
    }

    // Bateria 100 e brilho 1.0.
    {
        hos::PsmService psm;
        hos::LblService lbl;
        hos::IpcMessage m;
        hos::IpcMessage r;
        m.cmd = 1;
        ASSERT_MSG(psm.dispatch(m, r) && r.payload[0] == 100, "bateria 100");
        m.cmd = 1;
        hos::IpcMessage lr;
        ASSERT_MSG(lbl.dispatch(m, lr) && lr.cmd == 1, "brilho veio");
        uint32_t u = static_cast<uint32_t>(lr.payload[0]) |
                     (static_cast<uint32_t>(lr.payload[1]) << 8) |
                     (static_cast<uint32_t>(lr.payload[2]) << 16) |
                     (static_cast<uint32_t>(lr.payload[3]) << 24);
        float b = 0;
        std::memcpy(&b, &u, 4);
        ASSERT_MSG(b == 1.0f, "brilho 1.0");
    }

    // Idioma en-US e região Américas.
    {
        hos::SetService set;
        hos::IpcMessage g;
        hos::IpcMessage r;
        g.cmd = 1;
        ASSERT_MSG(set.dispatch(g, r) && r.cmd == 1, "idioma veio");
        ASSERT_MSG(r.payload.size() == 8 && r.payload[0] == 'e' && r.payload[4] == 'S',
                   "en-US");
        g.cmd = 2;
        hos::IpcMessage rr;
        ASSERT_MSG(set.dispatch(g, rr) && rr.payload[0] == 1, "americas");
    }

    // Fatal registra em vez de sumir.
    {
        hos::FatalService fatal;
        hos::IpcMessage t;
        hos::IpcMessage r;
        t.cmd = 1;
        t.payload = {0xEF, 0xBE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        ASSERT_MSG(fatal.dispatch(t, r) && r.cmd == 1, "registrou");
        ASSERT_MSG(fatal.fatalCount() == 1, "contou 1");
        ASSERT_MSG(fatal.lastFatal() == 0xBEEF, "codigo certo");
    }

    // RomFS sintético: lista raiz e lê arquivo.
    {
        std::vector<uint8_t> blob(0xA8, 0);
        auto w32 = [&](size_t off, uint32_t v) {
            for (int i = 0; i < 4; i++) blob[off + i] = static_cast<uint8_t>(v >> (8 * i));
        };
        auto w64 = [&](size_t off, uint64_t v) {
            for (int i = 0; i < 8; i++) blob[off + i] = static_cast<uint8_t>(v >> (8 * i));
        };
        w32(0x0C, 0x50); // dir table
        w32(0x10, 0x18);
        w32(0x1C, 0x80); // file table
        w32(0x20, 0x25);
        w64(0x24, 0xA5); // dados
        // root @0x50: file -> 0x80
        w32(0x50 + 0x04, 0xFFFFFFFFu); // sibling
        w32(0x50 + 0x08, 0xFFFFFFFFu); // child
        w32(0x50 + 0x0C, 0x80);        // file
        // file0 @0x80: "a.txt", dados em 0, tamanho 3
        w32(0x80 + 0x00, 0x50); // parent
        w32(0x80 + 0x04, 0xFFFFFFFFu);
        w64(0x80 + 0x08, 0);
        w64(0x80 + 0x10, 3);
        w32(0x80 + 0x1C, 5);
        blob[0xA0] = 'a'; blob[0xA1] = '.'; blob[0xA2] = 't';
        blob[0xA3] = 'x'; blob[0xA4] = 't';
        blob[0xA5] = 7; blob[0xA6] = 8; blob[0xA7] = 9;
        emu::RomFsReader rom;
        ASSERT_MSG(rom.open(blob.data(), blob.size()), "romfs abriu");
        std::vector<std::string> names = rom.listRoot();
        ASSERT_MSG(names.size() == 1 && names[0] == "a.txt", "lista raiz");
        std::vector<uint8_t> data;
        ASSERT_MSG(rom.readRootFile("a.txt", data), "leu arquivo");
        ASSERT_MSG(data.size() == 3 && data[0] == 7 && data[2] == 9, "bytes certos");
        ASSERT_MSG(!rom.readRootFile("nada", data), "inexistente nega");
    }

    // PFS0 sintético: 2 arquivos entram e saem intactos.
    {
        std::vector<uint8_t> blob(82, 0);
        auto w32 = [&](size_t off, uint32_t v) {
            for (int i = 0; i < 4; i++) blob[off + i] = static_cast<uint8_t>(v >> (8 * i));
        };
        auto w64 = [&](size_t off, uint64_t v) {
            for (int i = 0; i < 8; i++) blob[off + i] = static_cast<uint8_t>(v >> (8 * i));
        };
        blob[0] = 'P'; blob[1] = 'F'; blob[2] = 'S'; blob[3] = '0';
        w32(4, 2);   // 2 arquivos
        w32(8, 12);  // strtab 12
        w64(16, 0); w64(24, 4); w32(32, 0);   // f0: off 0, size 4, nome 0
        w64(40, 4); w64(48, 2); w32(56, 6);   // f1: off 4, size 2, nome 6
        const char* s = "a.nca\0b.nca\0";
        for (int i = 0; i < 12; i++) blob[64 + i] = static_cast<uint8_t>(s[i]);
        blob[76] = 1; blob[77] = 2; blob[78] = 3; blob[79] = 4;
        blob[80] = 5; blob[81] = 6;
        emu::Pfs0Reader pfs;
        ASSERT_MSG(pfs.open(blob.data(), blob.size()), "pfs0 abriu");
        ASSERT_MSG(pfs.fileCount() == 2, "2 arquivos");
        ASSERT_MSG(pfs.fileName(1) == "b.nca", "nome certo");
        std::vector<uint8_t> f0, f1;
        ASSERT_MSG(pfs.readFile("a.nca", f0), "leu a");
        ASSERT_MSG(pfs.readFile("b.nca", f1), "leu b");
        ASSERT_MSG(f0.size() == 4 && f0[3] == 4, "a intacto");
        ASSERT_MSG(f1.size() == 2 && f1[0] == 5, "b intacto");
        std::vector<uint8_t> no;
        ASSERT_MSG(!pfs.readFile("z", no), "inexistente nega");
    }

    // AES: vetor NIST ECB + round-trip CTR.
    {
        uint8_t key[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
        uint8_t pt[16] = {0, 17, 34, 51, 68, 85, 102, 119, 136, 153, 170, 187, 204, 221, 238, 255};
        uint8_t ct[16] = {0};
        emu::aes::encryptEcb(key, pt, ct);
        const uint8_t want[16] = {0x69, 0xC4, 0xE0, 0xD8, 0x6A, 0x7B, 0x04, 0x30,
                                  0xD8, 0xCD, 0xB7, 0x80, 0x70, 0xB4, 0xC5, 0x55};
        bool ok = true;
        for (int i = 0; i < 16; i++) ok = ok && (ct[i] == want[i]);
        ASSERT_MSG(ok, "NIST ECB bate");
        uint8_t nonce[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
        uint8_t msg[40];
        for (int i = 0; i < 40; i++) msg[i] = static_cast<uint8_t>(i * 7 + 1);
        uint8_t enc[40] = {0}, dec[40] = {0};
        emu::aes::cryptCtr(key, nonce, msg, enc, 40);
        emu::aes::cryptCtr(key, nonce, enc, dec, 40);
        bool rt = true;
        for (int i = 0; i < 40; i++) rt = rt && (dec[i] == msg[i]);
        ASSERT_MSG(rt, "CTR round-trip");
        bool diff = false;
        for (int i = 0; i < 40; i++) diff = diff || (enc[i] != msg[i]);
        ASSERT_MSG(diff, "CTR embaralhou");
        // CMAC NIST: key 2b7e..., msg vazio -> bb1d6929e95937287fa37d129b756746
        uint8_t k2[16] = {0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
                          0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C};
        uint8_t tag[16] = {0};
        emu::aes::cmac(k2, nullptr, 0, tag);
        const uint8_t want2[16] = {0xBB, 0x1D, 0x69, 0x29, 0xE9, 0x59, 0x37, 0x28,
                                   0x7F, 0xA3, 0x7D, 0x12, 0x9B, 0x75, 0x67, 0x46};
        bool ok2 = true;
        for (int i = 0; i < 16; i++) ok2 = ok2 && (tag[i] == want2[i]);
        ASSERT_MSG(ok2, "NIST CMAC bate");
    }

    // SHA-256: vetor NIST "abc".
    {
        const uint8_t abc[3] = {'a', 'b', 'c'};
        uint8_t out[32] = {0};
        emu::sha::hash(abc, 3, out);
        const uint8_t want[32] = {0xBA, 0x78, 0x16, 0xBF, 0x8F, 0x01, 0xCF, 0xEA,
                                  0x41, 0x41, 0x40, 0xDE, 0x5D, 0xAE, 0x22, 0x23,
                                  0xB0, 0x03, 0x61, 0xA3, 0x96, 0x17, 0x7A, 0x9C,
                                  0xB4, 0x10, 0xFF, 0x61, 0xF2, 0x00, 0x15, 0xAD};
        bool ok = true;
        for (int i = 0; i < 32; i++) ok = ok && (out[i] == want[i]);
        ASSERT_MSG(ok, "NIST sha256 bate");
        uint8_t empty[32] = {0};
        emu::sha::hash(nullptr, 0, empty);
        ASSERT_MSG(empty[0] == 0xE3 && empty[31] == 0x55, "sha256 vazio bate");
    }

    // Sonda NCA: identifica programa, nega lixo e curto.
    {
        std::vector<uint8_t> blob(0x400, 0);
        blob[0x200] = 'N'; blob[0x201] = 'C'; blob[0x202] = 'A'; blob[0x203] = '3';
        blob[0x205] = 0; // programa
        emu::NcaProbe p = emu::probeNca(blob.data(), blob.size());
        ASSERT_MSG(p.valid, "nca valido");
        ASSERT_MSG(p.content_type == 0, "tipo programa");
        std::vector<uint8_t> lixo(0x400, 0);
        ASSERT_MSG(!emu::probeNca(lixo.data(), lixo.size()).valid, "lixo nega");
        ASSERT_MSG(!emu::probeNca(blob.data(), 16).valid, "curto nega");
    }

    // PM responde o PID.
    {
        hos::PmService pm;
        hos::IpcMessage g;
        hos::IpcMessage r;
        g.cmd = 1;
        ASSERT_MSG(pm.dispatch(g, r) && r.cmd == 1, "pid veio");
        uint64_t pid = 0;
        for (int i = 0; i < 8; i++) pid |= static_cast<uint64_t>(r.payload[i]) << (8 * i);
        ASSERT_MSG(pid == 1, "pid do jogo");
    }

    // Vsync: present acorda quem espera.
    {
        hos::ViService vi;
        hos::IpcMessage c4;
        hos::IpcMessage r4;
        c4.cmd = 4;
        ASSERT_MSG(vi.dispatch(c4, r4) && r4.cmd == 1, "evento criado");
        uint32_t e = static_cast<uint32_t>(r4.payload[0]) |
                     (static_cast<uint32_t>(r4.payload[1]) << 8) |
                     (static_cast<uint32_t>(r4.payload[2]) << 16) |
                     (static_cast<uint32_t>(r4.payload[3]) << 24);
        ASSERT_MSG(!vi.events().wait(e, false), "sem frame, sem sinal");
        hos::IpcMessage mk;
        hos::IpcMessage mr;
        mk.cmd = 2;
        ASSERT_MSG(vi.dispatch(mk, mr), "layer criada");
        hos::IpcMessage p;
        hos::IpcMessage pr;
        p.cmd = 3;
        p.payload = mr.payload;
        ASSERT_MSG(vi.dispatch(p, pr) && pr.cmd == 1, "present");
        ASSERT_MSG(vi.events().wait(e, false), "vsync acordou");
    }

    // NEON: D alimenta V (alias), FADD soma 2 lanes, ORR move 128.
    {
        emu::Cpu cpu;
        cpu.setReg(0, 3);
        cpu.setReg(1, 4);
        ASSERT_MSG(cpu.step(0x1E660000u), "d0=3.0 (v0[0])");
        ASSERT_MSG(cpu.step(0x1E660021u), "d1=4.0 (v1[0])");
        ASSERT_MSG(cpu.step(0x6E61D402u), "fadd v2.2d,v0.2d,v1.2d");
        ASSERT_MSG(cpu.step(0x1E620043u), "scvtf x3,d2");
        ASSERT_MSG(cpu.reg(3) == 7, "lane0=7");
        ASSERT_MSG(cpu.step(0x6E221C43u), "orr v3.16b,v2.16b,v2.16b");
        ASSERT_MSG(cpu.step(0x1E620063u), "scvtf x3,d3");
        ASSERT_MSG(cpu.reg(3) == 7, "moveu 128");
    }

    // Vetor inteiro 2 lanes: carrega 128, soma, guarda.
    {
        emu::Cpu cpu;
        auto w64 = [&](uint64_t addr, uint64_t v) {
            for (int i = 0; i < 8; i++)
                cpu.ram()[addr + i] = static_cast<uint8_t>(v >> (8 * i));
        };
        w64(0x100, 10); w64(0x108, 20); // V0 = [10,20]
        w64(0x110, 1); w64(0x118, 2);   // V1 = [1,2]
        cpu.setReg(1, 0x100);
        ASSERT_MSG(cpu.step(0x3DC00020u), "ldr q0,[x1]");
        ASSERT_MSG(cpu.step(0x3DC00421u), "ldr q1,[x1,#16]");
        ASSERT_MSG(cpu.step(0x6E218402u), "add v2.2d,v0.2d,v1.2d");
        ASSERT_MSG(cpu.step(0xD2804003u), "movz x3,#0x200");
        ASSERT_MSG(cpu.step(0x3D800062u), "str q2,[x3]");
        ASSERT_MSG(cpu.ram()[0x200] == 11, "lane0=11");
        ASSERT_MSG(cpu.ram()[0x208] == 22, "lane1=22");
    }

    // FMLA vetorial: d += n*m por lane.
    {
        emu::Cpu cpu;
        auto w64 = [&](uint64_t addr, uint64_t v) {
            for (int i = 0; i < 8; i++)
                cpu.ram()[addr + i] = static_cast<uint8_t>(v >> (8 * i));
        };
        // doubles como bits: 2.0=0x4000000000000000 etc. usa inteiros que cabem exato
        auto dbits = [](uint64_t iv) {
            double d = static_cast<double>(iv);
            uint64_t u = 0;
            __builtin_memcpy(&u, &d, 8);
            return u;
        };
        w64(0x100, dbits(2)); w64(0x108, dbits(3));
        w64(0x110, dbits(4)); w64(0x118, dbits(5));
        w64(0x120, dbits(10)); w64(0x128, dbits(10));
        cpu.setReg(1, 0x100);
        ASSERT_MSG(cpu.step(0x3DC00020u), "ldr q0");
        ASSERT_MSG(cpu.step(0x3DC00421u), "ldr q1");
        ASSERT_MSG(cpu.step(0x3DC00822u), "ldr q2");
        ASSERT_MSG(cpu.step(0x6E21CC02u), "fmla v2.2d,v0.2d,v1.2d");
        ASSERT_MSG(cpu.step(0xD2804003u), "movz x3,#0x200");
        ASSERT_MSG(cpu.step(0x3D800062u), "str q2");
        auto r64 = [&](uint64_t addr) {
            uint64_t v = 0;
            for (int i = 0; i < 8; i++) v |= static_cast<uint64_t>(cpu.ram()[addr + i]) << (8 * i);
            double d = 0;
            __builtin_memcpy(&d, &v, 8);
            return d;
        };
        ASSERT_MSG(r64(0x200) == 18.0 && r64(0x208) == 25.0, "fmla certo");
    }

    // FMAX vetorial.
    {
        emu::Cpu cpu;
        auto wdb = [&](uint64_t addr, double d) {
            uint64_t u = 0;
            __builtin_memcpy(&u, &d, 8);
            for (int i = 0; i < 8; i++)
                cpu.ram()[addr + i] = static_cast<uint8_t>(u >> (8 * i));
        };
        wdb(0x100, 3.0); wdb(0x108, 9.0);
        wdb(0x110, 7.0); wdb(0x118, 1.0);
        cpu.setReg(1, 0x100);
        ASSERT_MSG(cpu.step(0x3DC00020u), "ldr q0");
        ASSERT_MSG(cpu.step(0x3DC00421u), "ldr q1");
        ASSERT_MSG(cpu.step(0x6E610402u), "fmax v2.2d,v0.2d,v1.2d");
        ASSERT_MSG(cpu.step(0xD2804003u), "movz x3,#0x200");
        ASSERT_MSG(cpu.step(0x3D800062u), "str q2");
        auto rdb = [&](uint64_t addr) {
            uint64_t v = 0;
            for (int i = 0; i < 8; i++) v |= static_cast<uint64_t>(cpu.ram()[addr + i]) << (8 * i);
            double d = 0;
            __builtin_memcpy(&d, &v, 8);
            return d;
        };
        ASSERT_MSG(rdb(0x200) == 7.0 && rdb(0x208) == 9.0, "fmax certo");
    }

    // FCMEQ vetorial: igual vira tudo-1.
    {
        emu::Cpu cpu;
        auto wdb = [&](uint64_t addr, double d) {
            uint64_t u = 0;
            __builtin_memcpy(&u, &d, 8);
            for (int i = 0; i < 8; i++)
                cpu.ram()[addr + i] = static_cast<uint8_t>(u >> (8 * i));
        };
        wdb(0x100, 5.0); wdb(0x108, 6.0);
        wdb(0x110, 5.0); wdb(0x118, 7.0);
        cpu.setReg(1, 0x100);
        ASSERT_MSG(cpu.step(0x3DC00020u), "ldr q0");
        ASSERT_MSG(cpu.step(0x3DC00421u), "ldr q1");
        ASSERT_MSG(cpu.step(0x6E612402u), "fcmeq v2.2d,v0.2d,v1.2d");
        ASSERT_MSG(cpu.step(0xD2804003u), "movz x3,#0x200");
        ASSERT_MSG(cpu.step(0x3D800062u), "str q2");
        auto r64 = [&](uint64_t addr) {
            uint64_t v = 0;
            for (int i = 0; i < 8; i++) v |= static_cast<uint64_t>(cpu.ram()[addr + i]) << (8 * i);
            return v;
        };
        ASSERT_MSG(r64(0x200) == ~0ull && r64(0x208) == 0ull, "mascara certa");
    }

    // BSL blend por máscara.
    {
        emu::Cpu cpu;
        auto w64 = [&](uint64_t addr, uint64_t v) {
            for (int i = 0; i < 8; i++)
                cpu.ram()[addr + i] = static_cast<uint8_t>(v >> (8 * i));
        };
        w64(0x100, ~0ull); w64(0x108, 0);     // V0 = máscara
        w64(0x110, 0xAA); w64(0x118, 0xBB);   // V1 = novo
        w64(0x120, 0x11); w64(0x128, 0x22);   // V2 = velho
        cpu.setReg(1, 0x100);
        ASSERT_MSG(cpu.step(0x3DC00020u), "ldr q0");
        ASSERT_MSG(cpu.step(0x3DC00421u), "ldr q1");
        ASSERT_MSG(cpu.step(0x3DC00822u), "ldr q2");
        ASSERT_MSG(cpu.step(0x6E601C22u), "bsl v2,v1,v0");
        ASSERT_MSG(cpu.step(0xD2804003u), "movz x3,#0x200");
        ASSERT_MSG(cpu.step(0x3D800062u), "str q2");
        auto r64 = [&](uint64_t addr) {
            uint64_t v = 0;
            for (int i = 0; i < 8; i++) v |= static_cast<uint64_t>(cpu.ram()[addr + i]) << (8 * i);
            return v;
        };
        ASSERT_MSG(r64(0x200) == 0xAA && r64(0x208) == 0x22, "blend certo");
    }

    // TST + CMP registrado.
    {
        emu::Cpu cpu;
        cpu.setReg(0, 0xF0);
        cpu.setReg(1, 0x3C);
        ASSERT_MSG(cpu.step(0xEA01001Fu), "tst x0,x1");
        ASSERT_MSG(cpu.reg(0) == 0xF0, "tst nao escreve");
        uint64_t pc = cpu.pc();
        ASSERT_MSG(cpu.step(0x54000041u), "b.ne (0x30!=0)");
        ASSERT_MSG(cpu.pc() == pc + 8, "ne tomou");
        cpu.setReg(0, 10);
        cpu.setReg(1, 10);
        ASSERT_MSG(cpu.step(0xEB01001Fu), "cmp x0,x1");
        pc = cpu.pc();
        ASSERT_MSG(cpu.step(0x54000040u), "b.eq (iguais)");
        ASSERT_MSG(cpu.pc() == pc + 8, "eq tomou");
    }

    // Shifts registrados 32-bit.
    {
        emu::Cpu cpu;
        cpu.setReg(1, 1);
        cpu.setReg(2, 3);
        ASSERT_MSG(cpu.step(0x1AC22020u), "lslv w0,w1,w2");
        ASSERT_MSG(cpu.reg(0) == 8, "1<<3");
        cpu.setReg(1, 0x80000000ull);
        cpu.setReg(2, 4);
        ASSERT_MSG(cpu.step(0x1AC22823u), "asrv w3,w1,w2");
        ASSERT_MSG(cpu.reg(3) == 0xF8000000ull, "asr 32 propaga");
    }

    // SBFM 32-bit: SXTB/SXTH W.
    {
        emu::Cpu cpu;
        cpu.setReg(1, 0xFF);
        ASSERT_MSG(cpu.step(0x13401C20u), "sxtb w0,w1");
        ASSERT_MSG(cpu.reg(0) == 0xFFFFFFFFull, "byte->-1 (32)");
        cpu.setReg(1, 0xFFFF);
        ASSERT_MSG(cpu.step(0x13403C22u), "sxth w2,w1");
        ASSERT_MSG(cpu.reg(2) == 0xFFFFFFFFull, "half->-1 (32)");
        cpu.setReg(1, 0x7F);
        ASSERT_MSG(cpu.step(0x13401C20u), "sxtb w positivo");
        ASSERT_MSG(cpu.reg(0) == 0x7F, "mantem");
    }

    // FCVTZS / FCVTZU (double -> int32, trunca).
    {
        emu::Cpu cpu;
        cpu.setReg(0, 15);
        cpu.setReg(1, 2);
        ASSERT_MSG(cpu.step(0x1E660000u), "d0=15.0");
        ASSERT_MSG(cpu.step(0x1E660021u), "d1=2.0");
        ASSERT_MSG(cpu.step(0x1EE11002u), "fdiv d2,d0,d1 (7.5)");
        ASSERT_MSG(cpu.step(0x1E580040u), "fcvtzs w0,d2");
        ASSERT_MSG(cpu.reg(0) == 7, "trunca 7.5 -> 7");
        ASSERT_MSG(cpu.step(0x1E590042u), "fcvtzu w2,d2");
        ASSERT_MSG(cpu.reg(2) == 7, "unsigned 7");
    }

    // LDRSB / LDRSH / STRH.
    {
        emu::Cpu cpu;
        cpu.setReg(1, 0x100);
        cpu.setReg(0, 0xFF);
        ASSERT_MSG(cpu.step(0x39000020u), "strb x0,[x1]");
        ASSERT_MSG(cpu.step(0x39800022u), "ldrsb x2,[x1]");
        ASSERT_MSG(cpu.reg(2) == 0xFFFFFFFFFFFFFFFFull, "byte->-1 via ldrsb");
        cpu.setReg(0, 0xFFFF);
        ASSERT_MSG(cpu.step(0x79000420u), "strh w0,[x1,#2]");
        ASSERT_MSG(cpu.step(0x79800422u), "ldrsh x2,[x1,#2]");
        ASSERT_MSG(cpu.reg(2) == 0xFFFFFFFFFFFFFFFFull, "half->-1 via ldrsh");
        ASSERT_MSG(cpu.step(0x79400423u), "ldrh x3,[x1,#2]");
        ASSERT_MSG(cpu.reg(3) == 0xFFFFull, "ldrh zero-extend");
    }

    // LDUR/STUR com offset negativo.
    {
        emu::Cpu cpu;
        cpu.setReg(2, 0x100);
        cpu.setReg(1, 0xAB);
        ASSERT_MSG(cpu.step(0x381FF041u), "sturb w1,[x2,#-1]");
        ASSERT_MSG(cpu.step(0x385FF043u), "ldurb x3,[x2,#-1]");
        ASSERT_MSG(cpu.reg(3) == 0xAB, "byte sem escala");
        cpu.setReg(0, 0xFFFF);
        ASSERT_MSG(cpu.step(0x781FE040u), "sturh w0,[x2,#-2]");
        ASSERT_MSG(cpu.step(0x789FE044u), "ldursh x4,[x2,#-2]");
        ASSERT_MSG(cpu.reg(4) == 0xFFFFFFFFFFFFFFFFull, "half sinal sem escala");
    }

    // LDUR/STUR 32/64-bit + LDURSW.
    {
        emu::Cpu cpu;
        cpu.setReg(1, 0x100);
        cpu.setReg(0, 0x1122334455667788ull);
        ASSERT_MSG(cpu.step(0xF81F8020u), "stur x0,[x1,#-8]");
        ASSERT_MSG(cpu.step(0xF85F8022u), "ldur x2,[x1,#-8]");
        ASSERT_MSG(cpu.reg(2) == 0x1122334455667788ull, "64 sem escala");
        cpu.setReg(4, 0xFFFFFFFFull);
        ASSERT_MSG(cpu.step(0xB89F8024u), "stur w4,[x1,#-8]");
        ASSERT_MSG(cpu.step(0xB89F8023u), "ldursw x3,[x1,#-8]");
        ASSERT_MSG(cpu.reg(3) == 0xFFFFFFFFFFFFFFFFull, "sw estende");
    }

    // LDR/STR S + STP/LDP S (float 32).
    {
        emu::Cpu cpu;
        cpu.setReg(0, 7);
        cpu.setReg(1, 0x100);
        ASSERT_MSG(cpu.step(0x1E660000u), "d0=7.0");
        ASSERT_MSG(cpu.step(0x1E624001u), "fcvt s1,d0");
        ASSERT_MSG(cpu.step(0xBD000021u), "str s1,[x1]");
        ASSERT_MSG(cpu.step(0xBD400022u), "ldr s2,[x1]");
        ASSERT_MSG(cpu.step(0x1E22C042u), "fcvt d2,s2");
        ASSERT_MSG(cpu.step(0x1E620043u), "scvtf x3,d2");
        ASSERT_MSG(cpu.reg(3) == 7, "float 32 voltou");
    }

    // FMOV Xd,Dn: bits do double no GPR.
    {
        emu::Cpu cpu;
        cpu.setReg(0, 7);
        ASSERT_MSG(cpu.step(0x1E660000u), "d0=7.0");
        ASSERT_MSG(cpu.step(0x1E680001u), "fmov x1,d0");
        ASSERT_MSG(cpu.reg(1) == 0x401C000000000000ull, "bits de 7.0");
    }

    // LDTRB (unprivileged) funciona via caminho LDUR.
    {
        emu::Cpu cpu;
        cpu.setReg(1, 0x100);
        cpu.setReg(0, 0x5A);
        ASSERT_MSG(cpu.step(0x39000020u), "strb x0,[x1]");
        ASSERT_MSG(cpu.step(0x38600022u), "ldtrb w2,[x1]");
        ASSERT_MSG(cpu.reg(2) == 0x5A, "ldtrb leu");
    }

    std::cout << "  Emulator machine tests passed!" << std::endl;
    return true;
}
