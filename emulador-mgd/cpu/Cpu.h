#pragma once

// CPU do emulador MGD: interpretador ARM64 mínimo, decodificando com
// o Field MGD (peça do Eden transformada). Cada instrução nova entra
// com teste. Sem MMU, sem SVC, sem NEON — isso vem depois.

#include <array>
#include <cstdint>
#include <vector>

#include "../../ports/mario-odissey/src/mgd/common/BitField.h"
#include "../ram/Mmu.h"
#include "../hos/Kernel.h"

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
        stopped_ = false;
        exit_code_ = 0;
        flag_n_ = flag_z_ = flag_c_ = flag_v_ = false;
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

    bool stopped() const { return stopped_; }
    uint64_t exitCode() const { return exit_code_; }

    void setMmu(Mmu* mmu) { mmu_ = mmu; }
    void setKernel(hos::Kernel* k) { kernel_ = k; }
    hos::SvcResult lastSvc() const { return last_svc_; }

    uint64_t run(uint64_t maxSteps) {
        uint64_t done = 0;
        while (done < maxSteps && !stopped_) {
            uint64_t pa = 0;
            if (!phys(pc_, 4, false, true, pa)) break; // fetch sem exec = para
            uint32_t insn = 0;
            for (int i = 0; i < 4; i++)
                insn |= static_cast<uint32_t>(mem_[static_cast<size_t>(pa) + i]) << (8 * i);
            if (!step(insn)) break;
            done++;
        }
        return done;
    }

    // B, MOVZ, ADD/SUB imediato, ORR reg, LDR/STR (64/32/8-bit),
    // SVC mínimo (#0 sai com 0, #1 sai com X0).
    bool step(uint32_t insn) {
        ArmInsn dec;
        dec.hex = insn;
        if ((insn & 0xFFE0001F) == 0xD4000001) { // SVC #imm
            uint32_t imm = (insn >> 5) & 0xFFFF;
            if (imm == 0) { stopped_ = true; exit_code_ = 0; }
            else if (imm == 1 && !kernel_) { stopped_ = true; exit_code_ = regs_[0] & 0xFF; }
            else if (kernel_) {
                // Chamada HOS: X0-X7 entram, OK devolve out em X0/X1,
                // erro devolve o código em X0 (convenção nossa, documentada).
                hos::SvcArgs args;
                for (int i = 0; i < 8; i++) args.x[i] = regs_[i];
                hos::SvcResult r = kernel_->call(imm, args);
                last_svc_ = r;
                if (r == hos::RESULT_OK) {
                    regs_[0] = args.out[0];
                    regs_[1] = args.out[1];
                } else {
                    regs_[0] = static_cast<uint64_t>(r);
                }
                if (kernel_->exited()) { stopped_ = true; exit_code_ = 0; }
            }
            else return false;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn >> 26) == 0x05) { // B
            int32_t off = static_cast<int32_t>(dec.off26); // já com sinal
            pc_ = static_cast<uint64_t>(static_cast<int64_t>(pc_) + (static_cast<int64_t>(off) << 2));
            steps_++;
            return true;
        }
        if ((insn >> 26) == 0x25) { // BL (chamada: X30 = retorno)
            int32_t off = static_cast<int32_t>(dec.off26);
            regs_[30] = pc_ + 4;
            pc_ = static_cast<uint64_t>(static_cast<int64_t>(pc_) + (static_cast<int64_t>(off) << 2));
            steps_++;
            return true;
        }
        if (top == 0x58) { // LDR Xt,[PC,#imm19*4]
            int t = static_cast<int>(dec.rd);
            int64_t imm = static_cast<int64_t>((insn >> 5) & 0x7FFFF);
            if (imm & 0x40000) imm |= ~static_cast<int64_t>(0x7FFFF);
            uint64_t addr = pc_ + static_cast<uint64_t>(imm << 2);
            bool ok = true;
            uint64_t v = load64(addr, ok);
            if (!ok) return false;
            if (t != 31) regs_[t] = v;
            pc_ += 4;
            steps_++;
            return true;
        }
        uint8_t top = static_cast<uint8_t>(insn >> 24);
        if ((top & 0xFC) == 0xD2 || (top & 0xFC) == 0x52) { // MOVZ 64/32-bit
            bool is64 = (top & 0xFC) == 0xD2;
            int d = static_cast<int>(dec.rd);
            uint16_t imm = static_cast<uint16_t>((dec.hex >> 5) & 0xFFFF);
            int shift = static_cast<int>(((dec.hex >> 21) & 0x3)) * 16;
            uint64_t v = static_cast<uint64_t>(imm) << shift;
            if (d != 31) regs_[d] = is64 ? v : (v & 0xFFFFFFFFull);
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((top & 0xFC) == 0x92 || (top & 0xFC) == 0x12) { // MOVN 64/32-bit
            bool is64 = (top & 0xFC) == 0x92;
            int d = static_cast<int>(dec.rd);
            uint16_t imm = static_cast<uint16_t>((dec.hex >> 5) & 0xFFFF);
            int shift = static_cast<int>(((dec.hex >> 21) & 0x3)) * 16;
            uint64_t v = ~(static_cast<uint64_t>(imm) << shift);
            if (d != 31) regs_[d] = is64 ? v : (v & 0xFFFFFFFFull);
            pc_ += 4;
            steps_++;
            return true;
        }
        if (top == 0x11 || top == 0x51 || top == 0x31 || top == 0x71) {
            // ADDW / SUBW / ADDSW / SUBSW (32-bit, zero-extend)
            bool isAdd = (top == 0x11 || top == 0x31);
            bool setFlags = (top == 0x31 || top == 0x71);
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            uint32_t imm = static_cast<uint32_t>(dec.imm12);
            if (insn & 0x00400000) imm <<= 12;
            uint32_t nv = (n == 31) ? 0 : static_cast<uint32_t>(regs_[n]);
            uint32_t res = isAdd ? (nv + imm) : (nv - imm);
            if (d != 31) regs_[d] = res;
            if (setFlags) {
                flag_n_ = (res >> 31) != 0;
                flag_z_ = (res == 0);
                if (isAdd) {
                    flag_c_ = res < nv;
                    bool sn = ((nv >> 31) != 0), si = ((imm >> 31) != 0), sr = flag_n_;
                    flag_v_ = (sn == si) && (sr != sn);
                } else {
                    flag_c_ = nv >= imm;
                    bool sn = ((nv >> 31) != 0), si = ((imm >> 31) != 0), sr = flag_n_;
                    flag_v_ = (sn != si) && (sr != sn);
                }
            }
            pc_ += 4;
            steps_++;
            return true;
        }
        if (top == 0x91 || top == 0xD1 || top == 0xB1 || top == 0xF1) {
            // ADD / SUB / ADDS / SUBS imediato 64-bit
            bool isAdd = (top == 0x91 || top == 0xB1);
            bool setFlags = (top == 0xB1 || top == 0xF1);
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            uint32_t imm = static_cast<uint32_t>(dec.imm12);
            if (insn & 0x00400000) imm <<= 12;
            uint64_t nv = (n == 31) ? (setFlags ? 0 : sp_) : regs_[n];
            uint64_t res = isAdd ? (nv + imm) : (nv - imm);
            if (d != 31) regs_[d] = res;
            else if (!setFlags && n == 31) sp_ = res; // ADD/SUB SP,SP,#imm
            if (setFlags) {
                flag_n_ = (res >> 63) != 0;
                flag_z_ = (res == 0);
                if (isAdd) {
                    flag_c_ = res < nv;
                    bool sn = ((nv >> 63) != 0), si = ((imm >> 31) != 0), sr = flag_n_;
                    flag_v_ = (sn == si) && (sr != sn);
                } else {
                    flag_c_ = nv >= imm;
                    bool sn = ((nv >> 63) != 0), si = ((imm >> 31) != 0), sr = flag_n_;
                    flag_v_ = (sn != si) && (sr != sn);
                }
            }
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((top & 0xFC) == 0xF2) { // MOVK 64-bit (mantém o resto)
            int d = static_cast<int>(dec.rd);
            uint64_t imm = static_cast<uint64_t>((dec.hex >> 5) & 0xFFFF);
            int shift = static_cast<int>(((dec.hex >> 21) & 0x3)) * 16;
            uint64_t old = (d == 31) ? 0 : regs_[d];
            uint64_t res = (old & ~(static_cast<uint64_t>(0xFFFF) << shift)) | (imm << shift);
            if (d != 31) regs_[d] = res;
            pc_ += 4;
            steps_++;
            return true;
        }
        if (top == 0x54) { // B.cond
            int64_t imm = static_cast<int64_t>((insn >> 5) & 0x7FFFF);
            if (imm & 0x40000) imm |= ~static_cast<int64_t>(0x7FFFF);
            bool take = false;
            switch (insn & 0xF) {
                case 0x0: take = flag_z_; break;
                case 0x1: take = !flag_z_; break;
                case 0x2: take = flag_c_; break;
                case 0x3: take = !flag_c_; break;
                case 0x4: take = flag_n_; break;
                case 0x5: take = !flag_n_; break;
                case 0x6: take = flag_v_; break;
                case 0x7: take = !flag_v_; break;
                case 0x8: take = flag_c_ && !flag_z_; break;
                case 0x9: take = !(flag_c_ && !flag_z_); break;
                case 0xA: take = flag_n_ == flag_v_; break;
                case 0xB: take = flag_n_ != flag_v_; break;
                case 0xC: take = !flag_z_ && (flag_n_ == flag_v_); break;
                case 0xD: take = flag_z_ || (flag_n_ != flag_v_); break;
                case 0xE: take = true; break;
                default: return false;
            }
            pc_ = take ? static_cast<uint64_t>(static_cast<int64_t>(pc_) + (imm << 2)) : pc_ + 4;
            steps_++;
            return true;
        }
        if (top == 0x36 || top == 0x37) { // TBZ / TBNZ Xt,#bit,label
            int t = static_cast<int>(dec.rd);
            int bit = static_cast<int>(((insn >> 31) & 0x1) * 32 + ((insn >> 19) & 0x1F));
            int64_t imm = static_cast<int64_t>((insn >> 5) & 0x3FFF);
            if (imm & 0x2000) imm |= ~static_cast<int64_t>(0x3FFF); // sign 14
            uint64_t v = (t == 31) ? 0 : regs_[t];
            bool isSet = ((v >> bit) & 1u) != 0;
            bool take = (top == 0x36) ? !isSet : isSet;
            pc_ = take ? static_cast<uint64_t>(static_cast<int64_t>(pc_) + (imm << 2)) : pc_ + 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFFE0) == 0xD53BD040) { // MRS Xd,TPIDR_EL0 (TLS)
            int d = static_cast<int>(dec.rd);
            if (d != 31) regs_[d] = tpidr_;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFFE0) == 0xD51BD040) { // MSR TPIDR_EL0,Xn
            int n = static_cast<int>((insn >> 5) & 0x1F);
            tpidr_ = (n == 31) ? 0 : regs_[n];
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFFE0) == 0xD53BE040) { // MRS Xd,CNTVCT_EL0 (timer)
            int d = static_cast<int>(dec.rd);
            if (d != 31) regs_[d] = steps_; // contador = instruções executadas
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFF000) == 0xD5033000) { // DMB/DSB/ISB: single-thread, só segue
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFE0FC00) == 0xC85FFC00) { // LDAXR Xt,[Xn]
            int t = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            uint64_t base = (n == 31) ? sp_ : regs_[n];
            bool ok = true;
            uint64_t v = load64(base, ok);
            if (!ok) return false;
            if (t != 31) regs_[t] = v;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFE0FC00) == 0xC8007C00) { // STLXR Ws,Xt,[Xn] (sempre vence)
            int s = static_cast<int>(dec.rd);
            int t = static_cast<int>((insn >> 16) & 0x1F);
            int n = static_cast<int>(dec.rn);
            uint64_t base = (n == 31) ? sp_ : regs_[n];
            if (!store64(base, (t == 31) ? 0 : regs_[t])) return false;
            if (s != 31) regs_[s] = 0;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFE03C00) == 0x1EE00000 || (insn & 0xFFE03C00) == 0x1EE02000 ||
            (insn & 0xFFE03C00) == 0x1EE03000 || (insn & 0xFFE03C00) == 0x1EE01000 ||
            (insn & 0xFFE03C00) == 0x1EE04000 || (insn & 0xFFE03C00) == 0x1EE05000) {
            // FMUL / FADD / FSUB / FDIV / FMAX / FMIN Dd,Dn,Dm
            uint32_t base = insn & 0xFFE03C00;
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            double a = fpregs_[n], b = fpregs_[m];
            double res = 0;
            if (base == 0x1EE02000) res = a + b;
            else if (base == 0x1EE03000) res = a - b;
            else if (base == 0x1EE00000) res = a * b;
            else if (base == 0x1EE04000) res = (a >= b) ? a : b;
            else if (base == 0x1EE05000) res = (a <= b) ? a : b;
            else res = a / b;
            fpregs_[d] = res;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFE0FC00) == 0x1E602000) { // FCMP Dn,Dm (flags; NaN = unordered)
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            double a = fpregs_[n], b = fpregs_[m];
            bool nan = (a != a) || (b != b);
            flag_n_ = !nan && (a < b);
            flag_z_ = !nan && (a == b);
            flag_c_ = nan || (a >= b);
            flag_v_ = nan;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFC0FC00) == 0x1E61C000 || (insn & 0xFFC0FC00) == 0x1E614000 ||
            (insn & 0xFFC0FC00) == 0x1E60C000) {
            // FSQRT / FNEG / FABS Dd,Dn
            uint32_t base = insn & 0xFFC0FC00;
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            double v = fpregs_[n];
            double res = (base == 0x1E61C000) ? __builtin_sqrt(v)
                       : (base == 0x1E614000) ? -v
                       : (v < 0 ? -v : v);
            fpregs_[d] = res;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFE03C00) == 0x1E200000 || (insn & 0xFFE03C00) == 0x1E202000 ||
            (insn & 0xFFE03C00) == 0x1E203000 || (insn & 0xFFE03C00) == 0x1E201000) {
            // FMUL / FADD / FSUB / FDIV Sd,Sn,Sm (float, precisão simples)
            uint32_t base = insn & 0xFFE03C00;
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            float a = static_cast<float>(fpregs_[n]);
            float b = static_cast<float>(fpregs_[m]);
            float res = 0;
            if (base == 0x1E202000) res = a + b;
            else if (base == 0x1E203000) res = a - b;
            else if (base == 0x1E200000) res = a * b;
            else res = a / b;
            fpregs_[d] = static_cast<double>(res);
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFC0FC00) == 0x1E624000) { // FCVT Sd,Dn (double->float)
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            fpregs_[d] = static_cast<double>(static_cast<float>(fpregs_[n]));
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFC0FC00) == 0x1E22C000) { // FCVT Dd,Sn (float->double)
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            fpregs_[d] = fpregs_[n]; // já guardado como double do float
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFE08000) == 0x1FE00000 || (insn & 0xFFE08000) == 0x1FE08000) {
            // FMADD / FMSUB Dd,Dn,Dm,Da (a*b +/- c, sem arredondar no meio)
            bool isSub = (insn & 0xFFE08000) == 0x1FE08000;
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            int a = static_cast<int>((insn >> 10) & 0x1F);
            double res = fpregs_[n] * fpregs_[m] + (isSub ? -fpregs_[a] : fpregs_[a]);
            fpregs_[d] = res;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFE00C00) == 0x1E600C00) { // FCSEL Dd,Dn,Dm,cond
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            int cond = static_cast<int>((insn >> 12) & 0xF);
            fpregs_[d] = condTrue(cond) ? fpregs_[n] : fpregs_[m];
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFC0FC00) == 0x1E604000) { // FMOV Dd,Dn
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            fpregs_[d] = fpregs_[n];
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFC0FC00) == 0x1E620000 || (insn & 0xFFC0FC00) == 0x1E630000) {
            // SCVTF / UCVTF Xd,Dn (double -> int64/uint64)
            bool isSigned = (insn & 0xFFC0FC00) == 0x1E620000;
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            double v = fpregs_[n];
            uint64_t res = isSigned ? static_cast<uint64_t>(static_cast<int64_t>(v))
                                    : static_cast<uint64_t>(v);
            if (d != 31) regs_[d] = res;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFC0FC00) == 0x1E660000 || (insn & 0xFFC0FC00) == 0x1E670000) {
            // SCVTF / UCVTF Dd,Xn (int -> double)
            bool isSigned = (insn & 0xFFC0FC00) == 0x1E660000;
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            uint64_t nv = (n == 31) ? 0 : regs_[n];
            fpregs_[d] = isSigned ? static_cast<double>(static_cast<int64_t>(nv))
                                  : static_cast<double>(nv);
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFE0FC00) == 0xDAC00C00 || (insn & 0xFFE0FC00) == 0xDAC00800 ||
            (insn & 0xFFE0FC00) == 0xDAC00400) {
            // REV / REV32 / REV16 Xd,Xn
            uint32_t base = insn & 0xFFE0FC00;
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            uint64_t v = (n == 31) ? 0 : regs_[n];
            uint64_t res = 0;
            if (base == 0xDAC00C00) { // REV: 8 bytes
                for (int i = 0; i < 8; i++)
                    res |= ((v >> (8 * i)) & 0xFFull) << (8 * (7 - i));
            } else if (base == 0xDAC00800) { // REV32: cada metade
                for (int h = 0; h < 2; h++) {
                    uint64_t half = (v >> (32 * h)) & 0xFFFFFFFFull;
                    uint64_t rh = 0;
                    for (int i = 0; i < 4; i++)
                        rh |= ((half >> (8 * i)) & 0xFFull) << (8 * (3 - i));
                    res |= rh << (32 * h);
                }
            } else { // REV16: cada par
                for (int h = 0; h < 4; h++) {
                    uint64_t phe = (v >> (16 * h)) & 0xFFFFull;
                    res |= (((phe & 0xFFull) << 8) | ((phe >> 8) & 0xFFull)) << (16 * h);
                }
            }
            if (d != 31) regs_[d] = res;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFE0FC00) == 0xDAC00000) { // RBIT Xd,Xn
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            uint64_t v = (n == 31) ? 0 : regs_[n];
            uint64_t res = 0;
            for (int i = 0; i < 64; i++)
                if ((v >> i) & 1ull) res |= 1ull << (63 - i);
            if (d != 31) regs_[d] = res;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFE00000) == 0xD3800000) { // EXTR Xd,Xn,Xm,#lsb
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            int lsb = static_cast<int>((insn >> 10) & 0x3F);
            uint64_t nv = (n == 31) ? 0 : regs_[n];
            uint64_t mv = (m == 31) ? 0 : regs_[m];
            uint64_t res = (lsb == 0) ? nv : ((nv >> lsb) | (mv << (64 - lsb)));
            if (d != 31) regs_[d] = res;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFE0FC00) == 0xDAC01000) { // CLZ Xd,Xn (0 -> 64)
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            uint64_t v = (n == 31) ? 0 : regs_[n];
            uint64_t c = 0;
            for (int i = 63; i >= 0; i--) {
                if ((v >> i) & 1ull) break;
                c++;
            }
            if (d != 31) regs_[d] = c;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFE00C00) == 0x9A800400) {
            // CSEL / CSINC / CSINV / CSNEG Xd,Xn,Xm,cond
            int op = static_cast<int>((insn >> 10) & 0x3);
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            int cond = static_cast<int>((insn >> 12) & 0xF);
            uint64_t nv = (n == 31) ? 0 : regs_[n];
            uint64_t mv = (m == 31) ? 0 : regs_[m];
            uint64_t res;
            if (condTrue(cond)) {
                res = nv;
            } else if (op == 0) {
                res = mv;
            } else if (op == 1) {
                res = mv + 1;
            } else if (op == 2) {
                res = ~mv;
            } else {
                res = ~mv + 1;
            }
            if (d != 31) regs_[d] = res;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFF01F) == 0xD503201F) { // NOP e HINTs: aceita e segue
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFC1F) == 0xD65F0000) { // RET Xn
            int n = static_cast<int>((insn >> 5) & 0x1F);
            pc_ = (n == 31) ? 0 : regs_[n];
            steps_++;
            return true;
        }
        if ((insn & 0xFF800000) == 0xD3000000 && (insn & 0x400000)) {
            // UBFM 64-bit (cobre LSL/LSR imediato): dst = ROR(src,R) & wmask
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            uint64_t r = (insn >> 16) & 0x3F, s = (insn >> 10) & 0x3F;
            auto ones = [](uint64_t k) { return k >= 64 ? ~0ull : ((1ull << k) - 1ull); };
            auto ror = [](uint64_t x, uint64_t rot) {
                rot %= 64;
                return rot == 0 ? x : ((x >> rot) | (x << (64 - rot)));
            };
            uint64_t wmask = ror(ones(s + 1), r);
            uint64_t src = (n == 31) ? 0 : regs_[n];
            if (d != 31) regs_[d] = ror(src, r) & wmask;
            pc_ += 4;
            steps_++;
            return true;
        }
        if (((insn & 0xFF200000) == 0x0B000000 || (insn & 0xFF200000) == 0x4B000000 ||
             (insn & 0xFF200000) == 0x0A000000 || (insn & 0xFF200000) == 0x2A000000 ||
             (insn & 0xFF200000) == 0x4A000000) && ((insn >> 22) & 0x3) == 0x0) {
            // ADDW/SUBW/ANDW/ORRW/EORW com LSL #n (32-bit, zero-extend)
            uint32_t grp = insn & 0xFF200000;
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>(dec.rm);
            int sh = static_cast<int>((insn >> 10) & 0x1F);
            uint32_t nv = (n == 31) ? 0 : static_cast<uint32_t>(regs_[n]);
            uint32_t mv = (m == 31) ? 0 : static_cast<uint32_t>(regs_[m]);
            uint32_t sv = (sh == 0) ? mv : (mv << sh);
            uint32_t res = 0;
            if (grp == 0x0B000000) res = nv + sv;
            else if (grp == 0x4B000000) res = nv - sv;
            else if (grp == 0x0A000000) res = nv & sv;
            else if (grp == 0x2A000000) res = nv | sv;
            else res = nv ^ sv;
            if (d != 31) regs_[d] = res;
            pc_ += 4;
            steps_++;
            return true;
        }
        if (((insn & 0xFF200000) == 0x8B000000 || (insn & 0xFF200000) == 0xCB000000) &&
            ((insn >> 22) & 0x3) == 0x0) {
            // ADD / SUB 64-bit registrado com LSL #n (cobre NEG/MOV SP)
            bool isAdd = (insn & 0xFF200000) == 0x8B000000;
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>(dec.rm);
            int sh = static_cast<int>((insn >> 10) & 0x3F);
            uint64_t nv = (n == 31) ? sp_ : regs_[n]; // Rn=31 aqui é SP
            uint64_t mv = (m == 31) ? 0 : regs_[m];
            uint64_t sv = (sh == 0) ? mv : (mv << sh);
            uint64_t res = isAdd ? (nv + sv) : (nv - sv);
            if (d != 31) regs_[d] = res;
            else if (n == 31) sp_ = res; // ADD/SUB SP,SP,Xm
            pc_ += 4;
            steps_++;
            return true;
        }
        if (((insn & 0xFF200000) == 0x8A200000 || (insn & 0xFF200000) == 0xAA200000 ||
             (insn & 0xFF200000) == 0xCA200000) && ((insn >> 22) & 0x3) == 0x0) {
            // BIC / ORN / EON 64-bit com LSL #n (segundo operando invertido)
            int opc = static_cast<int>((insn >> 29) & 0x3);
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>(dec.rm);
            int sh = static_cast<int>((insn >> 10) & 0x3F);
            uint64_t nv = (n == 31) ? 0 : regs_[n];
            uint64_t mv = (m == 31) ? 0 : regs_[m];
            uint64_t sv = ~((sh == 0) ? mv : (mv << sh));
            uint64_t res = (opc == 0) ? (nv & sv) : (opc == 1) ? (nv | sv) : (nv ^ sv);
            if (d != 31) regs_[d] = res;
            pc_ += 4;
            steps_++;
            return true;
        }
        if (((insn & 0xFF200000) == 0x8A000000 || (insn & 0xFF200000) == 0xAA000000 ||
             (insn & 0xFF200000) == 0xCA000000) && ((insn >> 22) & 0x3) == 0x0) {
            // AND / ORR / EOR 64-bit com LSL #n
            int opc = static_cast<int>((insn >> 29) & 0x3);
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>(dec.rm);
            int sh = static_cast<int>((insn >> 10) & 0x3F);
            uint64_t nv = (n == 31) ? 0 : regs_[n];
            uint64_t mv = (m == 31) ? 0 : regs_[m];
            uint64_t sv = (sh == 0) ? mv : (mv << sh);
            uint64_t res = (opc == 0) ? (nv & sv) : (opc == 1) ? (nv | sv) : (nv ^ sv);
            if (d != 31) regs_[d] = res;
            pc_ += 4;
            steps_++;
            return true;
        }
        if (top == 0xF8 || top == 0xF9) { // STR / LDR Xt,[Xn,#imm*8]
            if (top == 0xF9 && ((insn >> 22) & 0x3) == 0x0) { // PRFM: aceita e segue
                pc_ += 4;
                steps_++;
                return true;
            }
            return memAccess(dec, 8, top == 0xF9);
        }
        if ((insn & 0xFFE0FC00) == 0x9AC02000 || (insn & 0xFFE0FC00) == 0x9AC02400 ||
            (insn & 0xFFE0FC00) == 0x9AC02800 || (insn & 0xFFE0FC00) == 0x9AC02C00) {
            // LSLV / LSRV / ASRV / RORV Xd,Xn,Xm (shift = Xm % 64)
            int op = static_cast<int>((insn >> 10) & 0x3);
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            uint64_t nv = (n == 31) ? 0 : regs_[n];
            uint64_t sh = ((m == 31) ? 0 : regs_[m]) % 64;
            uint64_t res = nv;
            if (op == 0) res = (sh == 0) ? nv : (nv << sh);
            else if (op == 1) res = (sh == 0) ? nv : (nv >> sh);
            else if (op == 2) res = (sh == 0) ? nv : static_cast<uint64_t>(static_cast<int64_t>(nv) >> sh);
            else res = (sh == 0) ? nv : ((nv >> sh) | (nv << (64 - sh)));
            if (d != 31) regs_[d] = res;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFE0FC00) == 0xF8206800 || (insn & 0xFFE0FC00) == 0xF8207800 ||
            (insn & 0xFFE0FC00) == 0xF8606800 || (insn & 0xFFE0FC00) == 0xF8607800) {
            // STR / LDR Xt,[Xn,Xm{,LSL #3}] (offset registrado, option=LSL)
            uint32_t opt = insn & 0xFFE0FC00;
            bool isLoad = (opt == 0xF8606800 || opt == 0xF8607800);
            int t = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            int s = static_cast<int>((insn >> 12) & 0x1);
            uint64_t base = (n == 31) ? sp_ : regs_[n];
            uint64_t off = (m == 31) ? 0 : regs_[m];
            uint64_t addr = base + (s ? (off << 3) : off);
            uint64_t pa = 0;
            if (!phys(addr, 8, !isLoad, false, pa)) return false;
            if (isLoad) {
                uint64_t v = 0;
                for (int i = 0; i < 8; i++)
                    v |= static_cast<uint64_t>(mem_[static_cast<size_t>(pa) + i]) << (8 * i);
                if (t != 31) regs_[t] = v;
            } else {
                uint64_t v = (t == 31) ? 0 : regs_[t];
                for (int i = 0; i < 8; i++)
                    mem_[static_cast<size_t>(pa) + i] = static_cast<uint8_t>(v >> (8 * i));
            }
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFC00000) == 0xFD000000 || (insn & 0xFFC00000) == 0xFD400000) {
            // STR / LDR Dn,[Xn,#imm*8] (double bit-exato)
            bool isLoad = (insn & 0xFFC00000) == 0xFD400000;
            int t = static_cast<int>(insn & 0x1F);
            int n = static_cast<int>((insn >> 5) & 0x1F);
            uint64_t base = (n == 31) ? sp_ : regs_[n];
            uint64_t addr = base + static_cast<uint64_t>((insn >> 10) & 0xFFF) * 8u;
            uint64_t pa = 0;
            if (!phys(addr, 8, !isLoad, false, pa)) return false;
            if (isLoad) {
                uint64_t v = 0;
                for (int i = 0; i < 8; i++)
                    v |= static_cast<uint64_t>(mem_[static_cast<size_t>(pa) + i]) << (8 * i);
                fpregs_[t] = u2d(v);
            } else {
                uint64_t v = d2u(fpregs_[t]);
                for (int i = 0; i < 8; i++)
                    mem_[static_cast<size_t>(pa) + i] = static_cast<uint8_t>(v >> (8 * i));
            }
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFC00000) == 0x6D000000 || (insn & 0xFFC00000) == 0x6D400000) {
            // STP / LDP Dd1,Dd2,[Xn,#imm*8] (par double, offset)
            bool isLoad = (insn & 0xFFC00000) == 0x6D400000;
            int t1 = static_cast<int>(insn & 0x1F);
            int n = static_cast<int>((insn >> 5) & 0x1F);
            int t2 = static_cast<int>((insn >> 10) & 0x1F);
            int64_t off = static_cast<int64_t>((insn >> 15) & 0x7F);
            if (off & 0x40) off |= ~static_cast<int64_t>(0x7F);
            uint64_t base = (n == 31) ? sp_ : regs_[n];
            uint64_t addr = base + static_cast<uint64_t>(off * 8);
            uint64_t pa = 0;
            if (!phys(addr, 16, !isLoad, false, pa)) return false;
            auto ld = [&](uint64_t a) {
                uint64_t v = 0;
                for (int i = 0; i < 8; i++)
                    v |= static_cast<uint64_t>(mem_[static_cast<size_t>(a) + i]) << (8 * i);
                return v;
            };
            auto st = [&](uint64_t a, uint64_t v) {
                for (int i = 0; i < 8; i++)
                    mem_[static_cast<size_t>(a) + i] = static_cast<uint8_t>(v >> (8 * i));
            };
            if (isLoad) {
                fpregs_[t1] = u2d(ld(pa));
                fpregs_[t2] = u2d(ld(pa + 8));
            } else {
                st(pa, d2u(fpregs_[t1]));
                st(pa + 8, d2u(fpregs_[t2]));
            }
            pc_ += 4;
            steps_++;
            return true;
        }
        if (top == 0xB8 || top == 0xB9) { // STRW / LDRW / LDRSW
            int opc = static_cast<int>((insn >> 22) & 0x3);
            if (top == 0xB8 && opc == 0x2) { // LDRSW Xt (estende sinal)
                int t = static_cast<int>(dec.rd);
                int n = static_cast<int>(dec.rn);
                uint64_t base = (n == 31) ? sp_ : regs_[n];
                uint64_t addr = base + static_cast<uint64_t>(dec.imm12) * 4u;
                uint64_t pa = 0;
                if (!phys(addr, 4, false, false, pa)) return false;
                uint32_t w = 0;
                for (int i = 0; i < 4; i++)
                    w |= static_cast<uint32_t>(mem_[static_cast<size_t>(pa) + i]) << (8 * i);
                if (t != 31) regs_[t] = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(w)));
                pc_ += 4;
                steps_++;
                return true;
            }
            return memAccess(dec, 4, top == 0xB9);
        }
        if (top == 0x38 || top == 0x39) { // STRB / LDRB (byte)
            return memAccess(dec, 1, top == 0x39);
        }
        if ((insn & 0x9F000000) == 0x90000000) { // ADRP Xd, page
            int d = static_cast<int>(dec.rd);
            int64_t imm = (static_cast<int64_t>((insn >> 5) & 0x7FFFF) << 2) |
                          static_cast<int64_t>((insn >> 29) & 0x3);
            if (imm & 0x100000) imm |= ~static_cast<int64_t>(0x1FFFFF); // sign 21
            uint64_t page = (pc_ & ~static_cast<uint64_t>(0xFFF)) +
                            static_cast<uint64_t>(imm << 12);
            setReg(d, page);
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFC00000) == 0xA9000000 || (insn & 0xFFC00000) == 0xA9400000 ||
            (insn & 0xFFC00000) == 0xA9800000 || (insn & 0xFFC00000) == 0xA9C00000 ||
            (insn & 0xFFC00000) == 0xA8800000 || (insn & 0xFFC00000) == 0xA8C00000) {
            // STP / LDP Xt1,Xt2,[Xn,#imm] offset, pré e pós-index
            bool isLoad = (insn & 0x400000) != 0;
            bool preIndex = ((insn >> 23) & 0x3) == 0x1;
            bool postIndex = ((insn >> 23) & 0x3) == 0x0;
            int t1 = static_cast<int>(insn & 0x1F);
            int n = static_cast<int>((insn >> 5) & 0x1F);
            int t2 = static_cast<int>((insn >> 10) & 0x1F);
            int64_t off = static_cast<int64_t>((insn >> 15) & 0x7F);
            if (off & 0x40) off |= ~static_cast<int64_t>(0x7F); // sign 7
            uint64_t base = (n == 31) ? sp_ : regs_[n];
            uint64_t addr = base + (preIndex ? static_cast<uint64_t>(off * 8) : 0);
            if (isLoad) {
                bool ok1 = true, ok2 = true;
                uint64_t v1 = load64(addr, ok1), v2 = load64(addr + 8, ok2);
                if (!ok1 || !ok2) return false;
                if (t1 != 31) regs_[t1] = v1;
                if (t2 != 31) regs_[t2] = v2;
            } else {
                if (!store64(addr, (t1 == 31) ? 0 : regs_[t1])) return false;
                if (!store64(addr + 8, (t2 == 31) ? 0 : regs_[t2])) return false;
            }
            if (preIndex || postIndex) { // writeback na base (ou SP)
                uint64_t nb = base + static_cast<uint64_t>(off * 8);
                if (n == 31) sp_ = nb;
                else regs_[n] = nb;
            }
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFE0FC00) == 0x9AC00C00 || (insn & 0xFFE0FC00) == 0x9A800C00) {
            // SDIV / UDIV Xd,Xn,Xm (div por zero = 0, sem trap)
            bool isSigned = (insn & 0xFFE0FC00) == 0x9AC00C00;
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            uint64_t nv = (n == 31) ? 0 : regs_[n];
            uint64_t mv = (m == 31) ? 0 : regs_[m];
            uint64_t res = 0;
            if (mv != 0) {
                if (isSigned) {
                    int64_t sn = static_cast<int64_t>(nv), sm = static_cast<int64_t>(mv);
                    if (!(sn == INT64_MIN && sm == -1)) res = static_cast<uint64_t>(sn / sm);
                    else res = static_cast<uint64_t>(INT64_MIN); // overflow: quociente
                } else {
                    res = nv / mv;
                }
            }
            if (d != 31) regs_[d] = res;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFE08000) == 0x9B200000 || (insn & 0xFFE08000) == 0x9BA00000) {
            // SMADDL / UMADDL Xd,Wn,Wm,Xa (32->64 com sinal ou não)
            bool isSigned = (insn & 0xFFE08000) == 0x9B200000;
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            int a = static_cast<int>((insn >> 10) & 0x1F);
            uint64_t av = (a == 31) ? 0 : regs_[a];
            uint64_t prod;
            if (isSigned) {
                int64_t sn = static_cast<int64_t>(static_cast<int32_t>((n == 31) ? 0 : regs_[n]));
                int64_t sm = static_cast<int64_t>(static_cast<int32_t>((m == 31) ? 0 : regs_[m]));
                prod = static_cast<uint64_t>(sn * sm);
            } else {
                uint64_t un = (n == 31) ? 0 : (regs_[n] & 0xFFFFFFFFull);
                uint64_t um = (m == 31) ? 0 : (regs_[m] & 0xFFFFFFFFull);
                prod = un * um;
            }
            if (d != 31) regs_[d] = av + prod;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFE0FC00) == 0x1AC00400 || (insn & 0xFFE0FC00) == 0x1AC00800 ||
            (insn & 0xFFE0FC00) == 0x1AC00C00 || (insn & 0xFFE0FC00) == 0x9AC00400) {
            // CRC32B/H/W/X Wd,Wn,Wm (refletido, sem init/xor — como o ARM)
            uint32_t base = insn & 0xFFE0FC00;
            int bytes = (base == 0x1AC00400) ? 1 : (base == 0x1AC00800) ? 2
                        : (base == 0x9AC00400) ? 8 : 4;
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            uint32_t crc = (n == 31) ? 0 : static_cast<uint32_t>(regs_[n]);
            uint64_t mv = (m == 31) ? 0 : regs_[m];
            for (int i = 0; i < bytes; i++) {
                uint32_t byte = static_cast<uint32_t>((mv >> (8 * i)) & 0xFFu);
                crc ^= byte;
                for (int b = 0; b < 8; b++)
                    crc = (crc & 1u) ? ((crc >> 1) ^ 0xEDB88320u) : (crc >> 1);
            }
            if (d != 31) regs_[d] = (static_cast<uint64_t>(crc) & 0xFFFFFFFFull);
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFE0FC00) == 0xBA000000 || (insn & 0xFFE0FC00) == 0xFA000000) {
            // ADCS / SBCS Xd,Xn,Xm (com carry, atualiza flags)
            bool isAdd = (insn & 0xFFE0FC00) == 0xBA000000;
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            uint64_t nv = (n == 31) ? 0 : regs_[n];
            uint64_t mv = (m == 31) ? 0 : regs_[m];
            uint64_t c = flag_c_ ? 1ull : 0ull;
            __uint128_t wide;
            if (isAdd) wide = static_cast<__uint128_t>(nv) + mv + c;
            else wide = static_cast<__uint128_t>(nv) - mv - (flag_c_ ? 0u : 1u);
            uint64_t res = static_cast<uint64_t>(wide);
            if (d != 31) regs_[d] = res;
            flag_n_ = (res >> 63) != 0;
            flag_z_ = (res == 0);
            bool wrapped = static_cast<uint64_t>(wide >> 64) != 0;
            flag_c_ = isAdd ? wrapped : !wrapped; // sub: C = sem borrow
            flag_v_ = isAdd ? (~(nv ^ mv) & (nv ^ res)) >> 63
                            : ((nv ^ mv) & (nv ^ res)) >> 63;
            flag_v_ = flag_v_ != 0;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFE08000) == 0x9B008000) { // MADD Xd,Xn,Xm,Xa
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            int a = static_cast<int>((insn >> 10) & 0x1F);
            uint64_t nv = (n == 31) ? 0 : regs_[n];
            uint64_t mv = (m == 31) ? 0 : regs_[m];
            uint64_t av = (a == 31) ? 0 : regs_[a];
            if (d != 31) regs_[d] = av + nv * mv;
            pc_ += 4;
            steps_++;
            return true;
        }
        if (top == 0xB4 || top == 0xB5) { // CBZ / CBNZ Xt, label
            int t = static_cast<int>(dec.rd);
            int64_t imm = static_cast<int64_t>((insn >> 5) & 0x7FFFF);
            if (imm & 0x40000) imm |= ~static_cast<int64_t>(0x7FFFF); // sign 19
            uint64_t v = (t == 31) ? 0 : regs_[t];
            bool take = (top == 0xB4) ? (v == 0) : (v != 0);
            pc_ = take ? static_cast<uint64_t>(static_cast<int64_t>(pc_) + (imm << 2)) : pc_ + 4;
            steps_++;
            return true;
        }
        return false;
    }

private:
    bool condTrue(int cond) const {
        switch (cond & 0xF) {
            case 0x0: return flag_z_;
            case 0x1: return !flag_z_;
            case 0x2: return flag_c_;
            case 0x3: return !flag_c_;
            case 0x4: return flag_n_;
            case 0x5: return !flag_n_;
            case 0x6: return flag_v_;
            case 0x7: return !flag_v_;
            case 0x8: return flag_c_ && !flag_z_;
            case 0x9: return !(flag_c_ && !flag_z_);
            case 0xA: return flag_n_ == flag_v_;
            case 0xB: return flag_n_ != flag_v_;
            case 0xC: return !flag_z_ && (flag_n_ == flag_v_);
            case 0xD: return flag_z_ || (flag_n_ != flag_v_);
            case 0xE: return true;
            default: return false;
        }
    }

    // VA -> PA (ou direto se sem MMU). false = fault.
    bool phys(uint64_t va, int size, bool w, bool x, uint64_t& pa) const {
        if (!mmu_) {
            if (va + static_cast<uint64_t>(size) > ramSize()) return false;
            pa = va;
            return true;
        }
        if (!mmu_->translate(va, static_cast<uint64_t>(size), w, x, pa)) return false;
        if (pa + static_cast<uint64_t>(size) > ramSize()) return false;
        return true;
    }

    uint64_t load64(uint64_t addr, bool& ok) const {
        uint64_t pa = 0;
        ok = phys(addr, 8, false, false, pa);
        if (!ok) return 0;
        uint64_t v = 0;
        for (int i = 0; i < 8; i++)
            v |= static_cast<uint64_t>(mem_[static_cast<size_t>(pa) + i]) << (8 * i);
        return v;
    }
    bool store64(uint64_t addr, uint64_t v) {
        uint64_t pa = 0;
        if (!phys(addr, 8, true, false, pa)) return false;
        for (int i = 0; i < 8; i++)
            mem_[static_cast<size_t>(pa) + i] = static_cast<uint8_t>(v >> (8 * i));
        return true;
    }

    bool memAccess(const ArmInsn& dec, int width, bool isLoad) {
        int t = static_cast<int>(dec.rd);
        int n = static_cast<int>(dec.rn);
        uint64_t base = (n == 31) ? sp_ : regs_[n];
        uint64_t addr = base + static_cast<uint64_t>(dec.imm12) * static_cast<uint64_t>(width);
        uint64_t pa = 0;
        if (!phys(addr, width, !isLoad, false, pa)) return false;
        if (isLoad) {
            uint64_t v = 0;
            for (int i = 0; i < width; i++)
                v |= static_cast<uint64_t>(mem_[static_cast<size_t>(pa) + i]) << (8 * i);
            if (t != 31) regs_[t] = v; // 32/8-bit já vêm zerados acima
        } else {
            uint64_t v = (t == 31) ? 0 : regs_[t];
            for (int i = 0; i < width; i++)
                mem_[static_cast<size_t>(pa) + i] = static_cast<uint8_t>(v >> (8 * i));
        }
        pc_ += 4;
        steps_++;
        return true;
    }

    static uint64_t d2u(double d) {
        uint64_t u = 0;
        __builtin_memcpy(&u, &d, 8);
        return u;
    }
    static double u2d(uint64_t u) {
        double d = 0;
        __builtin_memcpy(&d, &u, 8);
        return d;
    }

    std::array<uint64_t, REG_COUNT> regs_{};
    std::array<double, 32> fpregs_{};
    std::vector<uint8_t> mem_;
    Mmu* mmu_ = nullptr;
    hos::Kernel* kernel_ = nullptr;
    hos::SvcResult last_svc_ = hos::RESULT_OK;
    uint64_t sp_ = 0;
    uint64_t pc_ = 0;
    uint64_t steps_ = 0;
    uint64_t tpidr_ = 0;
    bool stopped_ = false;
    uint64_t exit_code_ = 0;
    bool flag_n_ = false, flag_z_ = false, flag_c_ = false, flag_v_ = false;
};

} // namespace emu
} // namespace mgd
