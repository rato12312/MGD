// Prova de vida do port: instancia as peças vendoradas e imprime o estado.
#include <cstdio>
#include "mgd/query/RegionPolygonCache.h"
#include "mgd/query/dna/DnaPipeline.h"
#include "mgd/painter/shader/ShaderCache.h"
#include "mgd/query/seed/SeedProvider.h"
#include "mgd/bridge/EmulatorHandoff.h"

int main() {
    mgd::RegionPolygonCache cache;
    mgd::dna::DnaPipeline pipe(64, 32);
    mgd::shader::ShaderCache shaders(16);
    mgd::seed::SeedProvider seed(847291ull);
    mgd::bridge::HandoffFrame frame;
    frame.frame_index = 1;
    std::printf("port ok: regions=%zu shaders=%zu seed=%llu frame=%llu\n",
                cache.regionCount(), shaders.size(),
                (unsigned long long)seed.seed(),
                (unsigned long long)frame.frame_index);
    (void)pipe;
    return 0;
}
