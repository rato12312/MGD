#include <cassert>
#include <cstdio>
#include <string>
#include "../src/runtime/BootSequence.h"
#include "../src/runtime/ServiceRegistry.h"
#include "../src/runtime/VirtualFs.h"
#include "../src/runtime/SystemClock.h"
#include "../src/runtime/GpuCaps.h"

// Boot integrado com as peças de verdade: serviços registrados, FS montado,
// relógio andando e GPU caps decidindo fallbacks. Falha em qualquer etapa
// para o boot e diz onde (como no log do Eden).
namespace {
port::ServiceRegistry g_services;
port::VirtualFs g_fs;
port::SystemClock g_clock;

bool stepServices(std::string& note) {
    g_services.call("fs");
    g_services.call("set");
    g_services.call("time");
    g_services.call("audio");
    g_services.call("vi");
    g_services.call("am");
    g_services.call("hid");
    note = "7 servicos tocados";
    return true;
}
bool stepFs(std::string& note) {
    g_fs.mount("save:/", "/data/save");
    g_fs.mount("rom:/", "/data/rom");
    auto r = g_fs.resolve("save:/MarioOdyssey/progress");
    if (!r) { note = "save:/ nao resolveu"; return false; }
    note = "save:/ e rom:/ montados";
    return true;
}
bool stepClock(std::string& note) {
    g_clock.tick(19200000ull);
    if (!g_clock.automaticCorrectionEnabled()) { note = "sem correcao"; return false; }
    note = "relogio andando";
    return true;
}
bool stepGpu(std::string& note) {
    port::GpuCaps caps;
    // Mali-G57: cada faltante precisa de um fallback concreto, senão trava aqui
    if (caps.fallbackFor("multiViewport") != port::GpuFallback::SingleViewport) {
        note = "sem fallback de viewport";
        return false;
    }
    if (caps.fallbackFor("shaderClipDistance") != port::GpuFallback::NoClipPlanes) {
        note = "sem fallback de clip";
        return false;
    }
    if (caps.fallbackFor("VK_EXT_vertex_attribute_divisor") != port::GpuFallback::CpuDivisor) {
        note = "sem fallback de divisor";
        return false;
    }
    note = "fallbacks ok";
    return true;
}
} // namespace

int main() {
    port::BootSequence boot;
    boot.addStep(port::BootStage::Services, stepServices);
    boot.addStep(port::BootStage::Filesystem, stepFs);
    boot.addStep(port::BootStage::Clock, stepClock);
    boot.addStep(port::BootStage::Gpu, stepGpu);
    assert(boot.run());
    assert(boot.passed() == 4);
    assert(g_services.calls("fs") == 1);
    assert(g_services.calls("vi") == 1);

    // Sem FS montado, o boot para na etapa certa
    port::BootSequence boot2;
    boot2.addStep(port::BootStage::Filesystem, [](std::string& note) {
        port::VirtualFs empty;
        if (!empty.resolve("save:/x")) { note = "sem mount, sem boot"; return false; }
        return true;
    });
    assert(!boot2.run());
    assert(boot2.results()[0].note == "sem mount, sem boot");

    std::printf("boot full tests passed!\n");
    return 0;
}
