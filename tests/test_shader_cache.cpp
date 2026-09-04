#include <cstdio>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#define ASSERT_MSG(cond, msg) do { if (!(cond)) { std::cerr << "FAIL: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; return false; } } while(0)

#include "core/painter/shader/ShaderCache.h"

using namespace mgd;
using namespace mgd::shader;

bool run_shader_cache_tests() {
    // 1. Miss no cache vazio, depois hit após store
    {
        ShaderCache cache(16);
        ShaderKey k{82, 102, 0};
        ASSERT_MSG(cache.lookup(k) == nullptr, "miss on empty");
        cache.store(k, 1001, 0xABCD);
        const ShaderEntry* e = cache.lookup(k);
        ASSERT_MSG(e != nullptr, "hit after store");
        ASSERT_MSG(e->pipeline_code == 1001, "pipeline code preserved");
        ASSERT_MSG(e->valid, "entry valid");
    }

    // 2. Chave inclui AssetID+PolygonID+flags (bases diferentes não colidem)
    {
        ShaderCache cache(16);
        ShaderKey a{82, 102, 0};
        ShaderKey b{82, 103, 0}; // outro polígono, mesmo asset
        ShaderKey c{83, 102, 0}; // outro asset
        cache.store(a, 1, 111);
        ASSERT_MSG(cache.lookup(b) == nullptr, "different polygon -> miss");
        ASSERT_MSG(cache.lookup(c) == nullptr, "different asset -> miss");
        cache.store(b, 2, 222);
        ASSERT_MSG(cache.lookup(a)->pipeline_code == 1, "first entry kept");
        ASSERT_MSG(cache.lookup(b)->pipeline_code == 2, "second entry kept");
    }

    // 3. Invalidação por asset (troca de material/textura)
    {
        ShaderCache cache(16);
        cache.store(ShaderKey{82, 102, 0}, 1, 111);
        cache.store(ShaderKey{82, 103, 0}, 2, 222);
        cache.store(ShaderKey{83, 102, 0}, 3, 333);
        ASSERT_MSG(cache.invalidateAsset(82), "invalidated asset 82");
        ASSERT_MSG(cache.lookup(ShaderKey{82, 102, 0}) == nullptr, "82/102 gone");
        ASSERT_MSG(cache.lookup(ShaderKey{83, 102, 0}) != nullptr, "83 kept");
        ASSERT_MSG(!cache.invalidateAsset(999), "unknown asset -> false");
    }

    // 4. Evicção com capacidade cheia (FIFO)
    {
        ShaderCache cache(2);
        cache.store(ShaderKey{1, 1, 0}, 1, 1);
        cache.store(ShaderKey{2, 2, 0}, 2, 2);
        cache.store(ShaderKey{3, 3, 0}, 3, 3); // evict primeira
        ASSERT_MSG(cache.size() == 2, "capacity respected");
        ASSERT_MSG(cache.lookup(ShaderKey{1, 1, 0}) == nullptr, "oldest evicted");
        ASSERT_MSG(cache.lookup(ShaderKey{3, 3, 0}) != nullptr, "newest kept");
    }

    // 5. Stats de hit/miss para benchmark
    {
        ShaderCache cache(16);
        ShaderKey k{82, 102, 0};
        cache.resetStats();
        cache.lookup(k); // miss
        cache.store(k, 1, 1);
        cache.lookup(k); // hit
        cache.lookup(k); // hit
        ASSERT_MSG(cache.misses() == 1, "one miss");
        ASSERT_MSG(cache.hits() == 2, "two hits");
    }

    // 6. Warmup no loading: pré-compila tudo antes do primeiro frame
    {
        ShaderCache cache(16);
        std::vector<ShaderKey> keys = {ShaderKey{1, 1, 0}, ShaderKey{2, 2, 0}, ShaderKey{3, 3, 0}};
        size_t compiled = cache.warmup(keys, [](const ShaderKey& k) {
            return std::make_pair(k.asset_id * 1000u + k.polygon_id, 0xFFull);
        });
        ASSERT_MSG(compiled == 3, "warmup compiles all misses");
        size_t compiled2 = cache.warmup(keys, [](const ShaderKey& k) {
            return std::make_pair(0u, 0ull);
        });
        ASSERT_MSG(compiled2 == 0, "warmup reuses hits, zero recompiles");
        ASSERT_MSG(cache.lookup(keys[0])->pipeline_code == 1001, "warmed pipeline kept");
    }

    // 7. Persistência perm: segunda abertura não recompila nada
    {
        const std::string path = "shader_cache_test.bin";
        {
            ShaderCache cache(16);
            cache.store(ShaderKey{82, 102, 0}, 1001, 0xABCD);
            cache.store(ShaderKey{83, 103, 1}, 1002, 0xEF01);
            ASSERT_MSG(cache.save(path), "save works");
        }
        {
            ShaderCache cache(16);
            ASSERT_MSG(cache.load(path), "load works");
            ASSERT_MSG(cache.size() == 2, "two entries restored");
            const ShaderEntry* e = cache.lookup(ShaderKey{82, 102, 0});
            ASSERT_MSG(e && e->pipeline_code == 1001 && e->hash == 0xABCD, "entry restored intact");
            ASSERT_MSG(cache.lookup(ShaderKey{99, 99, 9}) == nullptr, "unknown still miss");
        }
        std::remove(path.c_str());
        ASSERT_MSG(!ShaderCache().load("no_such_file_xyz.bin"), "missing file -> false");
    }

    std::cout << "  Shader cache tests passed!" << std::endl;
    return true;
}
