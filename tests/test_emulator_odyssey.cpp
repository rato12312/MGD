#include <iostream>
#include <string>

#define ASSERT_MSG(cond, msg) do { if (!(cond)) { std::cerr << "FAIL: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; return false; } } while(0)

#include "emulador-mgd/odyssey/OdysseyWorld.h"

using namespace mgd;

bool run_emulator_odyssey_tests() {
    odyssey::OdysseyWorld world;

    // Modo barato padrão: 0.5x, tudo desligado, reaproveita parado.
    ASSERT_MSG(world.cheap().resolution_factor == 0.5f, "padrao 0.5x");
    ASSERT_MSG(!world.cheap().shadows, "sem sombra");
    world.cheap(odyssey::CheapMode::edge());
    ASSERT_MSG(world.cheap().resolution_factor == 0.4f, "edge 0.4x");

    // Boot do reino: 20 polígonos alimentam o mapa e pintam.
    bridge::RuntimeFrameStats s1 = world.boot(20);
    ASSERT_MSG(s1.polygons_fed == 20, "boot alimenta 20");
    ASSERT_MSG(s1.pixels_written > 0, "boot pinta");

    // Frame parado: painter reaproveita, zero reescrita.
    bridge::RuntimeFrameStats s2 = world.idleFrame(2, 20);
    ASSERT_MSG(s2.pixels_written == 0, "parado nao reescreve");

    std::cout << "  Emulator odyssey tests passed!" << std::endl;
    return true;
}
