#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace port {

// Orquestrador de boot do port, estilo MGD: cada etapa só roda se a anterior
// passou, e tudo que é descoberto vira registro (nada se perde).
// Ordem espelha o log real de boot: loader -> patches -> serviços -> FS ->
// relógio -> input -> áudio -> GPU caps.
enum class BootStage : uint8_t {
    Loader = 0,
    Patches = 1,
    Services = 2,
    Filesystem = 3,
    Clock = 4,
    Input = 5,
    Audio = 6,
    Gpu = 7,
    Ready = 8,
};

struct BootStepResult {
    BootStage stage;
    bool ok = false;
    std::string note;
};

class BootSequence {
public:
    using StepFn = bool (*)(std::string& note);

    void addStep(BootStage stage, StepFn fn) {
        steps_.push_back(Step{stage, fn});
    }

    // Roda na ordem; para no primeiro que falhar. Retorna true se chegou em Ready.
    bool run() {
        results_.clear();
        for (auto& s : steps_) {
            std::string note;
            bool ok = s.fn ? s.fn(note) : false;
            results_.push_back(BootStepResult{s.stage, ok, note});
            if (!ok) return false;
        }
        return true;
    }

    const std::vector<BootStepResult>& results() const { return results_; }
    size_t passed() const {
        size_t n = 0;
        for (auto& r : results_) if (r.ok) ++n;
        return n;
    }

private:
    struct Step {
        BootStage stage;
        StepFn fn = nullptr;
    };
    std::vector<Step> steps_;
    std::vector<BootStepResult> results_;
};

} // namespace port
