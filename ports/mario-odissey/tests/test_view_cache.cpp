#include <cassert>
#include <cstdio>
#include "../src/runtime/ViewCache.h"

int main() {
    port::ViewCache cache;
    port::ViewDescriptor a{1, 37, 0, 1}; // imagem 1, RGBA8
    port::ViewDescriptor b{1, 43, 0, 1}; // mesma imagem, formato srgb
    port::ViewDescriptor c{2, 37, 0, 1}; // outra imagem

    uint32_t idA1 = cache.getOrCreate(a);
    uint32_t idA2 = cache.getOrCreate(a);
    assert(idA1 == idA2); // HIT: não recria
    assert(cache.hits() == 1 && cache.misses() == 1);

    uint32_t idB = cache.getOrCreate(b);
    uint32_t idC = cache.getOrCreate(c);
    assert(idB != idA1 && idC != idA1 && idB != idC); // descritores distintos
    assert(cache.size() == 3);

    // 1000 consultas repetidas: 1 miss inicial + resto hit
    cache.resetStats();
    for (int i = 0; i < 1000; ++i) cache.getOrCreate(a);
    assert(cache.hits() == 1000 && cache.misses() == 0);

    std::printf("view cache tests passed!\n");
    return 0;
}
