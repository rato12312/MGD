#pragma once

// Camera → Mental Map Query + Frustum Culling
// Conecta Camera → MentalMap → BasicVisibility → Rascunho Pipeline
// Fornece: visible polygons IDs + LOD + center priority

#include "core/bridge/EmulatorHandoff.h"
#include "core/visibility/BasicVisibility.h"
#include "core/visibility/IVisibilitySystem.h"
#include "core/mental_map/MentalMap.h"
#include "core/mental_map/ChunkManager.h"
#include "core/camera/Camera.h"
#include "core/collision/CollisionSystem.h"
#include "core/query/RegionPolygonCache.h"
#include "core/query/PolygonConsultant.h"
#include <vector>
#include <unordered_map>

namespace mgd {
namespace core {

// Resultado da query: polígonos visíveis com metadados para rascunho
struct VisiblePolygon {
    PolygonID id = INVALID_POLYGON_ID;
    AssetID asset_id = INVALID_ASSET_ID;
    Vec3 position{};
    float distance = 0.0f;
    int lod = 0; // 0=full, 1=medium, 2=low
    int priority = 0; // center priority (0=center, aumenta nas bordas)
    uint32_t flags = 0;
};

struct CameraQueryResult {
    std::vector<VisiblePolygon> polygons;
    std::vector<RegionID> regions;
    uint64_t frame_index = 0;
    float compute_time_ms = 0.0f;
};

// Query system: Camera + MentalMap + RegionPolygonCache + PolygonConsultant
class CameraMentalMapQuery {
public:
    CameraMentalMapQuery() = default;

    void setCamera(Camera* cam) { camera_ = cam; }
    void setMentalMap(MentalMap* map) { mental_map_ = map; }
    void setPolygonCache(RegionPolygonCache* cache) { poly_cache_ = cache; }
    void setPolygonConsultant(PolygonConsultant* consultant) { consultant_ = consultant; }
    void setCollisionSystem(CollisionSystem* collision) { collision_ = collision; }
    void setVisibilitySystem(IVisibilitySystem* vis) { visibility_ = vis; }

    // Configuração
    void setViewDistance(float d) { view_distance_ = d; }
    void setCenterPriorityRadius(float r) { center_priority_radius_ = r; }
    void setPredictionMargin(float m) { prediction_margin_ = m; }
    void enableLOD(bool on) { use_lod_ = on; }
    void setLODDistances(float near_d, float far_d) { lod_near_ = near_d; lod_far_ = far_d; }

    // Query principal: executa frustum culling + Mental Map lookup
    CameraQueryResult query(uint64_t frame_index) {
        CameraQueryResult result;
        result.frame_index = frame_index;
        auto start = std::chrono::high_resolution_clock::now();

        if (!camera_ || !mental_map_ || !poly_cache_) return result;

        // 1. Frustum culling via BasicVisibility (retorna entidades visíveis)
        VisibleSet visible = visibility_ ? visibility_->compute(*mental_map_, *camera_, *collision_)
                                         : BasicVisibility().compute(*mental_map_, *camera_, *collision_);

        // 2. Regiões da câmera (center + prediction margin)
        Vec3 cam_pos = camera_->getState().position;
        RegionID center_region = ChunkManager::worldToRegionId(cam_pos);
        result.regions.push_back(center_region);
        addPredictionRegions(result.regions, center_region, cam_pos);

        // 3. Para cada entidade visível, resolve polígonos via PolygonConsultant
        for (const auto& ve : visible.entities) {
            // Consulta polígonos na região da entidade
            RegionID r = ChunkManager::worldToRegionId(ve.position);
            const auto& polys = poly_cache_->getPolygons(r);
            
            for (const auto& p : polys) {
                // Filtro por distância + LOD
                float dist = ve.distance_to_camera;
                if (dist > view_distance_) continue;

                int lod = calculateLOD(dist);
                int priority = calculateCenterPriority(ve.position, cam_pos);

                VisiblePolygon vp;
                vp.id = p.polygon_id;
                vp.asset_id = p.asset_id;
                vp.position = p.position;
                vp.distance = dist;
                vp.lod = lod;
                vp.priority = priority;
                vp.flags = p.flags;
                result.polygons.push_back(vp);
            }
        }

        // 4. Ordena por prioridade (centro primeiro) + distância
        std::sort(result.polygons.begin(), result.polygons.end(),
            [](const VisiblePolygon& a, const VisiblePolygon& b) {
                if (a.priority != b.priority) return a.priority < b.priority;
                return a.distance < b.distance;
            });

        // 5. Predição de margem: adiciona polígonos das regiões vizinhas que podem entrar
        addPredictedPolygons(result, cam_pos);

        auto end = std::chrono::high_resolution_clock::now();
        result.compute_time_ms = std::chrono::duration<float, std::milli>(end - start).count();
        return result;
    }

private:
    Camera* camera_ = nullptr;
    MentalMap* mental_map_ = nullptr;
    RegionPolygonCache* poly_cache_ = nullptr;
    PolygonConsultant* consultant_ = nullptr;
    CollisionSystem* collision_ = nullptr;
    IVisibilitySystem* visibility_ = nullptr;

    float view_distance_ = 100.0f;
    float center_priority_radius_ = 0.3f; // 30% do centro da tela
    float prediction_margin_ = 2.0f; // metros
    bool use_lod_ = true;
    float lod_near_ = 20.0f;
    float lod_far_ = 60.0f;

    int calculateLOD(float dist) const {
        if (!use_lod_) return 0;
        if (dist < lod_near_) return 0;
        if (dist > lod_far_) return 2;
        return 1;
    }

    int calculateCenterPriority(const Vec3& pos, const Vec3& cam_pos) const {
        // Projeta posição no espaço de tela NDC
        const Frustum& frustum = camera_->getFrustum();
        // Simplificado: usa distância angular do centro
        Vec3 dir = (pos - cam_pos).normalized();
        Vec3 forward = camera_->getState().forward; // precisa expor
        float dot = dir.dot(forward);
        float angle = acosf(std::max(-1.0f, std::min(1.0f, dot)));
        float fov_half = camera_->getState().fov_degrees * 0.5f * M_PI / 180.0f;
        float normalized = angle / fov_half; // 0 = centro, 1 = borda
        return static_cast<int>(normalized * 100.0f);
    }

    void addPredictionRegions(std::vector<RegionID>& regions, RegionID center, const Vec3& cam_pos) {
        // Adiciona regiões na direção do movimento da câmera
        // Simplificado: 8 vizinhos (3x3)
        int cx = static_cast<int>(center >> 16);
        int cz = static_cast<int>(center & 0xFFFF);
        for (int dz = -1; dz <= 1; dz++) {
            for (int dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dz == 0) continue;
                int nx = cx + dx;
                int nz = cz + dz;
                RegionID r = static_cast<RegionID>((static_cast<uint32_t>(nx) << 16) | static_cast<uint32_t>(nz));
                regions.push_back(r);
            }
        }
    }

    void addPredictedPolygons(CameraQueryResult& result, const Vec3& cam_pos) {
        // Adiciona polígonos das regiões de predição
        if (!poly_cache_) return;
        Vec3 cam_vel = camera_->getState().velocity; // precisa expor
        if (cam_vel.length() < 0.01f) return;

        Vec3 pred_pos = cam_pos + cam_vel * prediction_margin_;
        RegionID pred_region = ChunkManager::worldToRegionId(pred_pos);
        const auto& preds = poly_cache_->getPolygons(pred_region);
        for (const auto& p : preds) {
            VisiblePolygon vp;
            vp.id = p.polygon_id;
            vp.asset_id = p.asset_id;
            vp.position = p.position;
            vp.distance = (p.position - cam_pos).length();
            vp.lod = calculateLOD(vp.distance);
            vp.priority = 1000; // baixa prioridade (predito)
            vp.flags = p.flags;
            result.polygons.push_back(vp);
        }
    }
};

} // namespace core
} // namespace mgd