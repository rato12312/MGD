#pragma once

#include <array>
#include <cstdint>

namespace port {
namespace cpu {

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
        if ((insn >> 26) == 0x05) { // B
            int32_t off = static_cast<int32_t>(insn & 0x03FFFFFF);
            if (off & 0x02000000) off |= ~0x03FFFFFF; // sign extend 26
            pc_ = static_cast<uint64_t>(static_cast<int64_t>(pc_) + (static_cast<int64_t>(off) << 2));
            steps_++;
            return true;
        }
        uint8_t top = static_cast<uint8_t>(insn >> 24);
        if ((top & 0xFC) == 0xD2) { // MOVZ 64-bit
            int d = static_cast<int>(insn & 0x1F);
            uint16_t imm = static_cast<uint16_t>((insn >> 5) & 0xFFFF);
            int shift = static_cast<int>(((insn >> 21) & 0x3)) * 16;
            setReg(d, static_cast<uint64_t>(imm) << shift);
            pc_ += 4;
            steps_++;
            return true;
        }
        if (top == 0x91 || top == 0xD1) { // ADD / SUB imediato 64-bit
            bool isAdd = (top == 0x91);
            int d = static_cast<int>(insn & 0x1F);
            int n = static_cast<int>((insn >> 5) & 0x1F);
            uint32_t imm = (insn >> 10) & 0xFFF;
            if (insn & 0x00400000) imm <<= 12; // LSL #12
            uint64_t nv = (n == 31) ? 0 : regs_[n];
            uint64_t res = isAdd ? (nv + imm) : (nv - imm);
            if (d != 31) regs_[d] = res;
            pc_ += 4;
            steps_++;
            return true;
        }
        if (((insn >> 21) & 0x7FF) == 0x550) { // ORR 64-bit, shift 00
            int d = static_cast<int>(insn & 0x1F);
            int n = static_cast<int>((insn >> 5) & 0x1F);
            int m = static_cast<int>((insn >> 16) & 0x1F);
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
