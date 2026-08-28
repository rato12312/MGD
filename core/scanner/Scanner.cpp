#include "Scanner.h"
#include <algorithm>

namespace mgd {

void Scanner::setCache(ICache* c) {
    cache = c;
}

void Scanner::setAdapter(ISourceAdapter* a) {
    adapter = a;
}

void Scanner::scan(const std::string& source_root, Config config) {
    resume_enabled = config.resume;
    progress = ScanProgress{};
    progress.start_time = std::chrono::steady_clock::now();

    FileDiscovery discovery;
    std::vector<FileInfo> files;

    if (adapter) {
        files = discovery.discoverWithExtensions(source_root, adapter->getKnownExtensions());
    } else {
        files = discovery.discover(source_root);
    }

    progress.total_files_discovered = static_cast<uint32_t>(files.size());

    for (const auto& file : files) {
        progress.current_file = file.filename;
        processFile(file);
        progress.updateProgress();
    }

    report.game = adapter ? adapter->getIdentity() : GameIdentity{};
    report.stats = progress;
    report.total_duration_seconds = progress.elapsedSeconds();

    auto entities = getEntityRecords();
    auto resources = getResourceRecords();
    auto collisions = getCollisionRecords();

    report.entity_records = static_cast<uint32_t>(entities.size());
    report.resource_records = static_cast<uint32_t>(resources.size());
    report.collision_records = static_cast<uint32_t>(collisions.size());
}

void Scanner::processFile(const FileInfo& file) {
    progress.files_scanned++;

    uint64_t file_hash = hasher.hashFile(file.path);
    if (file_hash == 0) {
        progress.errors++;
        report.errors.push_back("Failed to hash: " + file.path);
        return;
    }

    if (cache) {
        auto entry = cache->query(static_cast<uint32_t>(file_hash));
        if (entry.has_value() && entry->valid) {
            progress.cache_hits++;
            progress.bytes_processed += file.file_size;
            return;
        }
        progress.cache_misses++;
    }

    if (!adapter) {
        progress.errors++;
        report.errors.push_back("No adapter set for: " + file.path);
        return;
    }

    auto analyzer = adapter->getAnalyzer(file);
    if (!analyzer || !analyzer->canHandle(file)) {
        progress.warnings++;
        report.warnings.push_back("No analyzer for: " + file.path);
        progress.bytes_processed += file.file_size;
        return;
    }

    RawAnalysisResult analysis_result = analyzer->analyze(file);
    if (!analysis_result.success) {
        progress.errors++;
        report.errors.push_back("Analysis failed for " + file.path + ": " + analysis_result.error);
        progress.bytes_processed += file.file_size;
        return;
    }

    progress.files_analyzed++;

    uint32_t asset_id = registry.registerAsset(file, analysis_result);

    for (uint32_t dep : analysis_result.dependency_ids) {
        deps.addDependency(asset_id, dep);
    }

    if (analysis_result.detected_type == AssetType::MESH) {
        ResourceRecord res = infector.normalizeResource(
            asset_id, file.filename, file.path, file_hash, analysis_result.spatial_bounds);
        (void)res;
    } else if (analysis_result.detected_type == AssetType::TEXTURE) {
        int w = 0, h = 0;
        auto wit = analysis_result.metadata.find("width");
        auto hit = analysis_result.metadata.find("height");
        if (wit != analysis_result.metadata.end()) w = std::stoi(wit->second);
        if (hit != analysis_result.metadata.end()) h = std::stoi(hit->second);

        TextureRecord tex = infector.normalizeTexture(asset_id, file.path, w, h);
        tex.hash = file_hash;
        (void)tex;
    }

    if (cache) {
        CacheEntry entry;
        entry.id = asset_id;
        entry.hash = file_hash;
        entry.valid = true;
        entry.version = 1;
        cache->insert(static_cast<uint32_t>(file_hash), entry);
    }

    progress.bytes_processed += file.file_size;
}

ScanReport Scanner::getReport() const {
    return report;
}

ScanProgress Scanner::getProgress() const {
    return progress;
}

std::vector<EntityRecord> Scanner::getEntityRecords() const {
    std::vector<EntityRecord> results;
    auto mesh_assets = registry.getAssetsByType(AssetType::MESH);

    uint32_t entity_id = 1;
    for (const auto& asset : mesh_assets) {
        Vec3 pos = asset.result.spatial_bounds.center();
        EntityRecord rec = infector.normalizeEntity(
            entity_id++, pos, asset.result.spatial_bounds,
            asset.result.metadata.count("visual_ref") ?
                static_cast<uint32_t>(std::stoul(asset.result.metadata.at("visual_ref"))) : 0,
            INVALID_REGION_ID);
        results.push_back(rec);
    }

    return results;
}

std::vector<ResourceRecord> Scanner::getResourceRecords() const {
    std::vector<ResourceRecord> results;
    auto mesh_assets = registry.getAssetsByType(AssetType::MESH);

    for (uint32_t i = 0; i < static_cast<uint32_t>(mesh_assets.size()); ++i) {
        ResourceRecord rec = infector.normalizeResource(
            i + 1, mesh_assets[i].file.filename, mesh_assets[i].file.path,
            hasher.hashData(nullptr, 0), mesh_assets[i].result.spatial_bounds);
        results.push_back(rec);
    }

    return results;
}

std::vector<CollisionRecord> Scanner::getCollisionRecords() const {
    std::vector<CollisionRecord> results;
    auto mesh_assets = registry.getAssetsByType(AssetType::MESH);

    for (uint32_t i = 0; i < static_cast<uint32_t>(mesh_assets.size()); ++i) {
        CollisionRecord rec = infector.normalizeCollision(
            i + 1, ShapeType::AABB, mesh_assets[i].result.spatial_bounds);
        results.push_back(rec);
    }

    return results;
}

} // namespace mgd
