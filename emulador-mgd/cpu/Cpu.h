#pragma once

// CPU do emulador MGD: interpretador ARM64 mínimo, decodificando com
// o Field MGD (peça do Eden transformada). Cada instrução nova entra
// com teste. Sem MMU, sem SVC, sem NEON — isso vem depois.

#include <array>
#include <cstdint>
#include <vector>

#include "../../ports/mario-odissey/src/mgd/common/BitField.h"

namespace mgd {
namespace emu {

union ArmInsn {
    uint32_t hex;
    mgd::Field<0, 5, uint32_t> rd;
    mgd::Field<5, 5, uint32_t> rn;
    mgd::Field<10, 12, uint32_t> imm12;
    mgd::Field<16, 5, uint32_t> rm;
    mgd::Field<21, 2, uint32_t> hw;
    mgd::Field<0, 26, int32_t> off26;
};

class Cpu {
public:
    static constexpr int REG_COUNT = 31;

    explicit Cpu(uint64_t ramSize = 64 * 1024) { reset(ramSize); }

    void reset(uint64_t ramSize = 64 * 1024) {
        regs_.fill(0);
        mem_.assign(static_cast<size_t>(ramSize), 0);
        sp_ = 0;
        pc_ = 0;
        steps_ = 0;
    }

    uint64_t reg(int i) const { return (i >= 0 && i < REG_COUNT) ? regs_[i] : 0; }
    void setReg(int i, uint64_t v) { if (i >= 0 && i < REG_COUNT) regs_[i] = v; }
    uint64_t sp() const { return sp_; }
    void setSp(uint64_t v) { sp_ = v; }
    uint64_t pc() const { return pc_; }
    void setPc(uint64_t v) { pc_ = v; }
    uint64_t steps() const { return steps_; }
    uint8_t* ram() { return mem_.data(); }
    uint64_t ramSize() const { return static_cast<uint64_t>(mem_.size()); }

    uint64_t run(uint64_t maxSteps) {
        uint64_t done = 0;
        while (done < maxSteps) {
            if (pc_ + 4 > ramSize()) break;
            uint32_t insn = 0;
            for (int i = 0; i < 4; i++)
                insn |= static_cast<uint32_t>(mem_[static_cast<size_t>(pc_) + i]) << (8 * i);
            if (!step(insn)) break;
            done++;
        }
        return done;
    }

    // B, MOVZ, ADD/SUB imediato, ORR reg, LDR/STR imediato 64-bit.
    bool step(uint32_t insn) {
        ArmInsn dec;
        dec.hex = insn;
        if ((insn >> 26) == 0x05) { // B
            int32_t off = static_cast<int32_t>(dec.off26);
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
        if (top == 0x91 || top == 0xD1) { // ADD / SUB imediato
            bool isAdd = (top == 0x91);
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            uint32_t imm = static_cast<uint32_t>(dec.imm12);
            if (insn & 0x00400000) imm <<= 12;
            uint64_t nv = (n == 31) ? 0 : regs_[n];
            uint64_t res = isAdd ? (nv + imm) : (nv - imm);
            if (d != 31) regs_[d] = res;
            pc_ += 4;
            steps_++;
            return true;
        }
        if (((insn >> 21) & 0x7FF) == 0x550) { // ORR 64-bit
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
        if (top == 0xF8 || top == 0xF9) { // STR / LDR Xt,[Xn,#imm*8]
            bool isLoad = (top == 0xF9);
            int t = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            uint64_t base = (n == 31) ? sp_ : regs_[n];
            uint64_t addr = base + static_cast<uint64_t>(dec.imm12) * 8u;
            if (addr + 8 > ramSize()) return false;
            if (isLoad) {
                uint64_t v = 0;
                for (int i = 0; i < 8; i++)
                    v |= static_cast<uint64_t>(mem_[static_cast<size_t>(addr) + i]) << (8 * i);
                if (t != 31) regs_[t] = v;
            } else {
                uint64_t v = (t == 31) ? 0 : regs_[t];
                for (int i = 0; i < 8; i++)
                    mem_[static_cast<size_t>(addr) + i] = static_cast<uint8_t>(v >> (8 * i));
            }
            pc_ += 4;
            steps_++;
            return true;
        }
        return false;
    }

private:
    std::array<uint64_t, REG_COUNT> regs_{};
    std::vector<uint8_t> mem_;
    uint64_t sp_ = 0;
    uint64_t pc_ = 0;
    uint64_t steps_ = 0;
};

} // namespace emu
} // namespace mgd
