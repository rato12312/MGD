#include <cassert>
#include <cstdio>
#include <string>
#include "../src/runtime/BootSequence.h"

static bool okStep(std::string& note) { note = "ok"; return true; }
static bool failStep(std::string& note) { note = "quebrou aqui"; return false; }

int main() {
    // Boot completo: 8 etapas na ordem passam
    {
        port::BootSequence boot;
        boot.addStep(port::BootStage::Loader, okStep);
        boot.addStep(port::BootStage::Patches, okStep);
        boot.addStep(port::BootStage::Services, okStep);
        boot.addStep(port::BootStage::Filesystem, okStep);
        boot.addStep(port::BootStage::Clock, okStep);
        boot.addStep(port::BootStage::Input, okStep);
        boot.addStep(port::BootStage::Audio, okStep);
        boot.addStep(port::BootStage::Gpu, okStep);
        assert(boot.run());
        assert(boot.passed() == 8);
        assert(boot.results()[0].stage == port::BootStage::Loader);
    }
    // Falha no meio: para e registra onde
    {
        port::BootSequence boot;
        boot.addStep(port::BootStage::Loader, okStep);
        boot.addStep(port::BootStage::Patches, okStep);
        boot.addStep(port::BootStage::Services, failStep);
        boot.addStep(port::BootStage::Filesystem, okStep);
        assert(!boot.run());
        assert(boot.passed() == 2);
        assert(boot.results().size() == 3);
        assert(boot.results()[2].note == "quebrou aqui");
    }
    // Sem etapas: nada para fazer, mas não quebra
    {
        port::BootSequence boot;
        assert(boot.run());
        assert(boot.passed() == 0);
    }

    std::printf("boot sequence tests passed!\n");
    return 0;
}
