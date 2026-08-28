#include "FileCache.h"
#include "CacheRecord.h"
#include <cstring>
#include <fstream>

namespace mgd {

static constexpr char MAGIC[4] = {'M','G','D','C'};
static constexpr uint32_t FILE_VERSION = 1;

struct FileHeader {
    char magic[4];
    uint32_t version;
    uint32_t entry_count;
    uint32_t reserved;
};

bool FileCache::insert(uint32_t key, const CacheEntry& entry) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto [it, inserted] = m_entries.try_emplace(key, entry);
    if (!inserted) {
        it->second = entry;
    }
    return true;
}

std::optional<CacheEntry> FileCache::query(uint32_t key) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_entries.find(key);
    if (it == m_entries.end() || !it->second.valid) {
        return std::nullopt;
    }
    return it->second;
}

bool FileCache::invalidate(uint32_t key) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_entries.find(key);
    if (it == m_entries.end()) {
        return false;
    }
    it->second.valid = false;
    return true;
}

bool FileCache::exists(uint32_t key) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_entries.find(key);
    return it != m_entries.end() && it->second.valid;
}

std::size_t FileCache::size() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_entries.size();
}

void FileCache::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_entries.clear();
}

std::vector<uint32_t> FileCache::getAllKeys() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<uint32_t> keys;
    keys.reserve(m_entries.size());
    for (const auto& [k, v] : m_entries) {
        keys.push_back(k);
    }
    return keys;
}

bool FileCache::saveCheckpoint(const std::string& path) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) {
        return false;
    }

    FileHeader header{};
    std::memcpy(header.magic, MAGIC, 4);
    header.version = FILE_VERSION;
    header.entry_count = static_cast<uint32_t>(m_entries.size());
    header.reserved = 0;
    out.write(reinterpret_cast<const char*>(&header), sizeof(FileHeader));

    for (const auto& [key, entry] : m_entries) {
        CacheRecordHeader rh{};
        rh.type = entry.type;
        rh.id = entry.id;
        rh.source_hash = entry.hash;
        rh.mgd_format_version = entry.version;
        rh.data_size = static_cast<uint32_t>(entry.data.size());

        out.write(reinterpret_cast<const char*>(&rh), sizeof(CacheRecordHeader));
        out.write(reinterpret_cast<const char*>(&entry.timestamp), sizeof(uint64_t));
        out.write(reinterpret_cast<const char*>(&entry.valid), sizeof(bool));
        out.write(reinterpret_cast<const char*>(entry.data.data()), rh.data_size);
    }

    return out.good();
}

bool FileCache::loadCheckpoint(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return false;
    }

    FileHeader header{};
    in.read(reinterpret_cast<char*>(&header), sizeof(FileHeader));

    if (std::memcmp(header.magic, MAGIC, 4) != 0 || header.version != FILE_VERSION) {
        return false;
    }

    m_entries.clear();
    m_entries.reserve(header.entry_count);

    for (uint32_t i = 0; i < header.entry_count; ++i) {
        CacheRecordHeader rh{};
        in.read(reinterpret_cast<char*>(&rh), sizeof(CacheRecordHeader));

        uint64_t timestamp = 0;
        bool valid = false;
        in.read(reinterpret_cast<char*>(&timestamp), sizeof(uint64_t));
        in.read(reinterpret_cast<char*>(&valid), sizeof(bool));

        CacheEntry entry{};
        entry.id = rh.id;
        entry.type = rh.type;
        entry.hash = rh.source_hash;
        entry.timestamp = timestamp;
        entry.version = rh.mgd_format_version;
        entry.valid = valid;

        if (rh.data_size > 0) {
            entry.data.resize(rh.data_size);
            in.read(reinterpret_cast<char*>(entry.data.data()), rh.data_size);
        }

        m_entries.try_emplace(rh.id, std::move(entry));
    }

    return in.good();
}

} // namespace mgd
