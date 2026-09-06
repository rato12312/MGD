#include <cassert>
#include <cstdio>
#include "../src/runtime/RuntimeConfig.h"

int main() {
    port::RuntimeConfig c = port::RuntimeConfig::maliDefaults();
    // Defaults que o log do A15 provou: Mali sem features de desktop
    assert(c.resolution_scale == 0);
    assert(c.gpu_accuracy == 0);
    assert(c.async_shaders);
    assert(!c.extended_dynamic_state);
    assert(!c.compute_pipelines);
    assert(!c.vsync);
    assert(c.cpu_ticks == 16000);
    assert(c.multi_core);
    assert(c.audio_sample_rate == 48000);
    assert(c.audio_channels == 2);
    assert(c.resolutionLabel() == "0.5x");
    // Customizável sem quebrar os defaults
    c.resolution_scale = 1;
    assert(c.resolutionLabel() == "1x");

    std::printf("runtime config tests passed!\n");
    return 0;
}
