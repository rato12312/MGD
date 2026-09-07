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
    uint32_t prio = 0; // menor = mais importante
    bool finished = false;
};

class Scheduler {
public:
    Scheduler() = default;

    uint64_t spawn(uint64_t entry, uint64_t sp, uint32_t prio = 0) {
        Thread t;
        t.id = ++next_id_;
        t.ctx.pc = entry;
        t.ctx.sp = sp;
        t.prio = prio;
        queue_.push_back(t);
        return t.id;
    }

    // Roda até maxSteps no total, quantum por thread (menor prio primeiro).
    uint64_t run(emu::Cpu& cpu, uint64_t maxSteps, uint64_t quantum = 4) {
        uint64_t done = 0;
        while (done < maxSteps && !queue_.empty()) {
            size_t pick = 0;
            for (size_t i = 1; i < queue_.size(); i++) {
                if (queue_[i].prio < queue_[pick].prio) pick = i;
            }
            Thread t = queue_[pick];
            queue_.erase(queue_.begin() + pick);
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
