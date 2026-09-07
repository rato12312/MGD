#pragma once

// Threads HOS: cada uma carrega seu contexto de CPU.
// Escalonador round-robin simples: N passos por thread, em fila.

#include <cstdint>
#include <deque>
#include <vector>

#include "../cpu/Cpu.h"

namespace mgd {
namespace hos {

struct Thread {
    uint64_t id = 0;
    emu::Cpu::State ctx;
    bool finished = false;
};

class Scheduler {
public:
    Scheduler() = default;

    uint64_t spawn(uint64_t entry, uint64_t sp) {
        Thread t;
        t.id = ++next_id_;
        t.ctx.pc = entry;
        t.ctx.sp = sp;
        queue_.push_back(t);
        return t.id;
    }

    // Roda até maxSteps no total, quantum por thread. Retorna executadas.
    uint64_t run(emu::Cpu& cpu, uint64_t maxSteps, uint64_t quantum = 4) {
        uint64_t done = 0;
        while (done < maxSteps && !queue_.empty()) {
            Thread t = queue_.front();
            queue_.pop_front();
            cpu.load(t.ctx);
            uint64_t got = cpu.run(quantum);
            done += got;
            t.ctx = cpu.save();
            if (!cpu.stopped()) {
                queue_.push_back(t);
            }
            if (got == 0) break; // travou: não gira em falso
        }
        return done;
    }

    size_t pending() const { return queue_.size(); }

private:
    std::deque<Thread> queue_;
    uint64_t next_id_ = 0;
};

} // namespace hos
} // namespace mgd
