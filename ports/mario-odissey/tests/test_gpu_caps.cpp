#include <cassert>
#include <cstdio>
#include "../src/runtime/GpuCaps.h"

int main() {
    port::GpuCaps caps;
    // Tudo que o log provou que falta tem fallback (nada fica sem caminho)
    assert(caps.needsFallback("multiViewport"));
    assert(caps.fallbackFor("multiViewport") == port::GpuFallback::SingleViewport);
    assert(caps.fallbackFor("shaderClipDistance") == port::GpuFallback::NoClipPlanes);
    assert(caps.fallbackFor("shaderCullDistance") == port::GpuFallback::NoClipPlanes);
    assert(caps.fallbackFor("VK_EXT_vertex_attribute_divisor") == port::GpuFallback::CpuDivisor);
    assert(caps.fallbackFor("multiDrawIndirect") == port::GpuFallback::CpuIndirect);
    assert(caps.fallbackFor("fillModeNonSolid") == port::GpuFallback::WireframeOff);
    assert(caps.fallbackFor("vertexPipelineStoresAndAtomics") == port::GpuFallback::NoAtomics);
    // O que existe segue normal
    assert(!caps.needsFallback("geometryShaders"));
    assert(!caps.needsFallback("spirv14"));
    // Desconhecido tenta normal (não quebra)
    assert(!caps.needsFallback("something_new"));
    // 9 faltantes mapeados do log
    assert(caps.missingCount() == 9);

    std::printf("gpu caps tests passed!\n");
    return 0;
}
