#include <cassert>
#include <cstdio>
#include "../src/runtime/ServiceRegistry.h"

int main() {
    port::ServiceRegistry reg;
    assert(reg.size() == 7);
    // boot típico: fs/set/time/audio/vi/am/hid chamados
    assert(reg.call("fs") == port::ServiceMode::Implemented);
    assert(reg.call("vi") == port::ServiceMode::Stubbed);
    assert(reg.call("fs") == port::ServiceMode::Implemented);
    assert(reg.calls("fs") == 2);
    // serviço novo aparece como Unknown (para mapear, não quebrar)
    assert(reg.call("mii") == port::ServiceMode::Unknown);
    assert(reg.calls("mii") == 1);
    assert(reg.size() == 8);
    std::printf("service registry tests passed!\n");
    return 0;
}
