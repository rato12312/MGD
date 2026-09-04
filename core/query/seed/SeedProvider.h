#pragma once

#include "../../common/Vec3.h"
#include "../../common/Types.h"
#include "../../mental_map/ChunkManager.h"
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mgd {
namespace seed {

// Seed Provider — camada de previsão e preparação (não substitui jogo nem Mapa Mental).
// Entrada: seed + estado compacto + ação do jogador.
// Saída: regiões prováveis ordenadas por probabilidade, para pré-carregar
// (DNA, pixel map, cache) antes de entrarem na visão.
// Base honesta: heurística determinística por direção da câmera + cache de
// previsões. Errou -> descarta; acertou -> dados já preparados.
enum class PlayerAction : uint8_t {
    Idle = 0,
    MoveForward = 1,
    TurnLeft = 2,
    TurnRight = 3,
    Jump = 4,
    EnterArea = 5,
    Interact = 6
};

// Estado compacto do mundo (referências, sem copiar o jogo).
struct WorldState {
    Vec3 player_pos{};
    Vec3 camera_forward{0.0f, 0.0f, 1.0f};
    RegionID current_region = INVALID_REGION_ID;
    PlayerAction last_action = PlayerAction::Idle;

    uint64_t hash(uint64_t seed) const {
        uint64_t h = seed ^ 0x9E3779B97F4A7C15ull;
        auto mix = [&h](uint64_t v) {
            h ^= v + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2);
        };
        mix(static_cast<uint64_t>(current_region));
        mix(static_cast<uint64_t>(static_cast<int>(player_pos.x)));
        mix(static_cast<uint64_t>(static_cast<int>(player_pos.z)));
        // direção da câmera quantizada em 8 setores
        int yaw = 0;
        if (camera_forward.x != 0.0f || camera_forward.z != 0.0f) {
            float ang = camera_forward.z >= 0 ? camera_forward.x : -camera_forward.x;
            yaw = static_cast<int>(ang * 4.0f);
        }
        mix(static_cast<uint64_t>(yaw & 0xFF));
        mix(static_cast<uint64_t>(last_action));
        return h;
    }
};

struct RegionPrediction {
    RegionID region = INVALID_REGION_ID;
    float probability = 0.0f; // 0..1; abaixo de threshold o MGD ignora
};

struct PredictionKey {
    uint64_t state_hash = 0;
    PlayerAction action = PlayerAction::Idle;
    bool operator==(const PredictionKey& o) const {
        return state_hash == o.state_hash && action == o.action;
    }
};

struct PredictionKeyHash {
    size_t operator()(const PredictionKey& k) const noexcept {
        return static_cast<size_t>(k.state_hash ^ (static_cast<uint64_t>(k.action) * 0x9E3779B9ull));
    }
};

class SeedProvider {
public:
    explicit SeedProvider(uint64_t seed = 847291ull) : seed_(seed) {}

    void setSeed(uint64_t s) { seed_ = s; clear(); }
    uint64_t seed() const { return seed_; }

    // Prevê regiões prováveis para (estado, ação). Determinístico.
    // Usa cache de previsões: repetição da mesma situação = HIT.
    std::vector<RegionPrediction> predict(const WorldState& state, PlayerAction action) {
        uint64_t sh = state.hash(seed_);
        PredictionKey key{sh, action};
        auto it = prediction_cache_.find(key);
        if (it != prediction_cache_.end()) {
            hits_++;
            return it->second;
        }
        misses_++;
        auto out = computePrediction(state, action);
        prediction_cache_[key] = out;
        return out;
    }

    // Probabilidades abaixo disso são ignoradas pelo MGD (não prepara).
    void setThreshold(float t) { threshold_ = t; }
    float threshold() const { return threshold_; }

    void clear() { prediction_cache_.clear(); hits_ = misses_ = 0; }
    uint64_t hits() const { return hits_; }
    uint64_t misses() const { return misses_; }
    size_t cacheSize() const { return prediction_cache_.size(); }

private:
    // Heurística base documentada: região atual (0.9) + 1-2 chunks à frente
    // na direção da câmera (0.6/0.3), com desvio lateral em TurnLeft/Right.
    // Tunável; o ganho real se mede no benchmark, não aqui.
    std::vector<RegionPrediction> computePrediction(const WorldState& state, PlayerAction action) const {
        std::vector<RegionPrediction> out;
        RegionID cur = state.current_region != INVALID_REGION_ID
            ? state.current_region
            : ChunkManager::worldToRegionId(state.player_pos);
        out.push_back({cur, 0.9f});

        Vec3 fwd = state.camera_forward;
        float fl = fwd.x * fwd.x + fwd.z * fwd.z;
        if (fl < 1e-8f) return out;
        Vec3 n = Vec3{fwd.x, 0.0f, fwd.z}.normalized();
        float fx = n.x, fz = n.z;

        float lateral = 0.0f;
        if (action == PlayerAction::TurnLeft) lateral = -1.0f;
        else if (action == PlayerAction::TurnRight) lateral = 1.0f;
        int steps = (action == PlayerAction::Idle || action == PlayerAction::Interact) ? 1 : 2;
        // direita da câmera no plano XZ: (-fz, fx)
        for (int i = 1; i <= steps; ++i) {
            float d = static_cast<float>(i) * ChunkManager::CHUNK_SIZE;
            Vec3 ahead{
                state.player_pos.x + fx * d + (-fz) * lateral * d * 0.5f,
                0.0f,
                state.player_pos.z + fz * d + (fx) * lateral * d * 0.5f};
            RegionID r = ChunkManager::worldToRegionId(ahead);
            if (r == cur) continue;
            bool dup = false;
            for (auto& p : out) if (p.region == r) { dup = true; break; }
            if (dup) continue;
            out.push_back({r, i == 1 ? 0.6f : 0.3f});
        }
        return out;
    }

    uint64_t seed_;
    float threshold_ = 0.05f;
    std::unordered_map<PredictionKey, std::vector<RegionPrediction>, PredictionKeyHash> prediction_cache_;
    mutable uint64_t hits_ = 0;
    mutable uint64_t misses_ = 0;
};

} // namespace seed
} // namespace mgd
