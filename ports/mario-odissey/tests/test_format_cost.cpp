#include <cassert>
#include <cstdio>
#include "../src/runtime/FormatCost.h"

int main() {
    port::FormatCostTable table;
    // Mesmo formato: custo zero, não precisa cachear
    assert(table.cost("R8G8B8A8Unorm", "R8G8B8A8Unorm") == port::FormatCost::Zero);
    assert(!table.shouldCache("R8G8B8A8Unorm", "R8G8B8A8Unorm"));
    // O par do spam do log (sRGB <-> Unorm): caro na Mali, sempre cacheia
    assert(table.cost("R8G8B8A8Srgb", "R8G8B8A8Unorm") == port::FormatCost::High);
    assert(table.cost("R8G8B8A8Unorm", "R8G8B8A8Srgb") == port::FormatCost::High);
    assert(table.shouldCache("R8G8B8A8Srgb", "R8G8B8A8Unorm"));
    // Desconhecido = bloqueado (fallback, não invenção)
    assert(table.cost("X8", "Y8") == port::FormatCost::Blocked);
    // Extensível
    table.add("R16G16B16A16Float", "R8G8B8A8Unorm", port::FormatCost::Low);
    assert(table.cost("R16G16B16A16Float", "R8G8B8A8Unorm") == port::FormatCost::Low);
    assert(table.size() == 7);

    std::printf("format cost tests passed!\n");
    return 0;
}
