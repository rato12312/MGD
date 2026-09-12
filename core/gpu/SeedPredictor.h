#pragma once

// Seed Predictor — antecipa shaders/PSOs necessários baseando-se no estado do jogo
// Conecta: Game State (posição Mario, câmera, área) → Shader DNA → PipelineCache

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <string>
#include <chrono>
#include <functional>
#include <array>

#include "VulkanBackend.h"
#include "ShaderRecompiler.h"
#include "core/query/CameraMentalMapQuery.h"
#include "core/bridge/MentalMapRuntime.h"

namespace mgd {
namespace gpu {

// Shader DNA — identidade semântica do shader (mais rica que hash de bytecode)
struct ShaderDNA {
    uint32_t material_id = 0;          // Material do objeto (água, grama, metal, etc)
    uint32_t lighting_id = 0;          // Setup de iluminação (L1, L2, L3...)
    uint32_t geometry_class = 0;       // Classe geométrica (surface, foliage, water, transparent...)
    uint32_t camera_class = 0;         // Classe de câmera (close, mid, far, ortho...)
    uint32_t render_state = 0;         // Estado de render (depth, blend, cull, stencil...)
    uint32_t lod_level = 0;            // LOD level (0=high, 1=mid, 2=low)
    uint32_t pass_type = 0;            // Tipo de pass (base, shadow, depth_prepass, motion_vectors...)
    
    bool operator==(const ShaderDNA& other) const {
        return material_id == other.material_id &&
               lighting_id == other.lighting_id &&
               geometry_class == other.geometry_class &&
               camera_class == other.camera_class &&
               render_state == other.render_state &&
               lod_level == other.lod_level &&
               pass_type == other.pass_type;
    }
    
    uint64_t hash() const {
        // FNV-1a hash combiner
        uint64_t h = 14695981039346656037ull;
        auto mix = [](uint64_t h, uint64_t v) {
            h ^= v;
            h *= 1099511628211ull;
            return h;
        };
        h = mix(h, material_id);
        h = mix(h, (uint64_t)lighting_id << 8);
        h = mix(h, (uint64_t)geometry_class << 16);
        h = mix(h, (uint64_t)camera_class << 24);
        h = mix(h, (uint64_t)render_state << 32);
        h = mix(h, (uint64_t)lod_level << 40);
        h = mix(h, (uint64_t)pass_type << 48);
        return h;
    }
};

// Seed — intenção visual estruturada emitida pelo predictor
struct Seed {
    ShaderDNA dna;
    float expected_visibility = 0.0f;  // 0.0 - 1.0: probabilidade de ser necessário
    uint32_t priority = 0;             // HIGH=3, MEDIUM=2, LOW=1, BACKGROUND=0
    uint64_t frame_predicted = 0;      // Frame em que foi prevista
    uint32_t object_id = 0;            // ID do objeto que gerou a previsão (se aplicável)
    uint32_t region_id = 0;            // Região do mundo (para invalidação espacial)
    
    // Metadados para debug/replay
    std::string debug_context;         // Ex: "Mario approaching water area A+1"
};

// Métricas do predictor
struct SeedPredictorStats {
    uint64_t total_predictions = 0;
    uint64_t cache_hits = 0;           // Previsão acertou: shader já estava no cache
    uint64_t cache_misses = 0;         // Previsão errou: shader não estava no cache
    uint64_t background_compiles = 0;  // Compilações feitas em background
    uint64_t prep_time_us = 0;         // Tempo gasto preparando shaders previstos
    double hit_rate() const {
        return (cache_hits + cache_misses) > 0 ? 
            double(cache_hits) / double(cache_hits + cache_misses) : 0.0;
    }
    double precision() const {
        return total_predictions > 0 ? double(cache_hits) / double(total_predictions) : 0.0;
    }
};

// Configuração do predictor
struct SeedPredictorConfig {
    uint32_t max_seeds_per_frame = 64;
    float min_visibility_threshold = 0.3f;
    uint32_t prediction_horizon_frames = 2;  // Quantos frames à frente prever
    bool enable_background_compile = true;
    bool enable_spatial_prediction = true;   // Baseado em região do mundo
    bool enable_temporal_prediction = true;  // Baseado em velocidade/movimento
    uint32_t max_background_compiles_per_frame = 4;
};

class SeedPredictor {
public:
    SeedPredictor() = default;
    ~SeedPredictor() = default;
    
    // Inicializa com referências aos sistemas necessários
    bool init(ShaderRecompiler* recompiler, 
              core::CameraMentalMapQuery* camera_query,
              core::MentalMapRuntime* mental_map);
    void shutdown();
    
    // Configuração
    void setConfig(const SeedPredictorConfig& cfg) { config_ = cfg; }
    const SeedPredictorConfig& getConfig() const { return config_; }
    
    // Atualiza estado do jogo (chamado uma vez por frame)
    void updateGameState(const GameStateSnapshot& state);
    
    // Gera seeds para o frame atual + horizonte de previsão
    std::vector<Seed> predictSeeds(uint64_t current_frame);
    
    // Processa seeds: verifica cache, agenda compilação background
    void processSeeds(const std::vector<Seed>& seeds, uint64_t current_frame);
    
    // Força preparação imediata de um seed (para quando predição falha)
    bool prepareSeedImmediate(const Seed& seed);
    
    // Estatísticas
    const SeedPredictorStats& getStats() const { return stats_; }
    void resetStats() { stats_ = {}; }
    
    // Debug: serializa seeds para arquivo
    bool dumpSeedsToFile(const std::string& path, uint64_t frame);
    
    // Callback quando shader é realmente usado (para treino/ajuste)
    void onShaderUsed(const ShaderDNA& dna, bool was_cached);

private:
    // Estado interno
    ShaderRecompiler* recompiler_ = nullptr;
    core::CameraMentalMapQuery* camera_query_ = nullptr;
    core::MentalMapRuntime* mental_map_ = nullptr;
    SeedPredictorConfig config_;
    SeedPredictorStats stats_;
    
    // Cache de seeds recentes (para deduplicação temporal)
    struct CachedSeed {
        Seed seed;
        uint64_t last_frame_seen = 0;
        uint32_t hit_count = 0;
    };
    std::unordered_map<uint64_t, CachedSeed> seed_cache_;  // key = ShaderDNA::hash()
    
    // Histórico de movimento do Mario (para previsão temporal)
    struct MarioState {
        float x = 0, y = 0, z = 0;
        float vx = 0, vy = 0, vz = 0;
        uint32_t current_area = 0;
        uint32_t previous_area = 0;
    } mario_state_;
    
    // Previsão por tipo
    std::vector<Seed> predictFromCamera(uint64_t frame);
    std::vector<Seed> predictFromMarioMovement(uint64_t frame);
    std::vector<Seed> predictFromAreaTransition(uint64_t frame);
    std::vector<Seed> predictFromVisibleObjects(uint64_t frame);
    
    // Helpers
    ShaderDNA buildShaderDNA(uint32_t material_id, uint32_t lighting_id,
                              uint32_t geometry_class, uint32_t camera_class,
                              uint32_t render_state, uint32_t lod_level,
                              uint32_t pass_type) const;
    float estimateVisibility(const ShaderDNA& dna, const core::Vec3& camera_pos);
    uint32_t classifyGeometry(uint32_t object_id) const;
    uint32_t classifyCamera(const core::Vec3& cam_pos, const core::Vec3& cam_dir) const;
    uint32_t classifyRenderState(uint32_t material_id, uint32_t geometry_class) const;
    
    // Background compilation queue
    struct CompileJob {
        Seed seed;
        uint64_t submit_frame;
    };
    std::vector<CompileJob> compile_queue_;
    void processCompileQueue(uint64_t current_frame);
};

// Snapshot do estado do jogo para o predictor
struct GameStateSnapshot {
    // Mario
    float mario_x = 0, mario_y = 0, mario_z = 0;
    float mario_vx = 0, mario_vy = 0, mario_vz = 0;
    uint32_t mario_area = 0;
    
    // Camera
    float cam_x = 0, cam_y = 0, cam_z = 0;
    float cam_pitch = 0, cam_yaw = 0, cam_roll = 0;
    float cam_fov = 60.0f;
    
    // Mundo
    uint64_t frame_index = 0;
    float delta_time = 0.0f;
    
    // Objetos visíveis (do Mental Map)
    std::vector<uint32_t> visible_object_ids;
    std::vector<uint32_t> visible_polygon_ids;
};

} // namespace gpu
} // namespace mgd