#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace mgd {

class HashService {
    std::unordered_map<std::string, uint64_t> hash_cache;

public:
    uint64_t hashFile(const std::string& path);
    uint64_t hashData(const uint8_t* data, size_t len) const;
    void clearCache();
    size_t cacheSize() const;
};

} // namespace mgd
