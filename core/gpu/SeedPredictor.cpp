// Seed Predictor Implementation

#include "SeedPredictor.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cmath>

namespace mgd {
namespace gpu {

bool SeedPredictor::init(ShaderRecompiler* recompiler,
                         core::CameraMentalMapQuery* camera_query,
                         core::MentalMapRuntime* mental_map) {
    recompiler_ = recompiler;
    camera_query_ = camera_query;
    mental_map_ = mental_map;
    
    if (!recompiler_ || !camera_query_ || !mental_map_) {
        return false;
    }
    
    // Config defaults
    config_.max_seeds_per_frame = 64;
    config_.min_visibility_threshold = 0.3f;
    config_.prediction_horizon_frames = 2;
    config_.enable_background_compile = true;
    config_.enable_spatial_prediction = true;
    config_.enable_temporal_prediction = true;
    config_.max_background_compiles_per_frame = 4;
    
    return true;
}

void SeedPredictor::shutdown() {
    recompiler_ = nullptr;
    camera_query_ = nullptr;
    mental_map_ = nullptr;
    seed_cache_.clear();
    compile_queue_.clear();
}

void SeedPredictor::updateGameState(const GameStateSnapshot& state) {
    mario_state_.x = state.mario_x;
    mario_state_.y = state.mario_y;
    mario_state_.z = state.mario_z;
    mario_state_.vx = state.mario_vx;
    mario_state_.vy = state.mario_vy;
    mario_state_.vz = state.mario_vz;
    mario_state_.previous_area = mario_state_.current_area;
    mario_state_.current_area = state.mario_area;
}

std::vector<Seed> SeedPredictor::predictSeeds(uint64_t current_frame) {
    std::vector<Seed> all_seeds;
    
    // 1. Previsão baseada na câmera (objetos no frustum)
    if (config_.enable_spatial_prediction) {
        auto camera_seeds = predictFromCamera(current_frame);
        all_seeds.insert(all_seeds.end(), camera_seeds.begin(), camera_seeds.end());
    }
    
    // 2. Previsão baseada no movimento do Mario
    if (config_.enable_temporal_prediction) {
        auto mario_seeds = predictFromMarioMovement(current_frame);
        all_seeds.insert(all_seeds.end(), mario_seeds.begin(), mario_seeds.end());
    }
    
    // 3. Previsão de transição de área
    if (mario_state_.current_area != mario_state_.previous_area && 
        mario_state_.previous_area != 0) {
        auto area_seeds = predictFromAreaTransition(current_frame);
        all_seeds.insert(all_seeds.end(), area_seeds.begin(), area_seeds.end());
    }
    
    // 4. Previsão baseada em objetos visíveis atuais
    auto visible_seeds = predictFromVisibleObjects(current_frame);
    all_seeds.insert(all_seeds.end(), visible_seeds.begin(), visible_seeds.end());
    
    // Deduplica e filtra por threshold
    std::sort(all_seeds.begin(), all_seeds.end(), 
        [](const Seed& a, const Seed& b) {
            if (a.priority != b.priority) return a.priority > b.priority;
            return a.expected_visibility > b.expected_visibility;
        });
    
    // Remove duplicatas (mesmo DNA)
    std::vector<Seed> unique_seeds;
    std::unordered_set<uint64_t> seen_dna;
    for (const auto& seed : all_seeds) {
        uint64_t dna_hash = seed.dna.hash();
        if (seen_dna.insert(dna_hash).second) {
            if (seed.expected_visibility >= config_.min_visibility_threshold) {
                unique_seeds.push_back(seed);
            }
        }
    }
    
    // Limita quantidade
    if (unique_seeds.size() > config_.max_seeds_per_frame) {
        unique_seeds.resize(config_.max_seeds_per_frame);
    }
    
    // Atualiza cache de seeds
    for (auto& seed : unique_seeds) {
        seed.frame_predicted = current_frame;
        uint64_t dna_hash = seed.dna.hash();
        auto it = seed_cache_.find(dna_hash);
        if (it != seed_cache_.end()) {
            it->second.last_frame_seen = current_frame;
            it->second.hit_count++;
            seed.priority = std::max(seed.priority, static_cast<uint32_t>(std::min(3, it->second.hit_count)));
        } else {
            seed_cache_[dna_hash] = {seed, current_frame, 1};
        }
    }
    
    stats_.total_predictions += unique_seeds.size();
    return unique_seeds;
}

void SeedPredictor::processSeeds(const std::vector<Seed>& seeds, uint64_t current_frame) {
    for (const auto& seed : seeds) {
        uint64_t dna_hash = seed.dna.hash();
        
        // Verifica se já está no cache do recompiler
        // Nota: PipelineCache usa hash VS+FS, não ShaderDNA. 
        // Aqui fazemos lookup semântico via seed_cache_.
        auto cache_it = seed_cache_.find(dna_hash);
        bool was_cached = (cache_it != seed_cache_.end() && cache_it->second.hit_count > 0);
        
        if (was_cached) {
            stats_.cache_hits++;
            cache_it->second.hit_count++;
        } else {
            stats_.cache_misses++;
            
            // Agenda compilação em background se habilitado
            if (config_.enable_background_compile && 
                compile_queue_.size() < config_.max_background_compiles_per_frame * 4) {
                compile_queue_.push_back({seed, current_frame});
            }
        }
    }
    
    // Processa fila de compilação
    processCompileQueue(current_frame);
}

bool SeedPredictor::prepareSeedImmediate(const Seed& seed) {
    if (!recompiler_) return false;
    
    // Tenta encontrar/criar pipeline correspondente ao DNA
    // Como o recompiler usa hash de bytecode, não DNA, 
    // isso é um placeholder para integração real.
    // Na prática, precisaria mapear DNA → bytecode do jogo.
    
    stats_.background_compiles++;
    return true;
}

void SeedPredictor::onShaderUsed(const ShaderDNA& dna, bool was_cached) {
    uint64_t dna_hash = dna.hash();
    auto it = seed_cache_.find(dna_hash);
    if (it != seed_cache_.end()) {
        if (was_cached) {
            it->second.hit_count++;
        }
    }
}

std::vector<Seed> SeedPredictor::predictFromCamera(uint64_t frame) {
    std::vector<Seed> seeds;
    if (!camera_query_) return seeds;
    
    // Pega tiles visíveis da camera query
    auto visible_tiles = camera_query_->getVisibleTiles();
    
    // Para cada tile visível, prevê shaders prováveis
    for (uint32_t tile_id : visible_tiles) {
        // Consulta polígonos no tile via Mental Map
        // Placeholder: na implementação real, consultaria RegionPolygonCache
        
        // Exemplo: tile com água prevê shader de água
        // Isso viria de metadata do mundo/região
    }
    
    return seeds;
}

std::vector<Seed> SeedPredictor::predictFromMarioMovement(uint64_t frame) {
    std::vector<Seed> seeds;
    
    // Calcula posição futura baseada na velocidade
    float future_x = mario_state_.x + mario_state_.vx * config_.prediction_horizon_frames;
    float future_z = mario_state_.z + mario_state_.vz * config_.prediction_horizon_frames;
    
    // Estima região futura
    // Placeholder: usaria ChunkManager::worldToRegionId
    
    // Se movendo rápido para uma direção, prevê shaders daquela região
    float speed = std::sqrt(mario_state_.vx * mario_state_.vx + mario_state_.vz * mario_state_.vz);
    if (speed > 1.0f) {  // Movimento significativo
        // Prevê transição para nova área
        // Se direção aponta para água, prevê shader de água
    }
    
    return seeds;
}

std::vector<Seed> SeedPredictor::predictFromAreaTransition(uint64_t frame) {
    std::vector<Seed> seeds;
    
    // Transição de área detectada
    // Prevê shaders típicos da nova área
    // Ex: área de água → shaders de água, reflexão, transparência
    // Ex: área de grama → shaders de foliage, wind, LOD
    
    // Placeholder: mapearia area_id → lista de ShaderDNA típicos
    static const std::unordered_map<uint32_t, std::vector<ShaderDNA>> area_shaders = {
        // area_id → shaders típicos
    };
    
    auto it = area_shaders.find(mario_state_.current_area);
    if (it != area_shaders.end()) {
        for (const auto& dna : it->second) {
            Seed seed;
            seed.dna = dna;
            seed.expected_visibility = 0.9f;
            seed.priority = 3; // HIGH
            seed.object_id = 0;
            seed.region_id = mario_state_.current_area;
            seed.debug_context = "Area transition to " + std::to_string(mario_state_.current_area);
            seeds.push_back(seed);
        }
    }
    
    return seeds;
}

std::vector<Seed> SeedPredictor::predictFromVisibleObjects(uint64_t frame) {
    std::vector<Seed> seeds;
    
    // Baseado nos objetos já visíveis, prevê variantes (LOD, pass types)
    // Placeholder: iteraria sobre visible_object_ids do GameStateSnapshot
    
    return seeds;
}

ShaderDNA SeedPredictor::buildShaderDNA(uint32_t material_id, uint32_t lighting_id,
                                         uint32_t geometry_class, uint32_t camera_class,
                                         uint32_t render_state, uint32_t lod_level,
                                         uint32_t pass_type) const {
    ShaderDNA dna;
    dna.material_id = material_id;
    dna.lighting_id = lighting_id;
    dna.geometry_class = geometry_class;
    dna.camera_class = camera_class;
    dna.render_state = render_state;
    dna.lod_level = lod_level;
    dna.pass_type = pass_type;
    return dna;
}

float SeedPredictor::estimateVisibility(const ShaderDNA& dna, const core::Vec3& camera_pos) {
    // Heurística simples: prioriza shaders de objetos próximos à câmera
    // Na prática, usaria distância do objeto à câmera + frustum culling
    return 0.5f; // placeholder
}

uint32_t SeedPredictor::classifyGeometry(uint32_t object_id) const {
    // Classifica objeto em: 0=surface, 1=foliage, 2=water, 3=transparent, 4=sky, 5=character
    // Placeholder: consultaria AssetRegistry/MaterialDB
    return 0;
}

uint32_t SeedPredictor::classifyCamera(const core::Vec3& cam_pos, const core::Vec3& cam_dir) const {
    // Classifica câmera: 0=close (player), 1=mid, 2=far, 3=ortho (mapa)
    return 0;
}

uint32_t SeedPredictor::classifyRenderState(uint32_t material_id, uint32_t geometry_class) const {
    // Combina: depth_test | blend_mode | cull_mode | stencil
    uint32_t state = 0;
    state |= 1; // depth test on
    if (geometry_class == 2) state |= 2; // blend on para água
    if (geometry_class == 3) state |= 4; // blend on para transparente
    return state;
}

void SeedPredictor::processCompileQueue(uint64_t current_frame) {
    if (!config_.enable_background_compile || compile_queue_.empty()) return;
    
    uint32_t processed = 0;
    auto it = compile_queue_.begin();
    while (it != compile_queue_.end() && processed < config_.max_background_compiles_per_frame) {
        const auto& job = *it;
        
        // Tenta compilar/preparar o shader
        if (prepareSeedImmediate(job.seed)) {
            stats_.background_compiles++;
            it = compile_queue_.erase(it);
            processed++;
        } else {
            ++it;
        }
    }
}

bool SeedPredictor::dumpSeedsToFile(const std::string& path, uint64_t frame) {
    std::ofstream out(path, std::ios::app);
    if (!out) return false;
    
    out << "=== Frame " << frame << " ===\n";
    out << "Total predictions: " << stats_.total_predictions << "\n";
    out << "Cache hits: " << stats_.cache_hits << "\n";
    out << "Cache misses: " << stats_.cache_misses << "\n";
    out << "Hit rate: " << (stats_.hit_rate() * 100.0) << "%\n";
    out << "Background compiles: " << stats_.background_compiles << "\n";
    out << "Compile queue size: " << compile_queue_.size() << "\n";
    out << "Seed cache size: " << seed_cache_.size() << "\n\n";
    
    return true;
}

} // namespace gpu
} // namespace mgd