#pragma once

#include <cstdint>
#include <string>

namespace port {

// Configuração do port espelhando o que o log do A15 provou necessário:
// Mali-G57 sem as features de desktop, então os defaults já nascem
// Mali-amigáveis (resolução baixa, precisão low, sem EDS).
struct RuntimeConfig {
    // Render: 0 = 0.5x, 1 = 1x
    int resolution_scale = 0;
    // 0 = low, 1 = normal, 2 = high
    int gpu_accuracy = 0;
    bool async_shaders = true;
    bool extended_dynamic_state = false;
    bool compute_pipelines = false;
    bool vsync = false;
    // CPU: NCE com ticks padrão do log (16000)
    uint32_t cpu_ticks = 16000;
    bool multi_core = true;
    // Áudio: cubeb stereo 48kHz
    uint32_t audio_sample_rate = 48000;
    uint32_t audio_channels = 2;

    static RuntimeConfig maliDefaults() {
        RuntimeConfig c;
        c.resolution_scale = 0;
        c.gpu_accuracy = 0;
        c.async_shaders = true;
        c.extended_dynamic_state = false;
        c.compute_pipelines = false;
        c.vsync = false;
        return c;
    }

    std::string resolutionLabel() const {
        return resolution_scale == 0 ? "0.5x" : "1x";
    }
};

} // namespace port
