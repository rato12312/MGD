#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "CacheEntry.h"

namespace mgd {

class ICache {
public:
    virtual ~ICache() = default;

    virtual bool insert(uint32_t key, const CacheEntry& entry) = 0;
    virtual std::optional<CacheEntry> query(uint32_t key) const = 0;
    virtual bool invalidate(uint32_t key) = 0;
    virtual bool exists(uint32_t key) const = 0;
    virtual std::size_t size() const = 0;
    virtual void clear() = 0;

    virtual bool saveCheckpoint(const std::string& path) const = 0;
    virtual bool loadCheckpoint(const std::string& path) = 0;

    virtual std::vector<uint32_t> getAllKeys() const = 0;
};

} // namespace mgd
