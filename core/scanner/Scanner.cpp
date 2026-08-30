#include "Scanner.h"
#include "../mental_map/ChunkManager.h"
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

    buildRecords();

    report.entity_records = static_cast<uint32_t>(entity_records_.size());
    report.resource_records = static_cast<uint32_t>(resource_records_.size());
    report.collision_records = static_cast<uint32_t>(collision_records_.size());
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
        // Obra-prima cache por IDs: 32-bit key pode colidir, valida hash 64 completo
        // e repovoa o registry para que buildRecords não perca entidades em resume.
        if (entry.has_value() && entry->valid && entry->hash == file_hash) {
            progress.cache_hits++;
            progress.bytes_processed += file.file_size;
            // Se for mesh, garante que o registry tenha algo para buildRecords
            // (cache hit não re-analisa, mas precisa contar como asset para chunks)
            if (file.extension == ".nif") {
                RawAnalysisResult cached_result;
                cached_result.file = file;
                cached_result.detected_type = AssetType::MESH;
                cached_result.spatial_bounds = AABB{Vec3(-1.0f,-1.0f,-1.0f), Vec3(1.0f,1.0f,1.0f)};
                cached_result.metadata["cached"] = "true";
                cached_result.success = true;
                registry.registerAsset(file, cached_result);
            }
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
    file_hash_by_path_[file.path] = file_hash;

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

void Scanner::buildRecords() const {
    entity_records_.clear();
    resource_records_.clear();
    collision_records_.clear();

    auto mesh_assets = registry.getAssetsByType(AssetType::MESH);

    uint32_t idx = 0;
    for (const auto& asset : mesh_assets) {
        ResourceID rid = idx + 1;

        AABB bounds = asset.result.spatial_bounds;
        bool valid = bounds.min.x <= bounds.max.x &&
                     bounds.min.y <= bounds.max.y &&
                     bounds.min.z <= bounds.max.z;
        if (!valid) {
            bounds = AABB{Vec3(-1.0f, -1.0f, -1.0f), Vec3(1.0f, 1.0f, 1.0f)};
        }

        auto hit = file_hash_by_path_.find(asset.file.path);
        uint64_t hash = (hit != file_hash_by_path_.end()) ? hit->second : 0;

        resource_records_.push_back(
            infector.normalizeResource(rid, asset.file.filename, asset.file.path, hash, bounds));

        collision_records_.push_back(
            infector.normalizeCollision(rid, ShapeType::AABB, bounds));

        uint32_t visual_ref = 0;
        auto vit = asset.result.metadata.find("visual_ref");
        if (vit != asset.result.metadata.end()) {
            visual_ref = static_cast<uint32_t>(std::stoul(vit->second));
        }

        // Obra-prima chunks: Skyrim LE como Minecraft — cada mesh vai para
        // seu chunk (4096x4096). Quanto maior o jogo (LE+DLCs), mais chunks.
        // Base para puxar rápido: ID distribuído em chunk.
        Vec3 pos = bounds.center();
        // Se o asset vier de ESP/BSA, tenta extrair posição de metadata (se houver)
        // senão usa centro do bounds. Legendary Edition: arrange em grid para demo
        // quando bounds são placeholder (-1..1) — espalha para não colapsar.
        if (bounds.min.x == -1.0f && bounds.max.x == 1.0f) {
            // Placeholder bounds → espalha em grid 10x10 por chunk para visualização
            float gx = static_cast<float>((idx % 10) * 400 - 1800);
            float gz = static_cast<float>((idx / 10) * 400 - 1800);
            pos = Vec3{gx, 0.0f, gz};
        }
        RegionID region = ChunkManager::worldToRegionId(pos);

        // Sibling:DependencyResolver já tem dependency_ids do analyzer (ESP masters etc.)
        // Aqui usamos parent_id = chunk-parent para hierarchy: todas entidades do
        // mesmo chunk compartilham o chunk como parent lógico (se houver mais de 1).
        EntityID parent = INVALID_ENTITY_ID;

        entity_records_.push_back(infector.normalizeEntity(
            rid, pos, bounds, visual_ref, region, rid, rid, parent));

        ++idx;
    }

    records_built_ = true;
}

std::vector<EntityRecord> Scanner::getEntityRecords() const {
    if (!records_built_) buildRecords();
    return entity_records_;
}

std::vector<ResourceRecord> Scanner::getResourceRecords() const {
    if (!records_built_) buildRecords();
    return resource_records_;
}

std::vector<CollisionRecord> Scanner::getCollisionRecords() const {
    if (!records_built_) buildRecords();
    return collision_records_;
}

} // namespace mgd
