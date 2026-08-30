#include "HashService.h"
#include <algorithm>
#include <fstream>
#include <array>

namespace mgd {

uint64_t HashService::hashFile(const std::string& path) {
    auto it = hash_cache.find(path);
    if (it != hash_cache.end()) {
        return it->second;
    }

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return 0;
    }

    // FNV-1a 64: seed with file size for cache differentiation, then
    // stream the whole file in 64KB chunks — Legendary Edition BSA
    // files are 500MB+, 4KB sample hid DLC collisions. Obra-prima
    // cache por IDs exige hash estável de todo o conteúdo.
    file.seekg(0, std::ios::end);
    std::streamoff file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    uint64_t hash = 1469598103934665603ULL ^ static_cast<uint64_t>(file_size);
    if (file_size == 0) {
        hash *= 1099511628211ULL;
    }

    std::array<uint8_t, 65536> buffer{};
    while (file) {
        file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
        std::streamsize n = file.gcount();
        for (std::streamsize i = 0; i < n; ++i) {
            hash ^= static_cast<uint64_t>(buffer[static_cast<size_t>(i)]);
            hash *= 1099511628211ULL;
        }
        if (n == 0) break;
    }

    hash_cache[path] = hash;
    return hash;
}

uint64_t HashService::hashData(const uint8_t* data, size_t len) const {
    uint64_t hash = 1469598103934665603ULL ^ static_cast<uint64_t>(len);
    for (size_t i = 0; i < len; ++i) {
        hash ^= static_cast<uint64_t>(data[i]);
        hash *= 1099511628211ULL;
    }
    if (len == 0) hash *= 1099511628211ULL;
    return hash;
}

void HashService::clearCache() {
    hash_cache.clear();
}

size_t HashService::cacheSize() const {
    return hash_cache.size();
}

} // namespace mgd
