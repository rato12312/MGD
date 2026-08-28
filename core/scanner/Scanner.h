#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "../common/Types.h"
#include "../cache/ICache.h"
#include "ISourceAdapter.h"
#include "FileDiscovery.h"
#include "HashService.h"
#include "AssetRegistry.h"
#include "DependencyResolver.h"
#include "ScanProgress.h"
#include "ScanReport.h"
#include "normalize/EntityRecord.h"
#include "normalize/ResourceRecord.h"
#include "normalize/CollisionRecord.h"
#include "normalize/Infector.h"

namespace mgd {

class Scanner {
    ICache* cache = nullptr;
    ISourceAdapter* adapter = nullptr;
    ScanProgress progress;
    AssetRegistry registry;
    DependencyResolver deps;
    HashService hasher;
    Infector infector;
    ScanReport report;
    bool resume_enabled = false;

    mutable std::vector<EntityRecord> entity_records_;
    mutable std::vector<ResourceRecord> resource_records_;
    mutable std::vector<CollisionRecord> collision_records_;
    mutable std::unordered_map<std::string, uint64_t> file_hash_by_path_;
    mutable bool records_built_ = false;

    void processFile(const FileInfo& file);
    void buildRecords() const;

public:
    struct Config {
        ScanMode mode;
        bool resume;
        Config() : mode(ScanMode::FULL_ANALYSIS), resume(false) {}
    };

    void setCache(ICache* cache);
    void setAdapter(ISourceAdapter* adapter);

    void scan(const std::string& source_root, Config config = Config());
    ScanReport getReport() const;
    ScanProgress getProgress() const;

    std::vector<EntityRecord> getEntityRecords() const;
    std::vector<ResourceRecord> getResourceRecords() const;
    std::vector<CollisionRecord> getCollisionRecords() const;
};

} // namespace mgd
