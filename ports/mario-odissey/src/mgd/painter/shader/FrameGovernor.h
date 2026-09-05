#pragma once

#include <cstdint>

namespace mgd {
namespace shader {

// Governador de resolução dinâmica: mede o frame (EMA) e ajusta a escala
// de render para segurar o alvo. Queda gradual (±0.25) para não serrilhado.
// Base da pesquisa: resolução dinâmica é a alavanca padrão para FPS estável.
class FrameGovernor {
public:
    explicit FrameGovernor(float targetFps = 30.0f)
        : target_ms_(1000.0f / targetFps), ema_ms_(1000.0f / targetFps) {}

    // Informa o tempo do frame em ms. Retorna a escala atual (0.5..1.0).
    float update(float frameMs) {
        ema_ms_ = ema_ms_ * 0.9f + frameMs * 0.1f;
        if (ema_ms_ > target_ms_ * 1.1f && scale_ > min_scale_) {
            scale_ -= step_;
            if (scale_ < min_scale_) scale_ = min_scale_;
        } else if (ema_ms_ < target_ms_ * 0.8f && scale_ < 1.0f) {
            scale_ += step_;
            if (scale_ > 1.0f) scale_ = 1.0f;
        }
        return scale_;
    }

    float scale() const { return scale_; }
    float emaMs() const { return ema_ms_; }
    float targetMs() const { return target_ms_; }

private:
    float target_ms_;
    float ema_ms_;
    float scale_ = 1.0f;
    const float min_scale_ = 0.5f;
    const float step_ = 0.25f;
};

} // namespace shader
} // namespace mgd
