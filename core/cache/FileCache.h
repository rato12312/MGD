#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include "ICache.h"

namespace mgd {

class FileCache : public ICache {
public:
    FileCache() = default;
    ~FileCache() override = default;

    bool insert(uint32_t key, const CacheEntry& entry) override;
    std::optional<CacheEntry> query(uint32_t key) const override;
    bool invalidate(uint32_t key) override;
    bool exists(uint32_t key) const override;
    std::size_t size() const override;
    void clear() override;

    bool saveCheckpoint(const std::string& path) const override;
    bool loadCheckpoint(const std::string& path) override;

    std::vector<uint32_t> getAllKeys() const override;

private:
    mutable std::mutex m_mutex;
    std::unordered_map<uint32_t, CacheEntry> m_entries;
};

} // namespace mgd
