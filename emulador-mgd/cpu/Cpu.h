#pragma once

// CPU do emulador MGD: interpretador ARM64 mínimo, decodificando com
// o Field MGD (peça do Eden transformada). Cada instrução nova entra
// com teste. Sem MMU, sem SVC, sem NEON — isso vem depois.

#include <array>
#include <cstdint>
#include <vector>

#include "../../ports/mario-odissey/src/mgd/common/BitField.h"
#include "../ram/Mmu.h"

namespace mgd {
namespace emu {

// Dono das SVCs (o Kernel HOS implementa). Quebra o ciclo Cpu<->Kernel.
struct SvcHost {
    virtual ~SvcHost() = default;
    virtual uint32_t svcCall(uint32_t num, uint64_t x[8], uint64_t out[2]) = 0;
    virtual bool svcExited() const = 0;
};

union ArmInsn {
    uint32_t hex;
    ::port::mgd::Field<0, 5, uint32_t> rd;
    ::port::mgd::Field<5, 5, uint32_t> rn;
    ::port::mgd::Field<10, 12, uint32_t> imm12;
    ::port::mgd::Field<16, 5, uint32_t> rm;
    ::port::mgd::Field<21, 2, uint32_t> hw;
    ::port::mgd::Field<0, 26, int32_t> off26;
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
        fpcr_ = 0;
        fpsr_ = 0;
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
    const uint8_t* ram() const { return mem_.data(); }
    uint64_t ramSize() const { return static_cast<uint64_t>(mem_.size()); }

    bool stopped() const { return stopped_; }
    uint64_t exitCode() const { return exit_code_; }
    // Diagnóstico: últimos opcodes desconhecidos (diz o que implementar).
    uint64_t unknownCount() const { return unknown_total_; }
    uint32_t lastUnknown(size_t i) const {
        return i < unknown_log_.size() ? unknown_log_[i] : 0;
    }
    // Backtrace: segue X29 ([fp]=prev, [fp+8]=ret). Físico direto, com teto.
    std::vector<uint64_t> backtrace(size_t max = 32) const {
        std::vector<uint64_t> out;
        uint64_t fp = regs_[29];
        for (size_t i = 0; i < max && fp != 0; i++) {
            if (fp + 16 > ramSize() || (fp & 7) != 0) break;
            uint64_t prev = 0, ret = 0;
            __builtin_memcpy(&prev, &mem_[static_cast<size_t>(fp)], 8);
            __builtin_memcpy(&ret, &mem_[static_cast<size_t>(fp) + 8], 8);
            if (ret == 0) break;
            out.push_back(ret);
            if (prev == fp) break; // não anda em círculo
            fp = prev;
        }
        return out;
    }
    // Trace: últimos PCs executados (debug de código real).
    void traceEnable(bool on) { trace_on_ = on; }
    uint64_t tracePc(size_t i) const {
        return i < trace_.size() ? trace_[(trace_pos_ + trace_.size() - 1 - i) % trace_.size()] : 0;
    }

    struct State {
        std::array<uint64_t, REG_COUNT> regs{};
        std::array<double, 32> fpregs{};
        uint64_t sp = 0, pc = 0, steps = 0, tpidr = 0;
        uint64_t fpcr = 0, fpsr = 0;
        uint64_t sctlr = 0, ttbr0 = 0, ttbr1 = 0, tcr = 0;
        uint64_t mair = 0, vbar = 0, cpacr = 0;
        // Exception state (per-EL)
        uint64_t elr_el1 = 0, spsr_el1 = 0;
        uint64_t elr_el2 = 0, spsr_el2 = 0;
        uint64_t elr_el3 = 0, spsr_el3 = 0;
        uint64_t esr = 0, far = 0;
        uint64_t current_el = 1; // start at EL1
        uint64_t sctlr_el1 = 0, sctlr_el2 = 0, sctlr_el3 = 0;
        uint64_t vbar_el1 = 0, vbar_el2 = 0, vbar_el3 = 0;
        bool n = false, z = false, c = false, v = false;
        // Timer state
        uint64_t cntp_ctl = 0, cntp_cval = 0, cntp_tval = 0, cntpct = 0;
        uint64_t cntv_ctl = 0, cntv_cval = 0, cntv_tval = 0, cntvct = 0;
        bool cntp_enabled = false, cntv_enabled = false;
        // Interrupt state
        bool irq_mask = true, fiq_mask = true;
        bool serror_pending = false;
    };
    State save() const {
        State s;
        s.regs = regs_;
        s.fpregs = fp_.d;
        s.sp = sp_; s.pc = pc_; s.steps = steps_; s.tpidr = tpidr_;
        s.fpcr = fpcr_; s.fpsr = fpsr_;
        s.sctlr = sctlr_; s.ttbr0 = ttbr0_; s.ttbr1 = ttbr1_; s.tcr = tcr_;
        s.mair = mair_; s.vbar = vbar_; s.cpacr = cpacr_;
        s.elr_el1 = elr_el1_; s.spsr_el1 = spsr_el1_;
        s.elr_el2 = elr_el2_; s.spsr_el2 = spsr_el2_;
        s.elr_el3 = elr_el3_; s.spsr_el3 = spsr_el3_;
        s.esr = esr_; s.far = far_;
        s.current_el = current_el_;
        s.sctlr_el1 = sctlr_el1_; s.sctlr_el2 = sctlr_el2_; s.sctlr_el3 = sctlr_el3_;
        s.vbar_el1 = vbar_el1_; s.vbar_el2 = vbar_el2_; s.vbar_el3 = vbar_el3_;
        s.n = flag_n_; s.z = flag_z_; s.c = flag_c_; s.v = flag_v_;
        s.cntp_ctl = cntp_ctl_; s.cntp_cval = cntp_cval_; s.cntp_tval = cntp_tval_; s.cntpct = cntpct_;
        s.cntv_ctl = cntv_ctl_; s.cntv_cval = cntv_cval_; s.cntv_tval = cntv_tval_; s.cntvct = cntvct_;
        s.cntp_enabled = cntp_enabled_; s.cntv_enabled = cntv_enabled_;
        s.irq_mask = irq_mask_; s.fiq_mask = fiq_mask_;
        s.serror_pending = serror_pending_;
        return s;
    }
    void load(const State& s) {
        regs_ = s.regs;
        fp_.d = s.fpregs;
        sp_ = s.sp; pc_ = s.pc; steps_ = s.steps; tpidr_ = s.tpidr;
        fpcr_ = s.fpcr; fpsr_ = s.fpsr;
        sctlr_ = s.sctlr; ttbr0_ = s.ttbr0; ttbr1_ = s.ttbr1; tcr_ = s.tcr;
        mair_ = s.mair; vbar_ = s.vbar; cpacr_ = s.cpacr;
        elr_el1_ = s.elr_el1; spsr_el1_ = s.spsr_el1;
        elr_el2_ = s.elr_el2; spsr_el2_ = s.spsr_el2;
        elr_el3_ = s.elr_el3; spsr_el3_ = s.spsr_el3;
        esr_ = s.esr; far_ = s.far;
        current_el_ = s.current_el;
        sctlr_el1_ = s.sctlr_el1; sctlr_el2_ = s.sctlr_el2; sctlr_el3_ = s.sctlr_el3;
        vbar_el1_ = s.vbar_el1; vbar_el2_ = s.vbar_el2; vbar_el3_ = s.vbar_el3;
        flag_n_ = s.n; flag_z_ = s.z; flag_c_ = s.c; flag_v_ = s.v;
        cntp_ctl_ = s.cntp_ctl; cntp_cval_ = s.cntp_cval; cntp_tval_ = s.cntp_tval; cntpct_ = s.cntpct;
        cntv_ctl_ = s.cntv_ctl; cntv_cval_ = s.cntv_cval; cntv_tval_ = s.cntv_tval; cntvct_ = s.cntvct;
        cntp_enabled_ = s.cntp_enabled; cntv_enabled_ = s.cntv_enabled;
        irq_mask_ = s.irq_mask; fiq_mask_ = s.fiq_mask;
        serror_pending_ = s.serror_pending;
        stopped_ = false;
        exit_code_ = 0;
    }

    void setMmu(Mmu* mmu) { mmu_ = mmu; }
    void setSvcHost(SvcHost* h) { svc_host_ = h; }
    // Mantido por compatibilidade com os testes (aponta para o host).
    void setKernel(SvcHost* h) { svc_host_ = h; }
    uint32_t lastSvc() const { return last_svc_; }

    // External interrupt injection (for HOS services, device emulation)
    void triggerIrq(uint64_t source = 0) { irq_pending_vec_.push_back(source); }
    void triggerFiq(uint64_t source = 0) { fiq_pending_vec_.push_back(source); }
    void triggerSError() { serror_pending_ = true; }
    bool isIrqPending() const { return !irq_pending_vec_.empty(); }
    bool isFiqPending() const { return !fiq_pending_vec_.empty(); }
    bool isSErrorPending() const { return serror_pending_; }
    // Timer access for HOS
    uint64_t getCntpCval() const { return cntp_cval_; }
    uint64_t getCntpct() const { return cntpct_; }
    uint64_t getCntvct() const { return cntvct_; }
    uint64_t getCntpCtl() const { return cntp_ctl_; }
    void setCntpCval(uint64_t v) { cntp_cval_ = v; }
    void setCntpCtl(uint64_t v) { cntp_ctl_ = v; cntp_enabled_ = (v & 1) != 0; }

    uint64_t run(uint64_t maxSteps) {
        uint64_t done = 0;
        while (done < maxSteps && !stopped_) {
            uint64_t pa = 0;
            if (!phys(pc_, 4, false, true, pa)) break; // fetch sem exec = para
            uint32_t insn = 0;
            __builtin_memcpy(&insn, &mem_[static_cast<size_t>(pa)], 4);
            checkInterrupts();
            if (!step(insn)) break;
            done++;
        }
        return done;
    }

    // B, MOVZ, ADD/SUB imediato, ORR reg, LDR/STR (64/32/8-bit),
    // SVC mínimo (#0 sai com 0, #1 sai com X0).
    bool step(uint32_t insn) {
        if (trace_on_) {
            trace_[trace_pos_ % trace_.size()] = pc_;
            trace_pos_++;
        }
        ArmInsn dec;
        dec.hex = insn;
        uint8_t top = static_cast<uint8_t>(insn >> 24);
        if ((insn & 0xFFE0001F) == 0xD4000001) { // SVC #imm
            uint32_t imm = (insn >> 5) & 0xFFFF;
            if (imm == 0) { stopped_ = true; exit_code_ = 0; }
            else if (imm == 1 && !svc_host_) { stopped_ = true; exit_code_ = regs_[0] & 0xFF; }
            else if (svc_host_) {
                // Chamada HOS: X0-X7 entram, OK devolve out em X0/X1,
                // erro devolve o código em X0 (convenção nossa, documentada).
                uint64_t xin[8], xout[2] = {0, 0};
                for (int i = 0; i < 8; i++) xin[i] = regs_[i];
                uint32_t r = svc_host_->svcCall(imm, xin, xout);
                last_svc_ = r;
                if (r == 0) {
                    regs_[0] = xout[0];
                    regs_[1] = xout[1];
                } else {
                    regs_[0] = static_cast<uint64_t>(r);
                }
                if (svc_host_->svcExited()) { stopped_ = true; exit_code_ = 0; }
            }
            else return false;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFE0001F) == 0xD4000021) { // HVC #imm (Hypervisor Call -> EL2)
            uint32_t imm = (insn >> 5) & 0xFFFF;
            // Exception entry to EL2
            spsr_ = packPstate();
            elr_ = pc_ + 4;
            esr_ = 0x56000000 | (imm & 0xFFFF); // EC=0x16 (HVC)
            current_el_ = 2;
            pc_ = vbar_el2_ + 0x600; // HVC vector offset
            steps_++;
            return true;
        }
        if ((insn & 0xFFE0001F) == 0xD4000041) { // SMC #imm (Secure Monitor Call -> EL3)
            uint32_t imm = (insn >> 5) & 0xFFFF;
            spsr_ = packPstate();
            elr_ = pc_ + 4;
            esr_ = 0x5C000000 | (imm & 0xFFFF); // EC=0x17 (SMC)
            current_el_ = 3;
            pc_ = vbar_el3_ + 0x600; // SMC vector offset
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFFFF) == 0xD65F03E0) { // ERET (Exception Return)
            // Return from exception: restore PSTATE from SPSR, PC from ELR
            uint64_t spsr = getSpsrForEl(current_el_);
            uint64_t elr = getElrForEl(current_el_);
            unpackPstate(spsr);
            pc_ = elr;
            // Determine target EL from SPSR.M[3:0]
            uint64_t target_el = (spsr >> 2) & 0x3;
            if (target_el <= 3) current_el_ = target_el;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFC1F) == 0xD518401F || (insn & 0xFFFFFC1F) == 0xD518403F ||
            (insn & 0xFFFFFC1F) == 0xD518405F || (insn & 0xFFFFFC1F) == 0xD518407F) {
            // MSR ELR_EL1/EL2/EL3, Xn
            uint32_t base = insn & 0xFFFFFC1F;
            int n = static_cast<int>((insn >> 5) & 0x1F);
            uint64_t v = (n == 31) ? 0 : regs_[n];
            uint64_t el = 1;
            if (base == 0xD518401F) el = 1;
            else if (base == 0xD518403F) el = 2;
            else el = 3;
            getElrForEl(el) = v;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFC1F) == 0xD518409F || (insn & 0xFFFFFC1F) == 0xD51840BF ||
            (insn & 0xFFFFFC1F) == 0xD51840DF || (insn & 0xFFFFFC1F) == 0xD51840FF) {
            // MSR SPSR_EL1/EL2/EL3, Xn
            uint32_t base = insn & 0xFFFFFC1F;
            int n = static_cast<int>((insn >> 5) & 0x1F);
            uint64_t v = (n == 31) ? 0 : regs_[n];
            uint64_t el = 1;
            if (base == 0xD518409F) el = 1;
            else if (base == 0xD51840BF) el = 2;
            else el = 3;
            getSpsrForEl(el) = v;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFC1F) == 0xD518421F || (insn & 0xFFFFFC1F) == 0xD518423F ||
            (insn & 0xFFFFFC1F) == 0xD518425F || (insn & 0xFFFFFC1F) == 0xD518427F) {
            // MSR ESR_EL1/EL2/EL3, Xn
            uint32_t base = insn & 0xFFFFFC1F;
            int n = static_cast<int>((insn >> 5) & 0x1F);
            uint64_t v = (n == 31) ? 0 : regs_[n];
            if (base == 0xD518421F) esr_ = v;
            else if (base == 0xD518423F) { /* ESR_EL2 */ }
            else { /* ESR_EL3 */ }
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFC1F) == 0xD518429F || (insn & 0xFFFFFC1F) == 0xD51842BF ||
            (insn & 0xFFFFFC1F) == 0xD51842DF || (insn & 0xFFFFFC1F) == 0xD51842FF) {
            // MSR FAR_EL1/EL2/EL3, Xn
            uint32_t base = insn & 0xFFFFFC1F;
            int n = static_cast<int>((insn >> 5) & 0x1F);
            uint64_t v = (n == 31) ? 0 : regs_[n];
            if (base == 0xD518429F) far_ = v;
            else if (base == 0xD51842BF) { /* FAR_EL2 */ }
            else { /* FAR_EL3 */ }
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFC1F) == 0xD518441F) { // MSR VBAR_EL1, Xn
            int n = static_cast<int>((insn >> 5) & 0x1F);
            vbar_el1_ = (n == 31) ? 0 : regs_[n];
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFC1F) == 0xD518443F) { // MSR VBAR_EL2, Xn
            int n = static_cast<int>((insn >> 5) & 0x1F);
            vbar_el2_ = (n == 31) ? 0 : regs_[n];
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFC1F) == 0xD518445F) { // MSR VBAR_EL3, Xn
            int n = static_cast<int>((insn >> 5) & 0x1F);
            vbar_el3_ = (n == 31) ? 0 : regs_[n];
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFC1F) == 0xD538401F || (insn & 0xFFFFFC1F) == 0xD538403F ||
            (insn & 0xFFFFFC1F) == 0xD538405F || (insn & 0xFFFFFC1F) == 0xD538407F) {
            // MRS Xd, ELR_EL1/EL2/EL3
            uint32_t base = insn & 0xFFFFFC1F;
            int d = static_cast<int>(dec.rd);
            uint64_t v = 0;
            if (base == 0xD538401F) v = getElrForEl(1);
            else if (base == 0xD538403F) v = getElrForEl(2);
            else v = getElrForEl(3);
            if (d != 31) regs_[d] = v;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFC1F) == 0xD538409F || (insn & 0xFFFFFC1F) == 0xD53840BF ||
            (insn & 0xFFFFFC1F) == 0xD53840DF || (insn & 0xFFFFFC1F) == 0xD53840FF) {
            // MRS Xd, SPSR_EL1/EL2/EL3
            uint32_t base = insn & 0xFFFFFC1F;
            int d = static_cast<int>(dec.rd);
            uint64_t v = 0;
            if (base == 0xD538409F) v = getSpsrForEl(1);
            else if (base == 0xD53840BF) v = getSpsrForEl(2);
            else v = getSpsrForEl(3);
            if (d != 31) regs_[d] = v;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFC1F) == 0xD538421F || (insn & 0xFFFFFC1F) == 0xD538423F ||
            (insn & 0xFFFFFC1F) == 0xD538425F || (insn & 0xFFFFFC1F) == 0xD538427F) {
            // MRS Xd, ESR_EL1/EL2/EL3
            uint32_t base = insn & 0xFFFFFC1F;
            int d = static_cast<int>(dec.rd);
            uint64_t v = 0;
            if (base == 0xD538421F) v = esr_;
            else if (base == 0xD538423F) v = 0;
            else v = 0;
            if (d != 31) regs_[d] = v;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFC1F) == 0xD538429F || (insn & 0xFFFFFC1F) == 0xD53842BF ||
            (insn & 0xFFFFFC1F) == 0xD53842DF || (insn & 0xFFFFFC1F) == 0xD53842FF) {
            // MRS Xd, FAR_EL1/EL2/EL3
            uint32_t base = insn & 0xFFFFFC1F;
            int d = static_cast<int>(dec.rd);
            uint64_t v = 0;
            if (base == 0xD538429F) v = far_;
            else if (base == 0xD53842BF) v = 0;
            else v = 0;
            if (d != 31) regs_[d] = v;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFC1F) == 0xD538441F) { // MRS Xd, VBAR_EL1
            int d = static_cast<int>(dec.rd);
            if (d != 31) regs_[d] = vbar_el1_;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFC1F) == 0xD538443F) { // MRS Xd, VBAR_EL2
            int d = static_cast<int>(dec.rd);
            if (d != 31) regs_[d] = vbar_el2_;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFC1F) == 0xD538445F) { // MRS Xd, VBAR_EL3
            int d = static_cast<int>(dec.rd);
            if (d != 31) regs_[d] = vbar_el3_;
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
        if (top == 0xD2 || top == 0x52) { // MOVZ 64/32-bit
            bool is64 = top == 0xD2;
            int d = static_cast<int>(dec.rd);
            uint16_t imm = static_cast<uint16_t>((dec.hex >> 5) & 0xFFFF);
            int shift = static_cast<int>(((dec.hex >> 21) & 0x3)) * 16;
            uint64_t v = static_cast<uint64_t>(imm) << shift;
            if (d != 31) regs_[d] = is64 ? v : (v & 0xFFFFFFFFull);
            pc_ += 4;
            steps_++;
            return true;
        }
        if (top == 0x92 || top == 0x12) { // MOVN 64/32-bit
            bool is64 = top == 0x92;
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
        if (top == 0xF2) { // MOVK 64-bit (mantém o resto)
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
        if ((insn & 0xFFFFFFE0) == 0xD53B4200) { // MRS Xd,NZCV (empacota flags)
            int d = static_cast<int>(dec.rd);
            uint64_t v = (flag_n_ ? (1ull << 31) : 0) | (flag_z_ ? (1ull << 30) : 0) |
                         (flag_c_ ? (1ull << 29) : 0) | (flag_v_ ? (1ull << 28) : 0);
            if (d != 31) regs_[d] = v;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFC1F) == 0xD513421F) { // MSR NZCV,Xn (desempacota)
            int n = static_cast<int>((insn >> 5) & 0x1F);
            uint64_t v = (n == 31) ? 0 : regs_[n];
            flag_n_ = ((v >> 31) & 1u) != 0;
            flag_z_ = ((v >> 30) & 1u) != 0;
            flag_c_ = ((v >> 29) & 1u) != 0;
            flag_v_ = ((v >> 28) & 1u) != 0;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFFE0) == 0xD53BD060) { // MRS Xd,TPIDRRO_EL0
            int d = static_cast<int>(dec.rd);
            if (d != 31) regs_[d] = tpidr_; // single-thread: mesmo TLS
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFFE0) == 0xD53BE000) { // MRS Xd,CNTFRQ_EL0 (19.2MHz)
            int d = static_cast<int>(dec.rd);
            if (d != 31) regs_[d] = 19200000ull;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFFE0) == 0xD53B4400) { // MRS Xd,FPCR (guardado)
            int d = static_cast<int>(dec.rd);
            if (d != 31) regs_[d] = fpcr_;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFFE0) == 0xD5134400) { // MSR FPCR,Xn (sem efeito real)
            int n = static_cast<int>((insn >> 5) & 0x1F);
            fpcr_ = (n == 31) ? 0 : regs_[n];
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFFE0) == 0xD53B4420) { // MRS Xd,FPSR (guardado)
            int d = static_cast<int>(dec.rd);
            if (d != 31) regs_[d] = fpsr_;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFFE0) == 0xD5134420) { // MSR FPSR,Xn (sem efeito real)
            int n = static_cast<int>((insn >> 5) & 0x1F);
            fpsr_ = (n == 31) ? 0 : regs_[n];
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFFE0) == 0xD53B4220) { // MRS Xd,CurrentEL (sempre EL1)
            int d = static_cast<int>(dec.rd);
            if (d != 31) regs_[d] = 0x4;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFFE0) == 0xD53B1000 || (insn & 0xFFFFFFE0) == 0xD53B2000 ||
            (insn & 0xFFFFFFE0) == 0xD53B2020 || (insn & 0xFFFFFFE0) == 0xD53B2040 ||
            (insn & 0xFFFFFFE0) == 0xD53BA200 || (insn & 0xFFFFFFE0) == 0xD53BC000 ||
            (insn & 0xFFFFFFE0) == 0xD53B1020) {
            // MRS Xd, {SCTLR,TTBR0,TTBR1,TCR,MAIR,VBAR,CPACR}_EL1 (guardados)
            uint32_t base = insn & 0xFFFFFFE0;
            int d = static_cast<int>(dec.rd);
            uint64_t v = 0;
            if (base == 0xD53B1000) v = sctlr_;
            else if (base == 0xD53B2000) v = ttbr0_;
            else if (base == 0xD53B2020) v = ttbr1_;
            else if (base == 0xD53B2040) v = tcr_;
            else if (base == 0xD53BA200) v = mair_;
            else if (base == 0xD53BC000) v = vbar_;
            else v = cpacr_;
            if (d != 31) regs_[d] = v;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFC1F) == 0xD513101F || (insn & 0xFFFFFC1F) == 0xD513201F ||
            (insn & 0xFFFFFC1F) == 0xD513203F || (insn & 0xFFFFFC1F) == 0xD513205F ||
            (insn & 0xFFFFFC1F) == 0xD513A21F || (insn & 0xFFFFFC1F) == 0xD513C01F ||
            (insn & 0xFFFFFC1F) == 0xD513103F) {
            // MSR {SCTLR,TTBR0,TTBR1,TCR,MAIR,VBAR,CPACR}_EL1,Xn (guardados)
            uint32_t base = insn & 0xFFFFFC1F;
            int n = static_cast<int>((insn >> 5) & 0x1F);
            uint64_t v = (n == 31) ? 0 : regs_[n];
            if (base == 0xD513101F) sctlr_ = v;
            else if (base == 0xD513201F) ttbr0_ = v;
            else if (base == 0xD513203F) ttbr1_ = v;
            else if (base == 0xD513205F) tcr_ = v;
            else if (base == 0xD513A21F) mair_ = v;
            else if (base == 0xD513C01F) vbar_ = v;
            else cpacr_ = v;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFFE0) == 0xD53B0000) { // MRS Xd,MIDR_EL1 (Cortex-A57)
            int d = static_cast<int>(dec.rd);
            if (d != 31) regs_[d] = 0x410FD070ull;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFFE0) == 0xD53B0020) { // MRS Xd,CTR_EL0 (64B linhas)
            int d = static_cast<int>(dec.rd);
            if (d != 31) regs_[d] = 0x8444C004ull;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFFE0) == 0xD53B00E0) { // MRS Xd,DCZID_EL0 (BS=4)
            int d = static_cast<int>(dec.rd);
            if (d != 31) regs_[d] = 4;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFFE0) == 0xD53BE060) { // MRS Xd,CNTPCT_EL0 (timer)
            int d = static_cast<int>(dec.rd);
            if (d != 31) regs_[d] = steps_;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFE1F) == 0xD500401F) { // MSR SPSEL,#0/1 (aceita)
            pc_ += 4;
            steps_++;
            return true;
        }
        if (insn == 0xD508871F) { // TLBI VMALLE1 (aceita, sem paginação real)
            pc_ += 4;
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
        if ((insn & 0xFFFFFC1F) == 0xD51BD01F) { // MSR TPIDR_EL0,Xn
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
        // EL1 Physical Timer registers
        if ((insn & 0xFFFFFC1F) == 0xD518D21F) { // MSR CNTP_TVAL_EL0, Xn
            int n = static_cast<int>((insn >> 5) & 0x1F);
            cntp_tval_ = (n == 31) ? 0 : regs_[n];
            cntp_cval_ = steps_ + cntp_tval_;
            cntp_enabled_ = (cntp_ctl_ & 1) != 0;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFFE0) == 0xD53BD220) { // MRS Xd, CNTP_TVAL_EL0
            int d = static_cast<int>(dec.rd);
            if (d != 31) {
                uint64_t remaining = (cntp_cval_ > steps_) ? (cntp_cval_ - steps_) : 0;
                regs_[d] = remaining;
            }
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFC1F) == 0xD518D25F) { // MSR CNTP_CTL_EL0, Xn
            int n = static_cast<int>((insn >> 5) & 0x1F);
            cntp_ctl_ = (n == 31) ? 0 : regs_[n];
            cntp_enabled_ = (cntp_ctl_ & 1) != 0;
            if (cntp_enabled_) cntp_cval_ = steps_ + cntp_tval_;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFFE0) == 0xD53BD260) { // MRS Xd, CNTP_CTL_EL0
            int d = static_cast<int>(dec.rd);
            if (d != 31) regs_[d] = cntp_ctl_;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFC1F) == 0xD518D23F) { // MSR CNTP_CVAL_EL0, Xn
            int n = static_cast<int>((insn >> 5) & 0x1F);
            cntp_cval_ = (n == 31) ? 0 : regs_[n];
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFFE0) == 0xD53BD240) { // MRS Xd, CNTP_CVAL_EL0
            int d = static_cast<int>(dec.rd);
            if (d != 31) regs_[d] = cntp_cval_;
            pc_ += 4;
            steps_++;
            return true;
        }
        // EL1 Virtual Timer registers
        if ((insn & 0xFFFFFC1F) == 0xD518D29F) { // MSR CNTV_TVAL_EL0, Xn
            int n = static_cast<int>((insn >> 5) & 0x1F);
            cntv_tval_ = (n == 31) ? 0 : regs_[n];
            cntv_cval_ = steps_ + cntv_tval_;
            cntv_enabled_ = (cntv_ctl_ & 1) != 0;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFFE0) == 0xD53BD2A0) { // MRS Xd, CNTV_TVAL_EL0
            int d = static_cast<int>(dec.rd);
            if (d != 31) {
                uint64_t remaining = (cntv_cval_ > steps_) ? (cntv_cval_ - steps_) : 0;
                regs_[d] = remaining;
            }
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFC1F) == 0xD518D2DF) { // MSR CNTV_CTL_EL0, Xn
            int n = static_cast<int>((insn >> 5) & 0x1F);
            cntv_ctl_ = (n == 31) ? 0 : regs_[n];
            cntv_enabled_ = (cntv_ctl_ & 1) != 0;
            if (cntv_enabled_) cntv_cval_ = steps_ + cntv_tval_;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFFE0) == 0xD53BD2E0) { // MRS Xd, CNTV_CTL_EL0
            int d = static_cast<int>(dec.rd);
            if (d != 31) regs_[d] = cntv_ctl_;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFC1F) == 0xD518D2BF) { // MSR CNTV_CVAL_EL0, Xn
            int n = static_cast<int>((insn >> 5) & 0x1F);
            cntv_cval_ = (n == 31) ? 0 : regs_[n];
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFFE0) == 0xD53BD2C0) { // MRS Xd, CNTV_CVAL_EL0
            int d = static_cast<int>(dec.rd);
            if (d != 31) regs_[d] = cntv_cval_;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFF000) == 0xD5033000) { // DMB/DSB/ISB: single-thread, só segue
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFC00) == 0xC85FFC00 || (insn & 0xFFFFFC00) == 0xC85F7C00) {
            // LDAR / LDAXR Xt,[Xn] (single-thread: mesma coisa)
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
        if ((insn & 0xFFFFFC00) == 0xC81FFC00) { // STLR Xt,[Xn]
            int t = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            uint64_t base = (n == 31) ? sp_ : regs_[n];
            if (!store64(base, (t == 31) ? 0 : regs_[t])) return false;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFE0FC00) == 0xC8A0BC00) { // SWPAL Xt,Xs,[Xn]
            int t = static_cast<int>(dec.rd);
            int s = static_cast<int>((insn >> 16) & 0x1F);
            int n = static_cast<int>(dec.rn);
            uint64_t base = (n == 31) ? sp_ : regs_[n];
            bool ok = true;
            uint64_t old = load64(base, ok);
            if (!ok) return false;
            if (!store64(base, (s == 31) ? 0 : regs_[s])) return false;
            if (t != 31) regs_[t] = old;
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
        if ((insn & 0xFFE0FC00) == 0x1EE00000 || (insn & 0xFFE0FC00) == 0x1EE02000 ||
            (insn & 0xFFE0FC00) == 0x1EE03000 || (insn & 0xFFE0FC00) == 0x1EE01000 ||
            (insn & 0xFFE0FC00) == 0x1EE04000 || (insn & 0xFFE0FC00) == 0x1EE05000 ||
            (insn & 0xFFE0FC00) == 0x1EE08000) {
            // FMUL / FADD / FSUB / FDIV / FMAX / FMIN / FNMUL Dd,Dn,Dm
            uint32_t base = insn & 0xFFE0FC00;
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            double a = fp_.d[n], b = fp_.d[m];
            double res = 0;
            if (base == 0x1EE02000) res = a + b;
            else if (base == 0x1EE03000) res = a - b;
            else if (base == 0x1EE00000) res = a * b;
            else if (base == 0x1EE04000) res = (a >= b) ? a : b;
            else if (base == 0x1EE05000) res = (a <= b) ? a : b;
            else if (base == 0x1EE08000) res = -(a * b);
            else res = a / b;
            fp_.d[d] = res;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFE0FC00) == 0x1E602000) { // FCMP Dn,Dm (flags; NaN = unordered)
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            double a = fp_.d[n], b = fp_.d[m];
            bool nan = (a != a) || (b != b);
            flag_n_ = !nan && (a < b);
            flag_z_ = !nan && (a == b);
            flag_c_ = nan || (a >= b);
            flag_v_ = nan;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFC00) == 0x1E61C000 || (insn & 0xFFFFFC00) == 0x1E614000 ||
            (insn & 0xFFFFFC00) == 0x1E60C000) {
            // FSQRT / FNEG / FABS Dd,Dn
            uint32_t base = insn & 0xFFFFFC00;
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            double v = fp_.d[n];
            double res = (base == 0x1E61C000) ? __builtin_sqrt(v)
                       : (base == 0x1E614000) ? -v
                       : (v < 0 ? -v : v);
            fp_.d[d] = res;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFE03C00) == 0x1E200000 || (insn & 0xFFE03C00) == 0x1E202000 ||
            (insn & 0xFFE03C00) == 0x1E203000 || (insn & 0xFFE03C00) == 0x1E201000) {
            // FMUL / FADD / FSUB / FDIV Sd,Sn,Sm (float, precisão simples)
            uint32_t base = insn & 0xFFE0FC00;
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            float a = static_cast<float>(fp_.d[n]);
            float b = static_cast<float>(fp_.d[m]);
            float res = 0;
            if (base == 0x1E202000) res = a + b;
            else if (base == 0x1E203000) res = a - b;
            else if (base == 0x1E200000) res = a * b;
            else res = a / b;
            fp_.d[d] = static_cast<double>(res);
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFC00) == 0x1E624000) { // FCVT Sd,Dn (double->float)
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            fp_.d[d] = static_cast<double>(static_cast<float>(fp_.d[n]));
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFC00) == 0x1E22C000) { // FCVT Dd,Sn (float->double)
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            fp_.d[d] = fp_.d[n]; // já guardado como double do float
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
            double res = fp_.d[n] * fp_.d[m] + (isSub ? -fp_.d[a] : fp_.d[a]);
            fp_.d[d] = res;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFE00C00) == 0x1E600C00) { // FCSEL Dd,Dn,Dm,cond
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            int cond = static_cast<int>((insn >> 12) & 0xF);
            fp_.d[d] = condTrue(cond) ? fp_.d[n] : fp_.d[m];
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFE0FC00) == 0x6E60D400 || (insn & 0xFFE0FC00) == 0x6E60DC00 ||
            (insn & 0xFFE0FC00) == 0x6E60CC00 || (insn & 0xFFE0FC00) == 0x6E60FC00) {
            // FADD / FSUB / FMUL / FDIV Vd.2D,Vn.2D,Vm.2D (opcodes [15:10])
            uint32_t base = insn & 0xFFE0FC00;
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            for (int lane = 0; lane < 2; lane++) {
                double a = u2d(fp_.q[n][lane]), b = u2d(fp_.q[m][lane]);
                double r = 0;
                if (base == 0x6E60D400) r = a + b;
                else if (base == 0x6E60DC00) r = a - b;
                else if (base == 0x6E60CC00) r = a * b;
                else r = a / b;
                fp_.q[d][lane] = d2u(r);
            }
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFF9FFC00) == 0x6E008400 || (insn & 0xFF9FFC00) == 0x6E008C00) {
            // ADD / SUB vetorial inteiro: size diz as lanes (8b/16b/32b/64b)
            bool isAdd = (insn & 0xFF9FFC00) == 0x6E008400;
            int sz = static_cast<int>((insn >> 21) & 0x3);
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            int lanes = 16 >> sz;          // 16, 8, 4, 2
            int bits = 8 << sz;            // 8, 16, 32, 64
            uint64_t mask = bits == 64 ? ~0ull : ((1ull << bits) - 1ull);
            uint64_t an[2] = {fp_.q[n][0], fp_.q[n][1]};
            uint64_t bn[2] = {fp_.q[m][0], fp_.q[m][1]};
            for (int lane = 0; lane < lanes; lane++) {
                int half = lane / (64 / bits);
                int shift = (lane * bits) % 64;
                uint64_t a = (an[half] >> shift) & mask;
                uint64_t b = (bn[half] >> shift) & mask;
                uint64_t r = (isAdd ? (a + b) : (a - b)) & mask;
                fp_.q[d][half] = (fp_.q[d][half] & ~(mask << shift)) | (r << shift);
            }
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFE0FC00) == 0x6E20CC00 || (insn & 0xFFE0FC00) == 0x6E20D000) {
            // FMLA / FMLS Vd.2D,Vn.2D,Vm.2D (d +=/-= n*m por lane)
            bool isSub = (insn & 0xFFE0FC00) == 0x6E20D000;
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            for (int lane = 0; lane < 2; lane++) {
                double r = u2d(fp_.q[d][lane]) + (isSub ? -1.0 : 1.0) * u2d(fp_.q[n][lane]) *
                                                        u2d(fp_.q[m][lane]);
                fp_.q[d][lane] = d2u(r);
            }
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFE0FC00) == 0x6E600400 || (insn & 0xFFE0FC00) == 0x6E601400) {
            // FMAX / FMIN Vd.2D,Vn.2D,Vm.2D (NaN: fica o outro, estilo maxNum)
            bool isMax = (insn & 0xFFE0FC00) == 0x6E600400;
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            for (int lane = 0; lane < 2; lane++) {
                double a = u2d(fp_.q[n][lane]), b = u2d(fp_.q[m][lane]);
                double r;
                if (a != a) r = b;
                else if (b != b) r = a;
                else r = isMax ? ((a >= b) ? a : b) : ((a <= b) ? a : b);
                fp_.q[d][lane] = d2u(r);
            }
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFE0FC00) == 0x6E602400) { // FCMEQ Vd.2D,Vn.2D,Vm.2D
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            for (int lane = 0; lane < 2; lane++) {
                double a = u2d(fp_.q[n][lane]), b = u2d(fp_.q[m][lane]);
                fp_.q[d][lane] = (a == b) ? ~0ull : 0ull;
            }
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFE0FC00) == 0x6E601C00) { // BSL Vd,Vn,Vm: d=(n&m)|(d&~m)
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            for (int lane = 0; lane < 2; lane++) {
                uint64_t dd = fp_.q[d][lane];
                fp_.q[d][lane] = (fp_.q[n][lane] & fp_.q[m][lane]) | (dd & ~fp_.q[m][lane]);
            }
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFF9FFC00) == 0x6E009C00) {
            // MUL vetorial inteiro size-aware (8b/16b/32b; 64b não existe)
            int sz = static_cast<int>((insn >> 21) & 0x3);
            if (sz == 3) return false; // deixa p/ log de desconhecido
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            int lanes = 16 >> sz;
            int bits = 8 << sz;
            uint64_t mask = (bits == 64) ? ~0ull : ((1ull << bits) - 1ull);
            uint64_t an[2] = {fp_.q[n][0], fp_.q[n][1]};
            uint64_t bn[2] = {fp_.q[m][0], fp_.q[m][1]};
            for (int lane = 0; lane < lanes; lane++) {
                int half = lane / (64 / bits);
                int shift = (lane * bits) % 64;
                uint64_t a = (an[half] >> shift) & mask;
                uint64_t b = (bn[half] >> shift) & mask;
                uint64_t r = (a * b) & mask;
                fp_.q[d][half] = (fp_.q[d][half] & ~(mask << shift)) | (r << shift);
            }
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFE0FC00) == 0x6E201C00) { // ORR Vd.16B,Vn,Vm (move 128)
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            uint64_t a0 = fp_.q[n][0] | fp_.q[m][0];
            uint64_t a1 = fp_.q[n][1] | fp_.q[m][1];
            fp_.q[d][0] = a0;
            fp_.q[d][1] = a1;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFC00) == 0x1E580000 || (insn & 0xFFFFFC00) == 0x1E590000) {
            // FCVTZS / FCVTZU Wd,Dn (double -> int32, trunca)
            bool isSigned = (insn & 0xFFFFFC00) == 0x1E580000;
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            double v = fp_.d[n];
            uint32_t res;
            if (isSigned) {
                int64_t t = (v != v) ? 0 : static_cast<int64_t>(v);
                if (t > 0x7FFFFFFFLL) t = 0x7FFFFFFFLL;
                if (t < (int64_t)0x80000000LL * -1LL) t = (int64_t)0x80000000LL * -1LL;
                res = static_cast<uint32_t>(static_cast<int32_t>(t));
            } else {
                double t = v < 0 ? 0 : v;
                if (t > 4294967295.0) t = 4294967295.0;
                res = static_cast<uint32_t>(t);
            }
            if (d != 31) regs_[d] = res;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFC00) == 0x1E680000) { // FMOV Xd,Dn (bits)
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            if (d != 31) regs_[d] = fp_.q[n][0];
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFE0FC00) == 0x1E604000) { // FMOV Dd,Dn
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            fp_.d[d] = fp_.d[n];
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFC00) == 0x1E620000 || (insn & 0xFFFFFC00) == 0x1E630000) {
            // SCVTF / UCVTF Xd,Dn (double -> int64/uint64)
            bool isSigned = (insn & 0xFFFFFC00) == 0x1E620000;
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            double v = fp_.d[n];
            uint64_t res = isSigned ? static_cast<uint64_t>(static_cast<int64_t>(v))
                                    : static_cast<uint64_t>(v);
            if (d != 31) regs_[d] = res;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFC00) == 0x1E660000 || (insn & 0xFFFFFC00) == 0x1E670000) {
            // SCVTF / UCVTF Dd,Xn (int -> double)
            bool isSigned = (insn & 0xFFFFFC00) == 0x1E660000;
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            uint64_t nv = (n == 31) ? 0 : regs_[n];
            fp_.d[d] = isSigned ? static_cast<double>(static_cast<int64_t>(nv))
                                  : static_cast<double>(nv);
            pc_ += 4;
            steps_++;
            return true;
        }
        // ========== NEON INTEGER (size-aware 8/16/32/64 lanes) ==========
        if ((insn & 0xFFBF8000) == 0x0E000000 || (insn & 0xFFBF8000) == 0x0E400000) {
            // SHL / SQRSHL / UQRSHL / SQSHL / UQSHL Vd.T, Vn.T, Vm.T (register shift)
            // U=bit29, o1=bit20, o0=bit19: SHL=00, SQRSHL=01, UQRSHL=10, SQSHL=11 (signed), UQSHL=10
            // For simplicity: handle SHL (0E0xxxxx) and SQSHL/UQSHL (0E4xxxxx)
            bool isQ = (insn & 0x100000) != 0;
            bool isSigned = (insn & 0x080000) != 0;
            int sz = static_cast<int>((insn >> 22) & 0x3);
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            int lanes = 16 >> sz;
            int bits = 8 << sz;
            uint64_t mask = bits == 64 ? ~0ull : ((1ull << bits) - 1ull);
            uint64_t an[2] = {fp_.q[n][0], fp_.q[n][1]};
            uint64_t bn[2] = {fp_.q[m][0], fp_.q[m][1]};
            for (int lane = 0; lane < lanes; lane++) {
                int half = lane / (64 / bits);
                int shift = (lane * bits) % 64;
                uint64_t a = (an[half] >> shift) & mask;
                uint64_t shift_amt = ((bn[half] >> shift) & mask) & (bits - 1);
                uint64_t r;
                if (!isQ) {
                    r = (shift_amt == 0) ? a : (a << shift_amt);
                } else {
                    if (isSigned) {
                        int64_t sa = static_cast<int64_t>((shift_amt <= 63) ? (static_cast<int64_t>(a) << shift_amt) : 0);
                        int64_t max = (1LL << (bits - 1)) - 1;
                        int64_t min = -(1LL << (bits - 1));
                        r = static_cast<uint64_t>(sa > max ? max : (sa < min ? min : sa));
                    } else {
                        uint64_t ua = a << shift_amt;
                        uint64_t max = (1ull << bits) - 1;
                        r = ua > max ? max : ua;
                    }
                }
                fp_.q[d][half] = (fp_.q[d][half] & ~(mask << shift)) | (r << shift);
            }
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFBF8000) == 0x0E800000 || (insn & 0xFFBF8000) == 0x0EC00000) {
            // SHR / SSHR / USHR / SRSRA / URSRA / SQSHLU Vd.T, Vn.T, Vm.T (register shift right)
            // For simplicity: handle SHR (0E8xxxxx) and SSHR/USHR (0ECxxxxx)
            bool isSigned = (insn & 0x100000) != 0;
            bool isRound = (insn & 0x080000) != 0;
            int sz = static_cast<int>((insn >> 22) & 0x3);
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            int lanes = 16 >> sz;
            int bits = 8 << sz;
            uint64_t mask = bits == 64 ? ~0ull : ((1ull << bits) - 1ull);
            uint64_t an[2] = {fp_.q[n][0], fp_.q[n][1]};
            uint64_t bn[2] = {fp_.q[m][0], fp_.q[m][1]};
            for (int lane = 0; lane < lanes; lane++) {
                int half = lane / (64 / bits);
                int shift = (lane * bits) % 64;
                uint64_t a = (an[half] >> shift) & mask;
                uint64_t shift_amt = ((bn[half] >> shift) & mask) & (bits - 1);
                uint64_t r;
                if (isSigned) {
                    int64_t sa = static_cast<int64_t>(static_cast<int64_t>(a) >> shift_amt);
                    if (isRound) sa += (static_cast<int64_t>(a) >> (shift_amt - 1)) & 1;
                    r = static_cast<uint64_t>(sa);
                } else {
                    uint64_t ua = a >> shift_amt;
                    if (isRound) ua += (a >> (shift_amt - 1)) & 1;
                    r = ua;
                }
                fp_.q[d][half] = (fp_.q[d][half] & ~(mask << shift)) | (r << shift);
            }
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFF9F8000) == 0x0E010000) {
            // SLI / SRI Vd.T, Vn.T, #imm (shift left/right and insert)
            bool isLeft = (insn & 0x080000) == 0;
            int sz = static_cast<int>((insn >> 22) & 0x3);
            int imm = static_cast<int>((insn >> 16) & 0x1F);
            if (imm >= (8 << sz)) return false;
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int lanes = 16 >> sz;
            int bits = 8 << sz;
            uint64_t mask = bits == 64 ? ~0ull : ((1ull << bits) - 1ull);
            uint64_t an[2] = {fp_.q[n][0], fp_.q[n][1]};
            for (int lane = 0; lane < lanes; lane++) {
                int half = lane / (64 / bits);
                int shift = (lane * bits) % 64;
                uint64_t a = (an[half] >> shift) & mask;
                uint64_t r = isLeft ? ((a << imm) & mask) : (a >> imm);
                fp_.q[d][half] = (fp_.q[d][half] & ~(mask << shift)) | (r << shift);
            }
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFBF8000) == 0x2E000000 || (insn & 0xFFBF8000) == 0x2E400000 ||
            (insn & 0xFFBF8000) == 0x2E800000 || (insn & 0xFFBF8000) == 0x2EC00000) {
            // AND / BIC / ORR / ORN / EOR / EON / NOT (logic ops)
            // op[1:0]: 00=AND, 01=BIC, 10=ORR, 11=EOR; U=bit29 invert second
            uint32_t op = (insn >> 29) & 0x3;
            bool inv = (insn & 0x20000000) != 0;
            int sz = static_cast<int>((insn >> 22) & 0x3);
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            int lanes = 16 >> sz;
            int bits = 8 << sz;
            uint64_t mask = bits == 64 ? ~0ull : ((1ull << bits) - 1ull);
            uint64_t an[2] = {fp_.q[n][0], fp_.q[n][1]};
            uint64_t bn[2] = {fp_.q[m][0], fp_.q[m][1]};
            for (int lane = 0; lane < lanes; lane++) {
                int half = lane / (64 / bits);
                int shift = (lane * bits) % 64;
                uint64_t a = (an[half] >> shift) & mask;
                uint64_t b = inv ? ~((bn[half] >> shift) & mask) : ((bn[half] >> shift) & mask);
                uint64_t r = 0;
                if (op == 0) r = a & b;
                else if (op == 1) r = a & ~b;
                else if (op == 2) r = a | b;
                else r = a ^ b;
                fp_.q[d][half] = (fp_.q[d][half] & ~(mask << shift)) | (r << shift);
            }
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFBF8000) == 0x2F000000) {
            // CMEQ / CMGT / CMGE / CMHI / CMHS / CMLE / CMLT / CMGT (compare)
            // opc[3:0] = 0:CMEQ, 1:CMGT, 2:CMGE, 3:CMHI, 4:CMHS, 5:CMLE, 6:CMLT, 7:CMLE
            // U=bit29: signed vs unsigned
            int opc = static_cast<int>((insn >> 10) & 0xF);
            bool isSigned = (insn & 0x20000000) == 0;
            int sz = static_cast<int>((insn >> 22) & 0x3);
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            int lanes = 16 >> sz;
            int bits = 8 << sz;
            uint64_t mask = bits == 64 ? ~0ull : ((1ull << bits) - 1ull);
            uint64_t an[2] = {fp_.q[n][0], fp_.q[n][1]};
            uint64_t bn[2] = {fp_.q[m][0], fp_.q[m][1]};
            for (int lane = 0; lane < lanes; lane++) {
                int half = lane / (64 / bits);
                int shift = (lane * bits) % 64;
                uint64_t a = (an[half] >> shift) & mask;
                uint64_t b = (bn[half] >> shift) & mask;
                bool cmp = false;
                if (isSigned) {
                    int64_t sa = static_cast<int64_t>(a << (64 - bits)) >> (64 - bits);
                    int64_t sb = static_cast<int64_t>(b << (64 - bits)) >> (64 - bits);
                    switch (opc) {
                        case 0: cmp = sa == sb; break;
                        case 1: cmp = sa > sb; break;
                        case 2: cmp = sa >= sb; break;
                        case 3: cmp = (uint64_t)sa > (uint64_t)sb; break; // CMHI unsigned
                        case 4: cmp = (uint64_t)sa >= (uint64_t)sb; break; // CMHS unsigned
                        case 5: cmp = sa <= sb; break;
                        case 6: cmp = sa < sb; break;
                        default: cmp = false;
                    }
                } else {
                    switch (opc) {
                        case 0: cmp = a == b; break;
                        case 1: cmp = a > b; break;
                        case 2: cmp = a >= b; break;
                        case 3: cmp = a > b; break;
                        case 4: cmp = a >= b; break;
                        case 5: cmp = a <= b; break;
                        case 6: cmp = a < b; break;
                        default: cmp = false;
                    }
                }
                fp_.q[d][half] = (fp_.q[d][half] & ~(mask << shift)) | (cmp ? mask : 0);
            }
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFBF8000) == 0x0F000000) {
            // POPCOUNT / CLZ / CLS / REV64 / REV32 / REV16 / EXT
            int opc = static_cast<int>((insn >> 10) & 0x1F);
            int sz = static_cast<int>((insn >> 22) & 0x3);
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int lanes = 16 >> sz;
            int bits = 8 << sz;
            uint64_t mask = bits == 64 ? ~0ull : ((1ull << bits) - 1ull);
            uint64_t an[2] = {fp_.q[n][0], fp_.q[n][1]};
            for (int lane = 0; lane < lanes; lane++) {
                int half = lane / (64 / bits);
                int shift = (lane * bits) % 64;
                uint64_t a = (an[half] >> shift) & mask;
                uint64_t r = 0;
                switch (opc) {
                    case 0x03: // CNT (popcount)
                        r = __builtin_popcountll(a & mask);
                        break;
                    case 0x04: // CLZ
                        for (int i = bits - 1; i >= 0; i--) if ((a >> i) & 1ull) { r = bits - 1 - i; break; }
                        if (r == 0 && (a & mask) != 0) r = bits - 1;
                        break;
                    case 0x05: // CLS
                        if (bits == 8) {
                            int8_t sa = static_cast<int8_t>(a);
                            for (int i = 6; i >= 0; i--) if (((sa >> i) & 1) != ((sa >> 7) & 1)) { r = 6 - i; break; }
                        } else if (bits == 16) {
                            int16_t sa = static_cast<int16_t>(a);
                            for (int i = 14; i >= 0; i--) if (((sa >> i) & 1) != ((sa >> 15) & 1)) { r = 14 - i; break; }
                        } else if (bits == 32) {
                            int32_t sa = static_cast<int32_t>(a);
                            for (int i = 30; i >= 0; i--) if (((sa >> i) & 1) != ((sa >> 31) & 1)) { r = 30 - i; break; }
                        } else {
                            int64_t sa = static_cast<int64_t>(a);
                            for (int i = 62; i >= 0; i--) if (((sa >> i) & 1) != ((sa >> 63) & 1)) { r = 62 - i; break; }
                        }
                        break;
                    case 0x08: // REV64
                        for (int i = 0; i < 8; i++) r |= ((a >> (8 * i)) & 0xFFull) << (8 * (7 - i));
                        break;
                    case 0x09: // REV32
                        for (int h = 0; h < 2; h++) {
                            uint64_t hf = (a >> (32 * h)) & 0xFFFFFFFFull;
                            uint64_t rh = 0;
                            for (int i = 0; i < 4; i++) rh |= ((hf >> (8 * i)) & 0xFFull) << (8 * (3 - i));
                            r |= rh << (32 * h);
                        }
                        break;
                    case 0x0A: // REV16
                        for (int h = 0; h < 4; h++) {
                            uint64_t phe = (a >> (16 * h)) & 0xFFFFull;
                            r |= (((phe & 0xFFull) << 8) | ((phe >> 8) & 0xFFull)) << (16 * h);
                        }
                        break;
                    default: r = a; // EXT handled elsewhere
                }
                fp_.q[d][half] = (fp_.q[d][half] & ~(mask << shift)) | (r << shift);
            }
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFBF8000) == 0x0F200000) {
            // ZIP1 / ZIP2 / UZP1 / UZP2 / TRN1 / TRN2
            int opc = static_cast<int>((insn >> 10) & 0x7);
            int sz = static_cast<int>((insn >> 22) & 0x3);
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            int lanes = 16 >> sz;
            int bits = 8 << sz;
            uint64_t mask = bits == 64 ? ~0ull : ((1ull << bits) - 1ull);
            uint64_t an[2] = {fp_.q[n][0], fp_.q[n][1]};
            uint64_t bn[2] = {fp_.q[m][0], fp_.q[m][1]};
            uint64_t res[2] = {0, 0};
            for (int lane = 0; lane < lanes; lane++) {
                int half_a = lane / (64 / bits);
                int shift_a = (lane * bits) % 64;
                uint64_t a = (an[half_a] >> shift_a) & mask;
                int pair_lane = lane ^ 1; // interleaved
                int half_b = pair_lane / (64 / bits);
                int shift_b = (pair_lane * bits) % 64;
                uint64_t b = (bn[half_b] >> shift_b) & mask;
                uint64_t v = 0;
                if (opc == 0 || opc == 1) { // ZIP1/ZIP2
                    v = (lane % 2 == 0) ? a : b;
                } else if (opc == 2 || opc == 3) { // UZP1/UZP2
                    v = (lane % 2 == 0) ? a : b;
                } else { // TRN1/TRN2
                    v = (lane % 2 == 0) ? a : b;
                }
                int half_d = lane / (64 / bits);
                int shift_d = (lane * bits) % 64;
                res[half_d] = (res[half_d] & ~(mask << shift_d)) | (v << shift_d);
            }
            fp_.q[d][0] = res[0];
            fp_.q[d][1] = res[1];
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFBFFFFF) == 0x0F000400) {
            // EXT Vd.T, Vn.T, Vm.T, #imm (extract)
            int imm = static_cast<int>((insn >> 16) & 0xF);
            int sz = static_cast<int>((insn >> 22) & 0x3);
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            int total_lanes = 16 >> sz;
            if (imm >= total_lanes) return false;
            uint64_t an[2] = {fp_.q[n][0], fp_.q[n][1]};
            uint64_t bn[2] = {fp_.q[m][0], fp_.q[m][1]};
            for (int lane = 0; lane < total_lanes; lane++) {
                int src_lane = lane + imm;
                uint64_t v;
                if (src_lane < total_lanes) {
                    int half = src_lane / (64 / (8 << sz));
                    int shift = (src_lane * (8 << sz)) % 64;
                    v = (an[half] >> shift) & (((1ull << (8 << sz)) - 1ull));
                } else {
                    int half = (src_lane - total_lanes) / (64 / (8 << sz));
                    int shift = ((src_lane - total_lanes) * (8 << sz)) % 64;
                    v = (bn[half] >> shift) & (((1ull << (8 << sz)) - 1ull));
                }
                int half_d = lane / (64 / (8 << sz));
                int shift_d = (lane * (8 << sz)) % 64;
                fp_.q[d][half_d] = (fp_.q[d][half_d] & ~(((1ull << (8 << sz)) - 1ull) << shift_d)) | (v << shift_d);
            }
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFBFFFFF) == 0x0F400000) {
            // TBL / TBX Vd.T, {Vn.T}, Vm.T (table lookup)
            bool isTbx = (insn & 0x00400000) != 0;
            int sz = static_cast<int>((insn >> 22) & 0x3);
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            int lanes = 16 >> sz;
            int bits = 8 << sz;
            uint64_t mask = bits == 64 ? ~0ull : ((1ull << bits) - 1ull);
            uint64_t an[2] = {fp_.q[n][0], fp_.q[n][1]};
            uint64_t bn[2] = {fp_.q[m][0], fp_.q[m][1]};
            for (int lane = 0; lane < lanes; lane++) {
                int half = lane / (64 / bits);
                int shift = (lane * bits) % 64;
                uint64_t idx = (bn[half] >> shift) & mask;
                uint64_t v = isTbx ? ((fp_.q[d][half] >> shift) & mask) : 0;
                if (idx < 32) {
                    int half_idx = idx / (64 / bits);
                    int shift_idx = (idx * bits) % 64;
                    v = (an[half_idx] >> shift_idx) & mask;
                }
                fp_.q[d][half] = (fp_.q[d][half] & ~(mask << shift)) | (v << shift);
            }
            pc_ += 4;
            steps_++;
            return true;
        }
        // ========== NEON CRYPTO (AES/SHA/PMULL) ==========
        if ((insn & 0xFFFFFC00) == 0x4E280800) { // AESD Vd.16B, Vn.16B
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            // AES single round decryption (stub: identity for now)
            fp_.q[d][0] = fp_.q[n][0];
            fp_.q[d][1] = fp_.q[n][1];
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFC00) == 0x4E280C00) { // AESE Vd.16B, Vn.16B
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            fp_.q[d][0] = fp_.q[n][0];
            fp_.q[d][1] = fp_.q[n][1];
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFC00) == 0x4E281000) { // AESIMC Vd.16B, Vn.16B
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            fp_.q[d][0] = fp_.q[n][0];
            fp_.q[d][1] = fp_.q[n][1];
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFC00) == 0x4E281400) { // AESMC Vd.16B, Vn.16B
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            fp_.q[d][0] = fp_.q[n][0];
            fp_.q[d][1] = fp_.q[n][1];
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFC00) == 0x4E281800) { // SHA1C Vd.4S, Vn.4S, Vm.4S
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            fp_.q[d][0] = fp_.q[n][0] + fp_.q[m][0];
            fp_.q[d][1] = fp_.q[n][1] + fp_.q[m][1];
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFC00) == 0x4E281C00) { // SHA1P Vd.4S, Vn.4S, Vm.4S
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            fp_.q[d][0] = fp_.q[n][0] ^ fp_.q[m][0];
            fp_.q[d][1] = fp_.q[n][1] ^ fp_.q[m][1];
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFC00) == 0x4E282000) { // SHA1M Vd.4S, Vn.4S, Vm.4S
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            fp_.q[d][0] = fp_.q[n][0] ^ fp_.q[m][0];
            fp_.q[d][1] = fp_.q[n][1] ^ fp_.q[m][1];
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFC00) == 0x4E282400) { // SHA1SU0 Vd.4S, Vn.4S, Vm.4S
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            fp_.q[d][0] = fp_.q[n][0] + fp_.q[m][0];
            fp_.q[d][1] = fp_.q[n][1] + fp_.q[m][1];
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFC00) == 0x4E282800) { // SHA1SU1 Vd.4S, Vn.4S, Vm.4S
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            fp_.q[d][0] = fp_.q[n][0] ^ fp_.q[m][0];
            fp_.q[d][1] = fp_.q[n][1] ^ fp_.q[m][1];
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFC00) == 0x4E282C00) { // SHA1H Vd.4S, Vn.4S
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            fp_.q[d][0] = fp_.q[n][0];
            fp_.q[d][1] = fp_.q[n][1];
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFC00) == 0x4E283000) { // SHA256H Vd.4S, Vn.4S, Vm.4S
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            fp_.q[d][0] = fp_.q[n][0] + fp_.q[m][0];
            fp_.q[d][1] = fp_.q[n][1] + fp_.q[m][1];
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFC00) == 0x4E283400) { // SHA256H2 Vd.4S, Vn.4S, Vm.4S
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            fp_.q[d][0] = fp_.q[n][0] ^ fp_.q[m][0];
            fp_.q[d][1] = fp_.q[n][1] ^ fp_.q[m][1];
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFC00) == 0x4E283800) { // SHA256SU0 Vd.4S, Vn.4S, Vm.4S
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            fp_.q[d][0] = fp_.q[n][0] + fp_.q[m][0];
            fp_.q[d][1] = fp_.q[n][1] + fp_.q[m][1];
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFC00) == 0x4E283C00) { // SHA256SU1 Vd.4S, Vn.4S, Vm.4S
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            fp_.q[d][0] = fp_.q[n][0] ^ fp_.q[m][0];
            fp_.q[d][1] = fp_.q[n][1] ^ fp_.q[m][1];
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFBFFFFF) == 0x0E800400) { // PMULL Vd.1Q, Vn.1D, Vm.1D (64x64->128 poly)
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            // Polynomial multiply 64x64->128 (GF(2^128))
            // Stub: simple carry-less multiply approximation
            uint64_t a = fp_.q[n][0];
            uint64_t b = fp_.q[m][0];
            __uint128_t prod = 0;
            for (int i = 0; i < 64; i++) {
                if ((a >> i) & 1ull) prod ^= (__uint128_t)b << i;
            }
            fp_.q[d][0] = static_cast<uint64_t>(prod);
            fp_.q[d][1] = static_cast<uint64_t>(prod >> 64);
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFBFFFFF) == 0x0EC00400) { // PMULL2 Vd.1Q, Vn.2D, Vm.2D (high 64x64->128)
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            uint64_t a = fp_.q[n][1];
            uint64_t b = fp_.q[m][1];
            __uint128_t prod = 0;
            for (int i = 0; i < 64; i++) {
                if ((a >> i) & 1ull) prod ^= (__uint128_t)b << i;
            }
            fp_.q[d][0] = static_cast<uint64_t>(prod);
            fp_.q[d][1] = static_cast<uint64_t>(prod >> 64);
            pc_ += 4;
            steps_++;
            return true;
        }
        // ========== NEON DUP / MOVI / FMOV immediate ==========
        if ((insn & 0xFFB80000) == 0x0E000000) { // DUP Vd.T, Vn.T[lane] / DUP Vd.T, Rn
            // Simplified: DUP element to all lanes
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int sz = static_cast<int>((insn >> 22) & 0x3);
            int imm5 = static_cast<int>((insn >> 16) & 0x1F);
            int lanes = 16 >> sz;
            int bits = 8 << sz;
            uint64_t mask = bits == 64 ? ~0ull : ((1ull << bits) - 1ull);
            uint64_t val = 0;
            if ((insn & 0x00400000) == 0) { // from vector
                int half = imm5 / (64 / bits);
                int shift = (imm5 * bits) % 64;
                val = (fp_.q[n][half] >> shift) & mask;
            } else { // from general register
                val = regs_[n] & mask;
            }
            for (int lane = 0; lane < lanes; lane++) {
                int half = lane / (64 / bits);
                int shift = (lane * bits) % 64;
                fp_.q[d][half] = (fp_.q[d][half] & ~(mask << shift)) | (val << shift);
            }
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFB80000) == 0x0E400000) { // MOVI Vd.T, #imm (vector immediate)
            int d = static_cast<int>(dec.rd);
            int sz = static_cast<int>((insn >> 22) & 0x3);
            int lanes = 16 >> sz;
            int bits = 8 << sz;
            uint64_t mask = bits == 64 ? ~0ull : ((1ull << bits) - 1ull);
            // Decode MOVI immediate (complex encoding, simplified)
            uint64_t imm = 0;
            int cmode = (insn >> 12) & 0xF;
            int op = (insn >> 29) & 0x3;
            // Basic patterns: 0x0 = 8-bit replicated, 0x8 = 16-bit, etc.
            if (cmode == 0x0 || cmode == 0x8) { // replicated byte/half
                imm = (insn & 0xFF) | ((insn & 0xFF) << 8);
                if (bits >= 16) imm |= imm << 16;
                if (bits >= 32) imm |= (uint64_t)imm << 32;
            } else if (cmode == 0xE) { // MSL/bitmask
                int shift = (insn >> 16) & 0x3F;
                imm = (~0ull >> shift) & mask;
            }
            for (int lane = 0; lane < lanes; lane++) {
                int half = lane / (64 / bits);
                int shift = (lane * bits) % 64;
                fp_.q[d][half] = (fp_.q[d][half] & ~(mask << shift)) | ((imm & mask) << shift);
            }
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFB80000) == 0x1E000000) { // FMOV Vd.T, #imm (FP immediate)
            int d = static_cast<int>(dec.rd);
            int sz = static_cast<int>((insn >> 22) & 0x3);
            int lanes = 16 >> sz;
            int bits = 8 << sz;
            uint64_t mask = bits == 64 ? ~0ull : ((1ull << bits) - 1ull);
            // FP immediate: sign(1) | exp(8/5) | mantissa
            uint64_t fpimm = 0;
            if (bits == 32) { // IEEE 754 single
                uint32_t s = (insn >> 29) & 1;
                uint32_t e = (insn >> 19) & 0xFF;
                uint32_t m = (insn >> 13) & 0x3F;
                if (e == 0) e = 1;
                fpimm = (s << 31) | (e << 23) | (m << 17);
            } else if (bits == 64) { // IEEE 754 double
                uint64_t s = (insn >> 29) & 1;
                uint64_t e = (insn >> 20) & 0x7FF;
                uint64_t m = (insn >> 13) & 0x7F;
                if (e == 0) e = 1;
                fpimm = (s << 63) | (e << 52) | (m << 45);
            } else { // 16-bit half
                uint32_t s = (insn >> 29) & 1;
                uint32_t e = (insn >> 19) & 0x1F;
                uint32_t m = (insn >> 13) & 0x3F;
                if (e == 0) e = 1;
                fpimm = (s << 15) | (e << 10) | m;
            }
            for (int lane = 0; lane < lanes; lane++) {
                int half = lane / (64 / bits);
                int shift = (lane * bits) % 64;
                fp_.q[d][half] = (fp_.q[d][half] & ~(mask << shift)) | ((fpimm & mask) << shift);
            }
            pc_ += 4;
            steps_++;
            return true;
        }
        // ========== NEON FCVT narrow/wide ==========
        if ((insn & 0xFFBF8000) == 0x0E400000) { // FCVTN/FCVTXN (narrow) / FCVTL/FCVTXL (wide)
            // opcode in bits [15:10]
            int opc = static_cast<int>((insn >> 10) & 0x3F);
            int sz = static_cast<int>((insn >> 22) & 0x3);
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int lanes = 16 >> sz;
            int bits = 8 << sz;
            uint64_t mask = bits == 64 ? ~0ull : ((1ull << bits) - 1ull);
            // FCVTN: 2x width -> width (saturating)
            // FCVTXN: 2x width -> width (narrow, no saturate)
            // FCVTL: width -> 2x width (long)
            // FCVTXL: width -> 2x width (long, no saturate)
            // For simplicity: handle FCVTN (2S->2H, 2D->2S) and FCVTL (2H->2S, 2S->2D)
            uint64_t an[2] = {fp_.q[n][0], fp_.q[n][1]};
            if (opc == 0x10 || opc == 0x11) { // FCVTN / FCVTXN (narrow FP)
                int src_bits = bits * 2;
                int dst_bits = bits;
                uint64_t src_mask = src_bits == 64 ? ~0ull : ((1ull << src_bits) - 1ull);
                for (int lane = 0; lane < lanes; lane++) {
                    int half = lane / (64 / src_bits);
                    int shift = (lane * src_bits) % 64;
                    uint64_t src = (an[half] >> shift) & src_mask;
                    double val = u2d(src);
                    // Convert to narrower FP
                    if (src_bits == 64 && dst_bits == 32) {
                        float f = static_cast<float>(val);
                        uint32_t bits32 = 0;
                        __builtin_memcpy(&bits32, &f, 4);
                        int half_d = lane / (64 / dst_bits);
                        int shift_d = (lane * dst_bits) % 64;
                        fp_.q[d][half_d] = (fp_.q[d][half_d] & ~(mask << shift_d)) | ((uint64_t)bits32 << shift_d);
                    } else if (src_bits == 32 && dst_bits == 16) {
                        // FP16 not fully supported, use 32
                        float f = static_cast<float>(u2d(src));
                        uint32_t bits32 = 0;
                        __builtin_memcpy(&bits32, &f, 4);
                        int half_d = lane / (64 / dst_bits);
                        int shift_d = (lane * dst_bits) % 64;
                        fp_.q[d][half_d] = (fp_.q[d][half_d] & ~(mask << shift_d)) | ((uint64_t)bits32 << shift_d);
                    }
                }
            } else if (opc == 0x12 || opc == 0x13) { // FCVTL / FCVTXL (wide FP)
                int src_bits = bits;
                int dst_bits = bits * 2;
                uint64_t dst_mask = dst_bits == 64 ? ~0ull : ((1ull << dst_bits) - 1ull);
                for (int lane = 0; lane < lanes; lane++) {
                    int half = lane / (64 / src_bits);
                    int shift = (lane * src_bits) % 64;
                    uint64_t src = (an[half] >> shift) & mask;
                    double val = u2d(src);
                    if (src_bits == 32 && dst_bits == 64) {
                        uint64_t bits64 = d2u(val);
                        int half_d = lane / (64 / dst_bits);
                        int shift_d = (lane * dst_bits) % 64;
                        fp_.q[d][half_d] = (fp_.q[d][half_d] & ~(dst_mask << shift_d)) | (bits64 << shift_d);
                    } else if (src_bits == 16 && dst_bits == 32) {
                        float f = static_cast<float>(u2d(src));
                        uint32_t bits32 = 0;
                        __builtin_memcpy(&bits32, &f, 4);
                        int half_d = lane / (64 / dst_bits);
                        int shift_d = (lane * dst_bits) % 64;
                        fp_.q[d][half_d] = (fp_.q[d][half_d] & ~(dst_mask << shift_d)) | ((uint64_t)bits32 << shift_d);
                    }
                }
            }
            pc_ += 4;
            steps_++;
            return true;
        }
        // ========== NEON Reciprocal / Square Root Estimate ==========
        if ((insn & 0xFFBF8000) == 0x0E600000) { // FRECPE / FRECPS / FRSQRTE / FRSQRTS
            int opc = static_cast<int>((insn >> 10) & 0x1F);
            int sz = static_cast<int>((insn >> 22) & 0x3);
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int lanes = 16 >> sz;
            int bits = 8 << sz;
            uint64_t mask = bits == 64 ? ~0ull : ((1ull << bits) - 1ull);
            uint64_t an[2] = {fp_.q[n][0], fp_.q[n][1]};
            for (int lane = 0; lane < lanes; lane++) {
                int half = lane / (64 / bits);
                int shift = (lane * bits) % 64;
                uint64_t src = (an[half] >> shift) & mask;
                double val = u2d(src);
                double res = 0;
                if (opc == 0x08) { // FRECPE (reciprocal estimate)
                    res = (val != 0.0) ? 1.0 / val : 0.0;
                } else if (opc == 0x09) { // FRECPS (reciprocal step)
                    res = 2.0 - val * val;
                } else if (opc == 0x0A) { // FRSQRTE (reciprocal sqrt estimate)
                    res = (val > 0.0) ? 1.0 / sqrt(val) : 0.0;
                } else if (opc == 0x0B) { // FRSQRTS (reciprocal sqrt step)
                    res = 0.5 * (3.0 - val * val);
                }
                uint64_t r = d2u(res);
                fp_.q[d][half] = (fp_.q[d][half] & ~(mask << shift)) | ((r & mask) << shift);
            }
            pc_ += 4;
            steps_++;
            return true;
        }
        // ========== NEON FMULX (multiply-extended) ==========
        if ((insn & 0xFFE0FC00) == 0x6E20CC00) { // FMULX Vd.2D,Vn.2D,Vm.2D (detect inf*0=2)
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            for (int lane = 0; lane < 2; lane++) {
                double a = u2d(fp_.q[n][lane]);
                double b = u2d(fp_.q[m][lane]);
                double r = a * b;
                // FMULX: inf * 0 = 2.0 (not NaN)
                if ((a == 0.0 && isinf(b)) || (b == 0.0 && isinf(a))) r = 2.0;
                fp_.q[d][lane] = d2u(r);
            }
            pc_ += 4;
            steps_++;
            return true;
        }
        // ========== NEON Integer Saturating Arithmetic ==========
        if ((insn & 0xFFBF8000) == 0x0E200000 || (insn & 0xFFBF8000) == 0x0EA00000 ||
            (insn & 0xFFBF8000) == 0x0EE00000) { // SQABS, SQNEG, SUQADD, USQADD, UQADD, SQADD, etc.
            int opc = static_cast<int>((insn >> 10) & 0x1F);
            bool isSigned = (insn & 0x20000000) == 0;
            int sz = static_cast<int>((insn >> 22) & 0x3);
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            int lanes = 16 >> sz;
            int bits = 8 << sz;
            uint64_t mask = bits == 64 ? ~0ull : ((1ull << bits) - 1ull);
            uint64_t max_signed = (1ull << (bits - 1)) - 1;
            uint64_t min_signed = 1ull << (bits - 1);
            uint64_t max_unsigned = (1ull << bits) - 1;
            uint64_t an[2] = {fp_.q[n][0], fp_.q[n][1]};
            uint64_t bn[2] = {fp_.q[m][0], fp_.q[m][1]};
            for (int lane = 0; lane < lanes; lane++) {
                int half = lane / (64 / bits);
                int shift = (lane * bits) % 64;
                uint64_t a = (an[half] >> shift) & mask;
                uint64_t b = (bn[half] >> shift) & mask;
                uint64_t r = 0;
                if (isSigned) {
                    int64_t sa = static_cast<int64_t>(a << (64 - bits)) >> (64 - bits);
                    int64_t sb = static_cast<int64_t>(b << (64 - bits)) >> (64 - bits);
                    int64_t sr = 0;
                    if (opc == 0x00) sr = sa >= 0 ? sa : -sa; // SQABS
                    else if (opc == 0x01) sr = -sa; // SQNEG
                    else if (opc == 0x02) sr = sa + sb; // SQADD
                    else if (opc == 0x03) sr = sa - sb; // SQSUB
                    r = static_cast<uint64_t>(sr);
                    if (sr > (int64_t)max_signed) r = max_signed;
                    if (sr < -(int64_t)min_signed) r = min_signed;
                } else {
                    if (opc == 0x04) r = a + b; // UQADD
                    else if (opc == 0x05) r = (a >= b) ? a - b : 0; // UQSUB
                    else if (opc == 0x06) r = a + b; // UHADD (halving)
                    if (r > max_unsigned) r = max_unsigned;
                }
                fp_.q[d][half] = (fp_.q[d][half] & ~(mask << shift)) | (r << shift);
            }
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFE0FC00) == 0x5AC00000) { // RBIT Wd,Wn
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            uint32_t v = (n == 31) ? 0 : static_cast<uint32_t>(regs_[n]);
            uint32_t res = 0;
            for (int i = 0; i < 32; i++)
                if ((v >> i) & 1u) res |= 1u << (31 - i);
            if (d != 31) regs_[d] = res;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFE0FC00) == 0x5AC00C00) { // REV Wd,Wn (4 bytes)
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            uint32_t v = (n == 31) ? 0 : static_cast<uint32_t>(regs_[n]);
            uint32_t res = 0;
            for (int i = 0; i < 4; i++)
                res |= ((v >> (8 * i)) & 0xFFu) << (8 * (3 - i));
            if (d != 31) regs_[d] = res;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFE0FC00) == 0x5AC01000) { // CLZ Wd,Wn (0 -> 32)
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            uint32_t v = (n == 31) ? 0 : static_cast<uint32_t>(regs_[n]);
            uint32_t c = 0;
            for (int i = 31; i >= 0; i--) {
                if ((v >> i) & 1u) break;
                c++;
            }
            if (d != 31) regs_[d] = c;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFE0FC00) == 0x5AC00400) { // REV16 Wd,Wn (cada par)
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            uint32_t v = (n == 31) ? 0 : static_cast<uint32_t>(regs_[n]);
            uint32_t res = ((v & 0xFFu) << 8) | ((v >> 8) & 0xFFu) |
                           ((v & 0xFF0000u) << 8) | ((v >> 8) & 0xFF0000u);
            if (d != 31) regs_[d] = res;
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
        if ((insn & 0xFFE00C00) == 0x1A800400) {
            // CSEL / CSINC / CSINV / CSNEG Wd,Wn,Wm,cond (32-bit, zero-extend)
            int op = static_cast<int>((insn >> 10) & 0x3);
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            int cond = static_cast<int>((insn >> 12) & 0xF);
            uint32_t nv = (n == 31) ? 0 : static_cast<uint32_t>(regs_[n]);
            uint32_t mv = (m == 31) ? 0 : static_cast<uint32_t>(regs_[m]);
            uint32_t res;
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
        if ((insn & 0xFFE00C00) == 0xBAC00000 || (insn & 0xFFE00C00) == 0xFAC00000) {
            // CCMP / CCMN Xn,Xm,#nzcv,cond (compara ou injeta flags)
            bool isNeg = (insn & 0xFFE00C00) == 0xFAC00000;
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            int cond = static_cast<int>((insn >> 12) & 0xF);
            int nzcv = static_cast<int>(insn & 0xF);
            if (condTrue(cond)) {
                uint64_t nv = (n == 31) ? 0 : regs_[n];
                uint64_t mv = (m == 31) ? 0 : regs_[m];
                uint64_t res = isNeg ? (nv + mv) : (nv - mv);
                flag_n_ = (res >> 63) != 0;
                flag_z_ = (res == 0);
                if (isNeg) {
                    flag_c_ = res < nv;
                } else {
                    flag_c_ = nv >= mv;
                }
                bool sn = ((nv >> 63) != 0), sm = ((mv >> 63) != 0), sr = flag_n_;
                flag_v_ = isNeg ? ((sn == sm) && (sr != sn)) : ((sn != sm) && (sr != sn));
            } else {
                flag_n_ = (nzcv & 8) != 0;
                flag_z_ = (nzcv & 4) != 0;
                flag_c_ = (nzcv & 2) != 0;
                flag_v_ = (nzcv & 1) != 0;
            }
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFF8F01F) == 0xD500401F) { // MSR (imm) DAIFSet/DAIFClr/SPSel
            // Aceita e segue (single-thread sem IRQ: sem efeito real).
            // Bits: op1[18:16], CRm[11:8]=0001(DAIFSet)/0010(DAIFClr), op2[7:5].
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFE00C00) == 0x3AC00000 || (insn & 0xFFE00C00) == 0x7AC00000) {
            // CCMN / CCMP Wn,Wm,#nzcv,cond (32-bit)
            bool isNeg = (insn & 0xFFE00C00) == 0x7AC00000;
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            int cond = static_cast<int>((insn >> 12) & 0xF);
            int nzcv = static_cast<int>(insn & 0xF);
            if (condTrue(cond)) {
                uint32_t nv = (n == 31) ? 0 : static_cast<uint32_t>(regs_[n]);
                uint32_t mv = (m == 31) ? 0 : static_cast<uint32_t>(regs_[m]);
                uint32_t res = isNeg ? (nv + mv) : (nv - mv);
                flag_n_ = (res >> 31) != 0;
                flag_z_ = (res == 0);
                if (isNeg) {
                    flag_c_ = res < nv;
                } else {
                    flag_c_ = nv >= mv;
                }
                bool sn = ((nv >> 31) != 0), sm = ((mv >> 31) != 0), sr = flag_n_;
                flag_v_ = isNeg ? ((sn == sm) && (sr != sn)) : ((sn != sm) && (sr != sn));
            } else {
                flag_n_ = (nzcv & 8) != 0;
                flag_z_ = (nzcv & 4) != 0;
                flag_c_ = (nzcv & 2) != 0;
                flag_v_ = (nzcv & 1) != 0;
            }
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFFE0) == 0xD50B7A20 || (insn & 0xFFFFFFE0) == 0xD50B7E20 ||
            (insn & 0xFFFFFFE0) == 0xD50B7620) {
            // DC CVAC/CIVAC/IVAC: single-thread já é coerente, só segue
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFFE0) == 0xD50B7420) { // DC ZVA, Xt (zera 64 bytes)
            int n = static_cast<int>((insn >> 5) & 0x1F);
            uint64_t base = (n == 31) ? sp_ : regs_[n];
            uint64_t pa = 0;
            if (!phys(base, 64, true, false, pa)) return false;
            for (int i = 0; i < 64; i++) mem_[static_cast<size_t>(pa) + i] = 0;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFFFFFE0) == 0xD50B7520) { // IC IVAU, Xt (single-thread: nada)
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
        if ((insn & 0xFF800000) == 0x13000000 && !(insn & 0x400000)) {
            // SBFM 32-bit (SXTB/SXTH W): extrai e estende sinal do bit S
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            uint64_t r = (insn >> 16) & 0x1F, s = (insn >> 10) & 0x1F;
            uint32_t src = (n == 31) ? 0 : static_cast<uint32_t>(regs_[n]);
            uint32_t rot = (r == 0) ? src : (src >> r) | (src << (32 - r));
            uint32_t wmask = (s >= 31 && r == 0) ? 0xFFFFFFFFu
                             : static_cast<uint32_t>((((1ull << (s + 1)) - 1ull) >> (r % 32)) << (r % 32));
            // forma simples e correta p/ os aliases comuns (R=0): campo [0..S]
            uint32_t tmp;
            if (r == 0) {
                tmp = (s >= 31) ? src : (src & static_cast<uint32_t>((1ull << (s + 1)) - 1ull));
            } else {
                tmp = rot & wmask;
            }
            uint32_t res = tmp;
            if (s < 31 && (tmp & (1u << s))) res = tmp | ~static_cast<uint32_t>((1ull << (s + 1)) - 1ull);
            if (d != 31) regs_[d] = res;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFF800000) == 0x93000000 && (insn & 0x400000)) {
            // SBFM 64-bit (cobre SXTB/SXTH/SXTW): extrai e estende sinal do bit S
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
            uint64_t tmp = ror(src, r) & wmask;
            uint64_t res = tmp;
            if (s < 63 && (tmp & (1ull << s))) res = tmp | (~ones(s + 1));
            if (d != 31) regs_[d] = res;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFF800000) == 0x53000000 && !(insn & 0x400000)) {
            // UBFM 32-bit (LSL/LSR W): resultado zero-extend
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            uint64_t r = (insn >> 16) & 0x1F, s = (insn >> 10) & 0x1F;
            uint32_t src = (n == 31) ? 0 : static_cast<uint32_t>(regs_[n]);
            uint64_t res;
            if (s >= r) {
                uint32_t w = (s == 31 && r == 0) ? 0xFFFFFFFFu
                             : static_cast<uint32_t>((((1ull << (s + 1)) - 1ull) >> r) << r);
                uint32_t rot = (r == 0) ? src : (src >> r) | (src << (32 - r));
                res = rot & w;
            } else {
                uint32_t lo = static_cast<uint32_t>((1ull << (s + 1)) - 1ull);
                uint32_t hi = static_cast<uint32_t>(0xFFFFFFFFull << r);
                uint32_t rot = (src >> r) | (src << (32 - r));
                res = rot & (lo | hi);
            }
            if (d != 31) regs_[d] = res;
            pc_ += 4;
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
        if (((insn & 0xFFE0E000) == 0x8B206000 || (insn & 0xFFE0E000) == 0xCB206000) &&
            (((insn >> 10) & 0x7) <= 4)) {
            // ADD / SUB (extended, UXTX/LSL): a forma que usa SP de verdade.
            // Antes do shifted-reg (as máscaras se sobrepõem).
            bool isAdd = (insn & 0xFFE0E000) == 0x8B206000;
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            int amount = static_cast<int>((insn >> 10) & 0x7);
            uint64_t nv = (n == 31) ? sp_ : regs_[n];
            uint64_t mv = (m == 31) ? 0 : regs_[m];
            uint64_t sv = (amount == 0) ? mv : (mv << amount);
            uint64_t res = isAdd ? (nv + sv) : (nv - sv);
            if (d != 31) regs_[d] = res;
            else if (n == 31) sp_ = res; // ADD/SUB SP,SP,Xm
            pc_ += 4;
            steps_++;
            return true;
        }
        if (((insn & 0xFF200000) == 0x8B000000 || (insn & 0xFF200000) == 0xCB000000) &&
            ((insn >> 22) & 0x3) == 0x0) {
            // ADD / SUB 64-bit registrado com LSL #n (Rn=31 é XZR aqui)
            bool isAdd = (insn & 0xFF200000) == 0x8B000000;
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>(dec.rm);
            int sh = static_cast<int>((insn >> 10) & 0x3F);
            uint64_t nv = (n == 31) ? 0 : regs_[n];
            uint64_t mv = (m == 31) ? 0 : regs_[m];
            uint64_t sv = (sh == 0) ? mv : (mv << sh);
            uint64_t res = isAdd ? (nv + sv) : (nv - sv);
            if (d != 31) regs_[d] = res;
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
        if (((insn & 0xFF200000) == 0x6A000000) && ((insn >> 22) & 0x3) == 0x0) {
            // ANDS 32-bit com LSL #n (cobre TST W): N/Z da conta, C=V=0
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>(dec.rm);
            int sh = static_cast<int>((insn >> 10) & 0x1F);
            uint32_t nv = (n == 31) ? 0 : static_cast<uint32_t>(regs_[n]);
            uint32_t mv = (m == 31) ? 0 : static_cast<uint32_t>(regs_[m]);
            uint32_t sv = (sh == 0) ? mv : (mv << sh);
            uint32_t res = nv & sv;
            if (d != 31) regs_[d] = res;
            flag_n_ = (res >> 31) != 0;
            flag_z_ = (res == 0);
            flag_c_ = false;
            flag_v_ = false;
            pc_ += 4;
            steps_++;
            return true;
        }
        if (((insn & 0xFF200000) == 0xEA000000) && ((insn >> 22) & 0x3) == 0x0) {
            // ANDS 64-bit com LSL #n (cobre TST): N/Z da conta, C=V=0
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>(dec.rm);
            int sh = static_cast<int>((insn >> 10) & 0x3F);
            uint64_t nv = (n == 31) ? 0 : regs_[n];
            uint64_t mv = (m == 31) ? 0 : regs_[m];
            uint64_t sv = (sh == 0) ? mv : (mv << sh);
            uint64_t res = nv & sv;
            if (d != 31) regs_[d] = res;
            flag_n_ = (res >> 63) != 0;
            flag_z_ = (res == 0);
            flag_c_ = false;
            flag_v_ = false;
            pc_ += 4;
            steps_++;
            return true;
        }
        if (((insn & 0xFF200000) == 0xAB000000 || (insn & 0xFF200000) == 0xEB000000) &&
            ((insn >> 22) & 0x3) == 0x0) {
            // ADDS / SUBS 64-bit registrado com LSL #n (cobre CMP/CMN)
            bool isAdd = (insn & 0xFF200000) == 0xAB000000;
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>(dec.rm);
            int sh = static_cast<int>((insn >> 10) & 0x3F);
            uint64_t nv = (n == 31) ? 0 : regs_[n];
            uint64_t mv = (m == 31) ? 0 : regs_[m];
            uint64_t sv = (sh == 0) ? mv : (mv << sh);
            uint64_t res = isAdd ? (nv + sv) : (nv - sv);
            if (d != 31) regs_[d] = res;
            flag_n_ = (res >> 63) != 0;
            flag_z_ = (res == 0);
            if (isAdd) {
                flag_c_ = res < nv;
                bool sn = ((nv >> 63) != 0), si = ((sv >> 63) != 0), sr = flag_n_;
                flag_v_ = (sn == si) && (sr != sn);
            } else {
                flag_c_ = nv >= sv;
                bool sn = ((nv >> 63) != 0), si = ((sv >> 63) != 0), sr = flag_n_;
                flag_v_ = (sn != si) && (sr != sn);
            }
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
        if (((insn & 0xFFE00C00) == 0xF8000000) || ((insn & 0xFFE00C00) == 0xF8400000) ||
            ((insn & 0xFFE00C00) == 0xB8000000) || ((insn & 0xFFE00C00) == 0xB8400000) ||
            ((insn & 0xFFE00C00) == 0xB8800000)) {
            // STUR/LDUR 64/32-bit + LDURSW (simm9 sem escala). Vem antes do
            // top==0xF8 (LDUR64 divide o top com STR).
            uint32_t ub = insn & 0xFFE00C00;
            bool is64 = (ub & 0x40000000u) != 0;
            int opc = static_cast<int>((insn >> 22) & 0x3);
            int t = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int64_t off = static_cast<int64_t>((insn >> 12) & 0x1FF);
            if (off & 0x100) off |= ~static_cast<int64_t>(0x1FF);
            uint64_t bb = (n == 31) ? sp_ : regs_[n];
            uint64_t addr = bb + static_cast<uint64_t>(off);
            if (!is64 && opc == 2) { // LDURSW Xt (estende sinal de 32)
                uint64_t pa = 0;
                if (!phys(addr, 4, false, false, pa)) return false;
                uint32_t w = 0;
                __builtin_memcpy(&w, &mem_[static_cast<size_t>(pa)], 4);
                if (t != 31)
                    regs_[t] = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(w)));
            } else {
                int width = is64 ? 8 : 4;
                bool isLoad = (opc == 1);
                uint64_t pa = 0;
                if (!phys(addr, width, !isLoad, false, pa)) return false;
                if (isLoad) {
                    uint64_t v = 0;
                    __builtin_memcpy(&v, &mem_[static_cast<size_t>(pa)], static_cast<size_t>(width));
                    if (t != 31) regs_[t] = v;
                } else {
                    uint64_t v = (t == 31) ? 0 : regs_[t];
                    __builtin_memcpy(&mem_[static_cast<size_t>(pa)], &v, static_cast<size_t>(width));
                }
            }
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
        if ((insn & 0xFFE0FC00) == 0x1AC02000 || (insn & 0xFFE0FC00) == 0x1AC02400 ||
            (insn & 0xFFE0FC00) == 0x1AC02800 || (insn & 0xFFE0FC00) == 0x1AC02C00) {
            // LSLV / LSRV / ASRV / RORV Wd,Wn,Wm (shift = Wm % 32)
            int op = static_cast<int>((insn >> 10) & 0x3);
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            uint32_t nv = (n == 31) ? 0 : static_cast<uint32_t>(regs_[n]);
            uint32_t sh = ((m == 31) ? 0 : static_cast<uint32_t>(regs_[m])) % 32;
            uint32_t res = nv;
            if (op == 0) res = (sh == 0) ? nv : (nv << sh);
            else if (op == 1) res = (sh == 0) ? nv : (nv >> sh);
            else if (op == 2) res = (sh == 0) ? nv : static_cast<uint32_t>(static_cast<int32_t>(nv) >> sh);
            else res = (sh == 0) ? nv : ((nv >> sh) | (nv << (32 - sh)));
            if (d != 31) regs_[d] = res;
            pc_ += 4;
            steps_++;
            return true;
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
                __builtin_memcpy(&v, &mem_[static_cast<size_t>(pa)], 8);
                fp_.d[t] = u2d(v);
            } else {
                uint64_t v = d2u(fp_.d[t]);
                __builtin_memcpy(&mem_[static_cast<size_t>(pa)], &v, 8);
            }
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFC00000) == 0x6D000000 || (insn & 0xFFC00000) == 0x6D400000 ||
            (insn & 0xFFC00000) == 0x6D800000 || (insn & 0xFFC00000) == 0x6DC00000 ||
            (insn & 0xFFC00000) == 0x6C800000 || (insn & 0xFFC00000) == 0x6CC00000) {
            // STP / LDP Dd1,Dd2,[Xn,#imm] offset, pré e pós-index
            uint32_t fam = insn & 0xFFC00000;
            bool isLoad = (fam == 0x6D400000 || fam == 0x6DC00000 || fam == 0x6CC00000);
            bool preIndex = (fam == 0x6D800000 || fam == 0x6DC00000);
            bool postIndex = (fam == 0x6C800000 || fam == 0x6CC00000);
            int t1 = static_cast<int>(insn & 0x1F);
            int n = static_cast<int>((insn >> 5) & 0x1F);
            int t2 = static_cast<int>((insn >> 10) & 0x1F);
            int64_t off = static_cast<int64_t>((insn >> 15) & 0x7F);
            if (off & 0x40) off |= ~static_cast<int64_t>(0x7F);
            uint64_t base = (n == 31) ? sp_ : regs_[n];
            uint64_t addr = postIndex ? base : base + static_cast<uint64_t>(off * 8);
            uint64_t pa = 0;
            if (!phys(addr, 16, !isLoad, false, pa)) return false;
            auto ld = [&](uint64_t a) {
                uint64_t v = 0;
                __builtin_memcpy(&v, &mem_[static_cast<size_t>(a)], 8);
                return v;
            };
            auto st = [&](uint64_t a, uint64_t v) {
                __builtin_memcpy(&mem_[static_cast<size_t>(a)], &v, 8);
            };
            if (isLoad) {
                fp_.d[t1] = u2d(ld(pa));
                fp_.d[t2] = u2d(ld(pa + 8));
            } else {
                st(pa, d2u(fp_.d[t1]));
                st(pa + 8, d2u(fp_.d[t2]));
            }
            if (preIndex || postIndex) {
                uint64_t nb = base + static_cast<uint64_t>(off * 8);
                if (n == 31) sp_ = nb;
                else regs_[n] = nb;
            }
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFC00000) == 0x69400000 || (insn & 0xFFC00000) == 0x69800000 ||
            (insn & 0xFFC00000) == 0x68C00000) {
            // LDPSW Xt1,Xt2,[Xn,#imm] (par com sinal; sem escrita = só leitura)
            uint32_t fam = insn & 0xFFC00000;
            bool preIndex = (fam == 0x69800000);
            bool postIndex = (fam == 0x68C00000);
            int t1 = static_cast<int>(insn & 0x1F);
            int n = static_cast<int>((insn >> 5) & 0x1F);
            int t2 = static_cast<int>((insn >> 10) & 0x1F);
            int64_t off = static_cast<int64_t>((insn >> 15) & 0x7F);
            if (off & 0x40) off |= ~static_cast<int64_t>(0x7F);
            uint64_t base = (n == 31) ? sp_ : regs_[n];
            uint64_t addr = postIndex ? base : base + static_cast<uint64_t>(off * 4);
            auto lds32 = [&](uint64_t a, bool& ok) {
                uint64_t pa = 0;
                ok = phys(a, 4, false, false, pa);
                if (!ok) return int64_t(0);
                uint32_t w = 0;
                __builtin_memcpy(&w, &mem_[static_cast<size_t>(pa)], 4);
                return static_cast<int64_t>(static_cast<int32_t>(w));
            };
            bool ok1 = true, ok2 = true;
            int64_t v1 = lds32(addr, ok1), v2 = lds32(addr + 4, ok2);
            if (!ok1 || !ok2) return false;
            if (t1 != 31) regs_[t1] = static_cast<uint64_t>(v1);
            if (t2 != 31) regs_[t2] = static_cast<uint64_t>(v2);
            if (preIndex || postIndex) {
                uint64_t nb = base + static_cast<uint64_t>(off * 4);
                if (n == 31) sp_ = nb;
                else regs_[n] = nb;
            }
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFC00000) == 0x29000000 || (insn & 0xFFC00000) == 0x29400000 ||
            (insn & 0xFFC00000) == 0x29800000 || (insn & 0xFFC00000) == 0x29C00000 ||
            (insn & 0xFFC00000) == 0x28800000 || (insn & 0xFFC00000) == 0x28C00000) {
            // STP / LDP Wt1,Wt2 (32-bit, zero-extend), offset/pre/pos
            uint32_t fam = insn & 0xFFC00000;
            bool isLoad = (fam == 0x29400000 || fam == 0x29C00000 || fam == 0x28C00000);
            bool preIndex = (fam == 0x29800000 || fam == 0x29C00000);
            bool postIndex = (fam == 0x28800000 || fam == 0x28C00000);
            int t1 = static_cast<int>(insn & 0x1F);
            int n = static_cast<int>((insn >> 5) & 0x1F);
            int t2 = static_cast<int>((insn >> 10) & 0x1F);
            int64_t off = static_cast<int64_t>((insn >> 15) & 0x7F);
            if (off & 0x40) off |= ~static_cast<int64_t>(0x7F);
            uint64_t base = (n == 31) ? sp_ : regs_[n];
            uint64_t addr = postIndex ? base : base + static_cast<uint64_t>(off * 4);
            auto ldw = [&](uint64_t a, bool& ok) {
                uint64_t pa = 0;
                ok = phys(a, 4, false, false, pa);
                if (!ok) return uint32_t(0);
                uint32_t v = 0;
                __builtin_memcpy(&v, &mem_[static_cast<size_t>(pa)], 4);
                return v;
            };
            auto stw = [&](uint64_t a, uint32_t v) {
                uint64_t pa = 0;
                if (!phys(a, 4, true, false, pa)) return false;
                __builtin_memcpy(&mem_[static_cast<size_t>(pa)], &v, 4);
                return true;
            };
            if (isLoad) {
                bool ok1 = true, ok2 = true;
                uint32_t v1 = ldw(addr, ok1), v2 = ldw(addr + 4, ok2);
                if (!ok1 || !ok2) return false;
                if (t1 != 31) regs_[t1] = v1;
                if (t2 != 31) regs_[t2] = v2;
            } else {
                uint32_t v1 = (t1 == 31) ? 0 : static_cast<uint32_t>(regs_[t1]);
                uint32_t v2 = (t2 == 31) ? 0 : static_cast<uint32_t>(regs_[t2]);
                if (!stw(addr, v1) || !stw(addr + 4, v2)) return false;
            }
            if (preIndex || postIndex) {
                uint64_t nb = base + static_cast<uint64_t>(off * 4);
                if (n == 31) sp_ = nb;
                else regs_[n] = nb;
            }
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFC00000) == 0x3D800000 || (insn & 0xFFC00000) == 0x3DC00000) {
            // STR / LDR Qt,[Xn,#imm*16] (128 bits)
            bool isLoad = (insn & 0xFFC00000) == 0x3DC00000;
            int t = static_cast<int>(insn & 0x1F);
            int n = static_cast<int>((insn >> 5) & 0x1F);
            uint64_t base = (n == 31) ? sp_ : regs_[n];
            uint64_t addr = base + static_cast<uint64_t>((insn >> 10) & 0xFFF) * 16u;
            uint64_t pa = 0;
            if (!phys(addr, 16, !isLoad, false, pa)) return false;
            if (isLoad) {
                __builtin_memcpy(&fp_.q[t][0], &mem_[static_cast<size_t>(pa)], 16);
            } else {
                __builtin_memcpy(&mem_[static_cast<size_t>(pa)], &fp_.q[t][0], 16);
            }
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFC00000) == 0xBD000000 || (insn & 0xFFC00000) == 0xBD400000) {
            // STR / LDR St,[Xn,#imm*4] (float bit-exato). size=10 -> top 0xBD
            // (0xB5 seria CBNZ! colisão checada.)
            bool isLoad = (insn & 0xFFC00000) == 0xBD400000;
            int t = static_cast<int>(insn & 0x1F);
            int n = static_cast<int>((insn >> 5) & 0x1F);
            uint64_t base = (n == 31) ? sp_ : regs_[n];
            uint64_t addr = base + static_cast<uint64_t>((insn >> 10) & 0xFFF) * 4u;
            uint64_t pa = 0;
            if (!phys(addr, 4, !isLoad, false, pa)) return false;
            if (isLoad) {
                uint32_t w = 0;
                __builtin_memcpy(&w, &mem_[static_cast<size_t>(pa)], 4);
                float f = 0;
                __builtin_memcpy(&f, &w, 4);
                fp_.d[t] = static_cast<double>(f);
            } else {
                float f = static_cast<float>(fp_.d[t]);
                uint32_t w = 0;
                __builtin_memcpy(&w, &f, 4);
                __builtin_memcpy(&mem_[static_cast<size_t>(pa)], &w, 4);
            }
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFC00000) == 0xAD000000 || (insn & 0xFFC00000) == 0xAD400000 ||
            (insn & 0xFFC00000) == 0xAD800000 || (insn & 0xFFC00000) == 0xADC00000 ||
            (insn & 0xFFC00000) == 0xAC800000 || (insn & 0xFFC00000) == 0xACC00000) {
            // STP / LDP Qt1,Qt2 (128 bits), offset/pre/pos
            uint32_t fam = insn & 0xFFC00000;
            bool isLoad = (fam == 0xAD400000 || fam == 0xADC00000 || fam == 0xACC00000);
            bool preIndex = (fam == 0xAD800000 || fam == 0xADC00000);
            bool postIndex = (fam == 0xAC800000 || fam == 0xACC00000);
            int t1 = static_cast<int>(insn & 0x1F);
            int n = static_cast<int>((insn >> 5) & 0x1F);
            int t2 = static_cast<int>((insn >> 10) & 0x1F);
            int64_t off = static_cast<int64_t>((insn >> 15) & 0x7F);
            if (off & 0x40) off |= ~static_cast<int64_t>(0x7F);
            uint64_t base = (n == 31) ? sp_ : regs_[n];
            uint64_t addr = postIndex ? base : base + static_cast<uint64_t>(off * 16);
            uint64_t pa = 0;
            if (!phys(addr, 32, !isLoad, false, pa)) return false;
            if (isLoad) {
                __builtin_memcpy(&fp_.q[t1][0], &mem_[static_cast<size_t>(pa)], 16);
                __builtin_memcpy(&fp_.q[t2][0], &mem_[static_cast<size_t>(pa) + 16], 16);
            } else {
                __builtin_memcpy(&mem_[static_cast<size_t>(pa)], &fp_.q[t1][0], 16);
                __builtin_memcpy(&mem_[static_cast<size_t>(pa) + 16], &fp_.q[t2][0], 16);
            }
            if (preIndex || postIndex) {
                uint64_t nb = base + static_cast<uint64_t>(off * 16);
                if (n == 31) sp_ = nb;
                else regs_[n] = nb;
            }
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFC00000) == 0x2D000000 || (insn & 0xFFC00000) == 0x2D400000 ||
            (insn & 0xFFC00000) == 0x2D800000 || (insn & 0xFFC00000) == 0x2DC00000 ||
            (insn & 0xFFC00000) == 0x2C800000 || (insn & 0xFFC00000) == 0x2CC00000) {
            // STP / LDP St1,St2 (float 32), offset/pre/pos
            uint32_t fam = insn & 0xFFC00000;
            bool isLoad = (fam == 0x2D400000 || fam == 0x2DC00000 || fam == 0x2CC00000);
            bool preIndex = (fam == 0x2D800000 || fam == 0x2DC00000);
            bool postIndex = (fam == 0x2C800000 || fam == 0x2CC00000);
            int t1 = static_cast<int>(insn & 0x1F);
            int n = static_cast<int>((insn >> 5) & 0x1F);
            int t2 = static_cast<int>((insn >> 10) & 0x1F);
            int64_t off = static_cast<int64_t>((insn >> 15) & 0x7F);
            if (off & 0x40) off |= ~static_cast<int64_t>(0x7F);
            uint64_t base = (n == 31) ? sp_ : regs_[n];
            uint64_t addr = postIndex ? base : base + static_cast<uint64_t>(off * 4);
            auto lds = [&](uint64_t a, bool& ok) {
                uint64_t pa = 0;
                ok = phys(a, 4, false, false, pa);
                if (!ok) return 0.0;
                uint32_t w = 0;
                __builtin_memcpy(&w, &mem_[static_cast<size_t>(pa)], 4);
                float f = 0;
                __builtin_memcpy(&f, &w, 4);
                return static_cast<double>(f);
            };
            auto sts = [&](uint64_t a, double v) {
                uint64_t pa = 0;
                if (!phys(a, 4, true, false, pa)) return false;
                float f = static_cast<float>(v);
                uint32_t w = 0;
                __builtin_memcpy(&w, &f, 4);
                __builtin_memcpy(&mem_[static_cast<size_t>(pa)], &w, 4);
                return true;
            };
            if (isLoad) {
                bool ok1 = true, ok2 = true;
                double v1 = lds(addr, ok1), v2 = lds(addr + 4, ok2);
                if (!ok1 || !ok2) return false;
                fp_.d[t1] = v1;
                fp_.d[t2] = v2;
            } else {
                if (!sts(addr, fp_.d[t1]) || !sts(addr + 4, fp_.d[t2])) return false;
            }
            if (preIndex || postIndex) {
                uint64_t nb = base + static_cast<uint64_t>(off * 4);
                if (n == 31) sp_ = nb;
                else regs_[n] = nb;
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
        if (((insn & 0xFFE00C00) == 0x38000000) || ((insn & 0xFFE00C00) == 0x38400000) ||
            ((insn & 0xFFE00C00) == 0x38800000) || ((insn & 0xFFE00C00) == 0x38C00000) ||
            ((insn & 0xFFE00C00) == 0x78000000) || ((insn & 0xFFE00C00) == 0x78400000) ||
            ((insn & 0xFFE00C00) == 0x78800000) || ((insn & 0xFFE00C00) == 0x78C00000)) {
            // LDUR/STUR byte/half (simm9 sem escala), com e sem sinal.
            // Nota: LDTR/STTR ([25:24]=10) caem aqui de propósito:
            // single-thread EL0, semântica idêntica.
            uint32_t ubase = insn & 0xFFE00C00;
            bool isHalf = (ubase & 0x40000000u) != 0; // size bit30
            int opc = static_cast<int>((insn >> 22) & 0x3);
            int t = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int64_t off = static_cast<int64_t>((insn >> 12) & 0x1FF);
            if (off & 0x100) off |= ~static_cast<int64_t>(0x1FF); // sign 9
            uint64_t bb = (n == 31) ? sp_ : regs_[n];
            uint64_t addr = bb + static_cast<uint64_t>(off);
            int width = isHalf ? 2 : 1;
            if (opc == 0) { // store
                uint64_t pa = 0;
                if (!phys(addr, width, true, false, pa)) return false;
                uint64_t v = (t == 31) ? 0 : regs_[t];
                __builtin_memcpy(&mem_[static_cast<size_t>(pa)], &v, static_cast<size_t>(width));
            } else if (opc == 1) { // load zero-extend
                uint64_t pa = 0;
                if (!phys(addr, width, false, false, pa)) return false;
                uint64_t v = 0;
                __builtin_memcpy(&v, &mem_[static_cast<size_t>(pa)], static_cast<size_t>(width));
                if (t != 31) regs_[t] = v;
            } else { // load com sinal: opc 2 = X (64), opc 3 = W (32)
                uint64_t pa = 0;
                if (!phys(addr, width, false, false, pa)) return false;
                uint64_t raw = 0;
                __builtin_memcpy(&raw, &mem_[static_cast<size_t>(pa)], static_cast<size_t>(width));
                uint64_t res;
                if (width == 1) {
                    int8_t sb = static_cast<int8_t>(raw & 0xFFu);
                    res = (opc == 2) ? static_cast<uint64_t>(static_cast<int64_t>(sb))
                                     : static_cast<uint64_t>(static_cast<uint32_t>(static_cast<int32_t>(sb)));
                } else {
                    int16_t sh = static_cast<int16_t>(raw & 0xFFFFu);
                    res = (opc == 2) ? static_cast<uint64_t>(static_cast<int64_t>(sh))
                                     : static_cast<uint64_t>(static_cast<uint32_t>(static_cast<int32_t>(sh)));
                }
                if (t != 31) regs_[t] = res;
            }
            pc_ += 4;
            steps_++;
            return true;
        }
        if (top == 0x38 || top == 0x39 || top == 0x79) {
            int opc = static_cast<int>((insn >> 22) & 0x3);
            // top 0x39: opc 00=STRB, 01=LDRB, 10=LDRSB X, 11=LDRSB W
            // top 0x79: opc 00=STRH, 01=LDRH, 10=LDRSH X, 11=LDRSH W
            // (top 0x38 aceito por leniência como STRB)
            if (top == 0x38 && opc == 0) return memAccess(dec, 1, false);
            if (top == 0x39 && opc == 0) return memAccess(dec, 1, false);
            if (top == 0x39 && opc == 1) return memAccess(dec, 1, true);
            if (top == 0x79 && opc == 0) return memAccess(dec, 2, false);
            if (top == 0x79 && opc == 1) return memAccess(dec, 2, true);
            // loads com sinal: opc 2 = Xt (64), opc 3 = Wt (32)
            bool isHalf = (top == 0x79);
            int width = isHalf ? 2 : 1;
            bool to64 = (opc == 2);
            int t = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            uint64_t base = (n == 31) ? sp_ : regs_[n];
            uint64_t addr = base + static_cast<uint64_t>(dec.imm12) * static_cast<uint64_t>(width);
            uint64_t pa = 0;
            if (!phys(addr, width, false, false, pa)) return false;
            uint64_t raw = 0;
            __builtin_memcpy(&raw, &mem_[static_cast<size_t>(pa)], static_cast<size_t>(width));
            uint64_t res;
            if (width == 1) {
                int8_t sb = static_cast<int8_t>(raw & 0xFFu);
                res = to64 ? static_cast<uint64_t>(static_cast<int64_t>(sb))
                           : static_cast<uint64_t>(static_cast<uint32_t>(static_cast<int32_t>(sb)));
            } else {
                int16_t sh = static_cast<int16_t>(raw & 0xFFFFu);
                res = to64 ? static_cast<uint64_t>(static_cast<int64_t>(sh))
                           : static_cast<uint64_t>(static_cast<uint32_t>(static_cast<int32_t>(sh)));
            }
            if (t != 31) regs_[t] = res;
            pc_ += 4;
            steps_++;
            return true;
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
            uint64_t addr = postIndex ? base : base + static_cast<uint64_t>(off * 8);
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
        if ((insn & 0xFFE0FC00) == 0x1AC01400 || (insn & 0xFFE0FC00) == 0x1AC01800 ||
            (insn & 0xFFE0FC00) == 0x1AC01C00 || (insn & 0xFFE0FC00) == 0x9AC01400) {
            // CRC32CB/CH/CW/CX (Castagnoli, poli 0x1EDC6F41 refletido 0x82F63B78)
            uint32_t base = insn & 0xFFE0FC00;
            int bytes = (base == 0x1AC01400) ? 1 : (base == 0x1AC01800) ? 2
                        : (base == 0x9AC01400) ? 8 : 4;
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            uint32_t crc = (n == 31) ? 0 : static_cast<uint32_t>(regs_[n]);
            uint64_t mv = (m == 31) ? 0 : regs_[m];
            for (int i = 0; i < bytes; i++) {
                uint32_t byte = static_cast<uint32_t>((mv >> (8 * i)) & 0xFFu);
                crc ^= byte;
                for (int b = 0; b < 8; b++)
                    crc = (crc & 1u) ? ((crc >> 1) ^ 0x82F63B78u) : (crc >> 1);
            }
            if (d != 31) regs_[d] = crc;
            pc_ += 4;
            steps_++;
            return true;
        }
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
        if ((insn & 0xFFE08000) == 0x1B008000) { // MADDW Wd,Wn,Wm,Wa
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            int a = static_cast<int>((insn >> 10) & 0x1F);
            uint32_t nv = (n == 31) ? 0 : static_cast<uint32_t>(regs_[n]);
            uint32_t mv = (m == 31) ? 0 : static_cast<uint32_t>(regs_[m]);
            uint32_t av = (a == 31) ? 0 : static_cast<uint32_t>(regs_[a]);
            if (d != 31) regs_[d] = av + nv * mv;
            pc_ += 4;
            steps_++;
            return true;
        }
        if ((insn & 0xFFE0FC00) == 0x1AC00C00 || (insn & 0xFFE0FC00) == 0x1A800C00) {
            // SDIVW / UDIVW (div por zero = 0)
            bool isSigned = (insn & 0xFFE0FC00) == 0x1AC00C00;
            int d = static_cast<int>(dec.rd);
            int n = static_cast<int>(dec.rn);
            int m = static_cast<int>((insn >> 16) & 0x1F);
            uint32_t nv = (n == 31) ? 0 : static_cast<uint32_t>(regs_[n]);
            uint32_t mv = (m == 31) ? 0 : static_cast<uint32_t>(regs_[m]);
            uint32_t res = 0;
            if (mv != 0) {
                if (isSigned) {
                    int32_t sn = static_cast<int32_t>(nv), sm = static_cast<int32_t>(mv);
                    if (!(sn == INT32_MIN && sm == -1)) res = static_cast<uint32_t>(sn / sm);
                    else res = static_cast<uint32_t>(INT32_MIN);
                } else {
                    res = nv / mv;
                }
            }
            if (d != 31) regs_[d] = res;
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
        logUnknown(insn);
        return false;
    }

private:
    // PSTATE packing/unpacking for exception handling
    uint64_t packPstate() const {
        uint64_t pstate = 0;
        pstate |= (flag_n_ ? 1ull : 0ull) << 31; // N
        pstate |= (flag_z_ ? 1ull : 0ull) << 30; // Z
        pstate |= (flag_c_ ? 1ull : 0ull) << 29; // C
        pstate |= (flag_v_ ? 1ull : 0ull) << 28; // V
        pstate |= (current_el_ & 0x3) << 2;      // M[3:2] = EL
        pstate |= 0x0; // M[1:0] = 0 (AArch64)
        pstate |= 0x1ull << 6; // D (debug mask)
        pstate |= (irq_mask_ ? 1ull : 0ull) << 7; // A (async abort mask)
        pstate |= (irq_mask_ ? 1ull : 0ull) << 8; // I (IRQ mask)
        pstate |= (fiq_mask_ ? 1ull : 0ull) << 9; // F (FIQ mask)
        return pstate;
    }
    void unpackPstate(uint64_t pstate) {
        flag_n_ = (pstate >> 31) & 1;
        flag_z_ = (pstate >> 30) & 1;
        flag_c_ = (pstate >> 29) & 1;
        flag_v_ = (pstate >> 28) & 1;
        current_el_ = (pstate >> 2) & 0x3;
        // Masks D/A/I/F restored from pstate bits 6-9
        irq_mask_ = (pstate >> 8) & 1;
        fiq_mask_ = (pstate >> 9) & 1;
    }

    // Get SPSR/ELR for current EL
    uint64_t& getSpsrForEl(uint64_t el) {
        switch (el) {
            case 1: return spsr_el1_;
            case 2: return spsr_el2_;
            case 3: return spsr_el3_;
            default: return spsr_el1_;
        }
    }
    uint64_t& getElrForEl(uint64_t el) {
        switch (el) {
            case 1: return elr_el1_;
            case 2: return elr_el2_;
            case 3: return elr_el3_;
            default: return elr_el1_;
        }
    }

    // Exception entry for IRQ/FIQ/SError
    void takeException(uint64_t esr_ec, uint64_t vector_offset, uint64_t target_el) {
        getSpsrForEl(target_el) = packPstate();
        getElrForEl(target_el) = pc_ + 4;
        esr_ = esr_ec;
        current_el_ = target_el;
        uint64_t vbar = (target_el == 1) ? vbar_el1_ : (target_el == 2) ? vbar_el2_ : vbar_el3_;
        pc_ = vbar + vector_offset;
        // Mask interrupts on exception entry
        irq_mask_ = true;
        fiq_mask_ = true;
        steps_++;
    }

    // Check and handle pending interrupts (called each instruction)
    void checkInterrupts() {
        // Increment timer counters
        cntpct_++;
        cntvct_++;
        
        // EL1 Physical Timer interrupt (CNTP)
        if (cntp_enabled_ && cntpct_ >= cntp_cval_) {
            irq_pending_vec_.push_back(0); // Timer IRQ source
        }
        // EL1 Virtual Timer interrupt (CNTV)
        if (cntv_enabled_ && cntvct_ >= cntv_cval_) {
            irq_pending_vec_.push_back(1); // Virtual timer IRQ source
        }
        
        // Vectorized IRQ handling - process all pending IRQs
        while (!irq_pending_vec_.empty() && !irq_mask_ && current_el_ <= 1) {
            takeException(0x00, 0x80, 1); // IRQ sync exception
            irq_pending_vec_.erase(irq_pending_vec_.begin());
            return; // Handle one at a time
        }
        // Vectorized FIQ handling
        while (!fiq_pending_vec_.empty() && !fiq_mask_ && current_el_ <= 1) {
            takeException(0x01, 0x100, 1); // FIQ sync exception
            fiq_pending_vec_.erase(fiq_pending_vec_.begin());
            return;
        }
        // SError exception (EL1, vector 0x180)
        if (serror_pending_ && !(packPstate() & (1ull << 7)) && current_el_ <= 1) {
            takeException(0x2F, 0x180, 1); // SError sync exception
            serror_pending_ = false;
            return;
        }
    }

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
        __builtin_memcpy(&v, &mem_[static_cast<size_t>(pa)], 8);
        return v;
    }
    bool store64(uint64_t addr, uint64_t v) {
        uint64_t pa = 0;
        if (!phys(addr, 8, true, false, pa)) return false;
        __builtin_memcpy(&mem_[static_cast<size_t>(pa)], &v, 8);
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
            __builtin_memcpy(&v, &mem_[static_cast<size_t>(pa)], static_cast<size_t>(width));
            if (t != 31) regs_[t] = v; // 32/8-bit já vêm zerados acima
        } else {
            uint64_t v = (t == 31) ? 0 : regs_[t];
            __builtin_memcpy(&mem_[static_cast<size_t>(pa)], &v, static_cast<size_t>(width));
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

    // Arquivo FP/NEON: D[n] é a metade baixa de V[n] (alias real do ARM).
    union FpFile {
        std::array<double, 32> d;
        std::array<std::array<uint64_t, 2>, 32> q;
        FpFile() {
            for (size_t i = 0; i < 32; i++) {
                q[i][0] = 0;
                q[i][1] = 0;
            }
        }
    };
    std::array<uint64_t, REG_COUNT> regs_{};
    FpFile fp_;
    std::vector<uint8_t> mem_;
    std::array<uint32_t, 16> unknown_log_{};
    size_t unknown_pos_ = 0;
    uint64_t unknown_total_ = 0;
    std::array<uint64_t, 64> trace_{};
    size_t trace_pos_ = 0;
    bool trace_on_ = false;
    Mmu* mmu_ = nullptr;
    SvcHost* svc_host_ = nullptr;
    uint32_t last_svc_ = 0;

    void logUnknown(uint32_t insn) {
        unknown_log_[unknown_pos_ % unknown_log_.size()] = insn;
        unknown_pos_++;
        unknown_total_++;
    }
    uint64_t sp_ = 0;
    uint64_t pc_ = 0;
    uint64_t steps_ = 0;
    uint64_t tpidr_ = 0;
    uint64_t fpcr_ = 0, fpsr_ = 0;
    uint64_t sctlr_ = 0, ttbr0_ = 0, ttbr1_ = 0, tcr_ = 0;
    uint64_t mair_ = 0, vbar_ = 0, cpacr_ = 0;
    // Exception state (per-EL)
    uint64_t elr_el1_ = 0, spsr_el1_ = 0;
    uint64_t elr_el2_ = 0, spsr_el2_ = 0;
    uint64_t elr_el3_ = 0, spsr_el3_ = 0;
    uint64_t esr_ = 0, far_ = 0;
    uint64_t current_el_ = 1;
    uint64_t sctlr_el1_ = 0, sctlr_el2_ = 0, sctlr_el3_ = 0;
    uint64_t vbar_el1_ = 0, vbar_el2_ = 0, vbar_el3_ = 0;
    bool stopped_ = false;
    uint64_t exit_code_ = 0;
    bool flag_n_ = false, flag_z_ = false, flag_c_ = false, flag_v_ = false;
    // Interrupt masks (from PSTATE)
    bool irq_mask_ = true;
    bool fiq_mask_ = true;
    // Interrupt pending state (vectorized)
    std::vector<uint64_t> irq_pending_vec_;  // multiple IRQ sources
    std::vector<uint64_t> fiq_pending_vec_;  // multiple FIQ sources
    bool serror_pending_ = false;
    // Generic Timer registers (EL1)
    uint64_t cntp_ctl_ = 0;      // CNTP_CTL_EL0
    uint64_t cntp_cval_ = 0;     // CNTP_CVAL_EL0
    uint64_t cntp_tval_ = 0;     // CNTP_TVAL_EL0
    uint64_t cntpct_ = 0;        // CNTPCT_EL0 (physical counter)
    uint64_t cntv_ctl_ = 0;      // CNTV_CTL_EL0
    uint64_t cntv_cval_ = 0;     // CNTV_CVAL_EL0
    uint64_t cntv_tval_ = 0;     // CNTV_TVAL_EL0
    uint64_t cntvct_ = 0;        // CNTVCT_EL0 (virtual counter)
    bool cntp_enabled_ = false;
    bool cntv_enabled_ = false;
};

} // namespace emu
} // namespace mgd
