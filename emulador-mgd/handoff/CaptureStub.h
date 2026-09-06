#pragma once

// Captura do handoff: hoje stub, amanhã hook no renderer.
// NullHandoff = sem estado (jogo não passou do START).
// ScriptedHandoff = fila de frames para teste do pipeline.

#include <cstdint>
#include <deque>

#include "core/bridge/EmulatorHandoff.h"

namespace mgd {
namespace emu {

class NullHandoff : public bridge::IHandoffSource {
public:
    bool poll(bridge::HandoffFrame&) override { return false; }
};

class ScriptedHandoff : public bridge::IHandoffSource {
public:
    void push(const bridge::HandoffFrame& f) { queue_.push_back(f); }
    bool poll(bridge::HandoffFrame& out) override {
        if (queue_.empty()) return false;
        out = queue_.front();
        queue_.pop_front();
        return true;
    }
    size_t pending() const { return queue_.size(); }

private:
    std::deque<bridge::HandoffFrame> queue_;
};

} // namespace emu
} // namespace mgd
