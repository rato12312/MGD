#pragma once

#include <array>
#include <cstdint>

#include "eden/common/bit_field.h"

namespace port {
namespace cpu {

// Decodificador de instrução usando o BitField vendorado do Eden.
union ArmInsn {
    uint32_t hex;
    BitField<0, 5, uint32_t> rd;    // registrador destino
    BitField<5, 5, uint32_t> rn;    // primeiro operando
    BitField<10, 12, uint32_t> imm12; // imediato 12 bits
    BitField<16, 5, uint32_t> rm;   // segundo operando (reg)
    BitField<21, 2, uint32_t> hw;   // MOVZ: qual meia-palavra
    BitField<0, 26, int32_t> off26; // B: offset (com sinal)
};

// Interpretador ARM64 mínimo do port (primeiro passo da execução).
// Escopo deliberado: 31 registradores X + SP + PC e um punhado de
// instruções (MOVZ, ADD/SUB imediato, ORR, B). Sem MMU, sem SVC, sem NEON.
// Cada instrução nova entra com teste. O resto vem depois.
class MiniArm {
public:
    static constexpr int REG_COUNT = 31;

    MiniArm() { reset(); }

    void reset() {
        regs_.fill(0);
        sp_ = 0;
        pc_ = 0;
        steps_ = 0;
    }

    uint64_t reg(int i) const { return (i >= 0 && i < REG_COUNT) ? regs_[i] : 0; }
    void setReg(int i, uint64_t v) { if (i >= 0 && i < REG_COUNT) regs_[i] = v; }
    uint64_t pc() const { return pc_; }
    void setPc(uint64_t v) { pc_ = v; }
    uint64_t steps() const { return steps_; }

    // Executa UMA instrução de 32 bits. Retorna false se opcode desconhecido.
    // Subconjunto ARMv8 64-bit (máscaras no byte alto / bits fixos):
    // - B <off26>              : 000101 (insn>>26 == 0x05)
    // - MOVZ Xd,#imm,LSL#s     : 11010010 1x (top byte 0xD2..0xD5)
    // - ADD  Xd,Xn,#imm(,LSL12): 10010001 (top byte 0x91)
    // - SUB  Xd,Xn,#imm(,LSL12): 11010001 (top byte 0xD1)
    // - ORR  Xd,Xn,Xm (MOV reg): bits[31:21] == 0x550 (shift 00)
    // X31 lido como XZR=0 e escrita descartada (SP simplificado).
    bool step(uint32_t insn) {
        ArmInsn dec;
        dec.hex = insn;
        if ((insn >> 26) == 0x05) { // B
            int32_t off = static_cast<int32_t>(dec.off26); // já com sinal
            pc_ = static_cast<uint64_t>(static_cast<int64_t>(pc_) + (static_cast<int64_t>(off) << 2));
            steps_++;
            return true;
        }
        uint8_t top = static_cast<uint8_t>(insn >> 24);
        if ((top & 0xFC) == 0xD2) { // MOVZ 64-bit
            int d = static_cast<int>(dec.rd);
            uint16_t imm = static_cast<uint16_t>((dec.hex >> 5) & 0xFFFF);
            int shift = static_cast<int>(dec.hw) * 16;
            setReg(d, static_cast<uint64_t>(imm) << shift);
            pc_ += 4;
            steps_++;
            return true;
        }
        if (top == 0x91 || top == 0xD1) { // ADD / SUB imediato 64-bit
            bool isAdd = (top == 0x91);
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            uint32_t imm = static_cast<uint32_t>(dec.imm12);
            if (insn & 0x00400000) imm <<= 12; // LSL #12
            uint64_t nv = (n == 31) ? 0 : regs_[n];
            uint64_t res = isAdd ? (nv + imm) : (nv - imm);
            if (d != 31) regs_[d] = res;
            pc_ += 4;
            steps_++;
            return true;
        }
        if (((insn >> 21) & 0x7FF) == 0x550) { // ORR 64-bit, shift 00
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>(dec.rm);
            uint64_t nv = (n == 31) ? 0 : regs_[n];
            uint64_t mv = (m == 31) ? 0 : regs_[m];
            if (d != 31) regs_[d] = nv | mv;
            pc_ += 4;
            steps_++;
            return true;
        }
        return false; // opcode fora do escopo: chamador decide (TODO: SVC, LDR/STR, MMU)
    }

private:
    std::array<uint64_t, REG_COUNT> regs_{};
    uint64_t sp_ = 0;
    uint64_t pc_ = 0;
    uint64_t steps_ = 0;
};

} // namespace cpu
} // namespace port
