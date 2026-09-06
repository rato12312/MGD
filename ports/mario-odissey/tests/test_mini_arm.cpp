#include <cassert>
#include <cstdio>
#include "../src/cpu/MiniArm.h"

int main() {
    port::cpu::MiniArm cpu;

    // MOVZ X0, #0x1234
    assert(cpu.step(0xD2824680u));
    assert(cpu.reg(0) == 0x1234u);
    assert(cpu.pc() == 4);

    // MOVZ X1, #1, LSL #16
    assert(cpu.step(0xD2A00021u));
    assert(cpu.reg(1) == 0x10000u);

    // ADD X2, X0, #5  (0x1234 + 5)
    assert(cpu.step(0x91001402u));
    assert(cpu.reg(2) == 0x1239u);

    // SUB X3, X2, #1
    assert(cpu.step(0xD1000443u));
    assert(cpu.reg(3) == 0x1238u);

    // ORR X4, X0, X1 (MOV registrado)
    assert(cpu.step(0xAA010004u));
    assert(cpu.reg(4) == (0x1234u | 0x10000u));

    // XZR: escrita descartada, leitura zero
    assert(cpu.step(0xAA1F03FFu)); // ORR XZR, XZR, XZR
    assert(cpu.reg(0) == 0x1234u); // X0 intacto

    // B +8 (pula 2 instruções a partir do pc atual)
    {
        uint64_t pc = cpu.pc();
        assert(cpu.step(0x14000002u));
        assert(cpu.pc() == pc + 8);
    }

    // Opcode fora do escopo (ex.: SVC) retorna false sem andar
    {
        uint64_t pc = cpu.pc();
        assert(!cpu.step(0xD4000001u)); // SVC #0
        assert(cpu.pc() == pc);
    }

    assert(cpu.steps() == 7);

    // STR X0, [X1] + LDR X5, [X1] (round-trip pela RAM)
    cpu.reset();
    assert(cpu.step(0xD2824680u)); // X0 = 0x1234
    cpu.setReg(1, 0x100);          // X1 = base
    assert(cpu.step(0xF8000020u)); // STR X0, [X1]
    cpu.setReg(0, 0);
    assert(cpu.step(0xF9400025u)); // LDR X5, [X1]
    assert(cpu.reg(5) == 0x1234u);

    // STR/LDR com offset: STR X5, [X1, #16] + LDR X6, [X1, #16]
    assert(cpu.step(0xF8000825u)); // STR X5, [X1, #16]
    cpu.setReg(5, 0);
    assert(cpu.step(0xF9400866u)); // LDR X6, [X1, #16]
    assert(cpu.reg(6) == 0x1234u);

    // fora da RAM retorna false sem andar
    {
        cpu.setReg(1, cpu.ramSize() - 4);
        uint64_t pc = cpu.pc();
        assert(!cpu.step(0xF8000020u)); // STR X0, [X1] estoura
        assert(cpu.pc() == pc);
    }

    cpu.reset();
    assert(cpu.pc() == 0 && cpu.reg(0) == 0 && cpu.steps() == 0);

    std::printf("mini arm tests passed!\n");
    return 0;
}
