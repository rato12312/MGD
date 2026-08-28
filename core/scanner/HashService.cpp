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

    file.seekg(0, std::ios::end);
    std::streamoff file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    uint64_t hash = static_cast<uint64_t>(file_size);

    std::array<uint8_t, 4096> buffer{};
    size_t to_read = static_cast<size_t>(std::min(file_size, static_cast<std::streamoff>(4096)));
    file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(to_read));

    for (size_t i = 0; i < to_read; ++i) {
        hash ^= static_cast<uint64_t>(buffer[i]) << (i % 8 * 8);
        hash *= 0x100000001B3ULL;
    }

    hash_cache[path] = hash;
    return hash;
}

uint64_t HashService::hashData(const uint8_t* data, size_t len) const {
    uint64_t hash = len;
    for (size_t i = 0; i < len; ++i) {
        hash ^= static_cast<uint64_t>(data[i]) << (i % 8 * 8);
        hash *= 0x100000001B3ULL;
    }
    return hash;
}

void HashService::clearCache() {
    hash_cache.clear();
}

size_t HashService::cacheSize() const {
    return hash_cache.size();
}

} // namespace mgd
