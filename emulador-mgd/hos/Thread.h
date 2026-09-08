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
    uint64_t wake_at = 0; // dorme até elapsed_ (0 = acordada)
    emu::Mmu* aspace = nullptr; // mapa da thread (nullptr = direto)
    bool finished = false;
};

class Scheduler {
public:
    Scheduler() = default;

    uint64_t spawn(uint64_t entry, uint64_t sp, uint32_t prio = 0, emu::Mmu* aspace = nullptr,
                   uint64_t tls = 0) {
        Thread t;
        t.id = ++next_id_;
        t.ctx.pc = entry;
        t.ctx.sp = sp;
        t.ctx.tpidr = (tls != 0) ? tls : (0x200000ull + t.id * 0x1000ull);
        t.prio = prio;
        t.aspace = aspace;
        queue_.push_back(t);
        return t.id;
    }

    // Dorme id por ticks (acorda sozinha no elapsed_).
    bool sleep(uint64_t id, uint64_t ticks) {
        for (auto& t : queue_) {
            if (t.id == id) {
                t.wake_at = elapsed_ + ticks;
                return true;
            }
        }
        return false;
    }

    // Roda até maxSteps no total, quantum por fatia (menor prio acordada primeiro).
    uint64_t run(emu::Cpu& cpu, uint64_t maxSteps, uint64_t quantum = 4) {
        uint64_t done = 0;
        while (done < maxSteps && !queue_.empty()) {
            int pick = pickReady();
            if (pick < 0) {
                // ninguém acordada: salta o relógio p/ o próximo wake
                uint64_t w = queue_[0].wake_at;
                for (const auto& t : queue_)
                    if (t.wake_at < w) w = t.wake_at;
                elapsed_ = w;
                continue;
            }
            Thread t = queue_[static_cast<size_t>(pick)];
            queue_.erase(queue_.begin() + pick);
            cpu.setMmu(t.aspace); // isolamento: cada thread no seu mapa
            cpu.load(t.ctx);
            uint64_t got = cpu.run(quantum);
            done += got;
            elapsed_ += got;
            t.ctx = cpu.save();
            if (!cpu.stopped()) {
                queue_.push_back(t);
            }
            if (got == 0) break; // travou: não gira em falso
        }
        return done;
    }

    size_t pending() const { return queue_.size(); }
    uint64_t elapsed() const { return elapsed_; }

private:
    // Índice da pronta com menor prio, ou -1 se todas dormem.
    int pickReady() const {
        int pick = -1;
        for (size_t i = 0; i < queue_.size(); i++) {
            if (queue_[i].wake_at > elapsed_) continue;
            if (pick < 0 || queue_[i].prio < queue_[static_cast<size_t>(pick)].prio)
                pick = static_cast<int>(i);
        }
        return pick;
    }

    std::deque<Thread> queue_;
    uint64_t next_id_ = 0;
    uint64_t elapsed_ = 0;
};

} // namespace hos
} // namespace mgd
