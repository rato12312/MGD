#include <cassert>
#include <cstdio>
#include "../src/runtime/SystemClock.h"

int main() {
    port::SystemClock clock;
    // steady avança por tick
    auto s0 = clock.steady();
    clock.tick(19200000ull); // 1 segundo
    auto s1 = clock.steady();
    assert(s1.ticks == s0.ticks + 19200000ull);
    // relógio do usuário acompanha (+1s unix)
    auto u0 = clock.userClock();
    (void)u0;
    clock.tick(19200000ull);
    auto u1 = clock.userClock();
    assert(u1.unix_time == 1700000000 + 2);
    // correção automática ligada e fuso padrão (como no log)
    assert(clock.automaticCorrectionEnabled());
    assert(clock.timeZoneName() == "Etc/GMT");
    clock.setLocation("America/Sao_Paulo");
    assert(clock.locationName() == "America/Sao_Paulo");
    std::printf("system clock tests passed!\n");
    return 0;
}
