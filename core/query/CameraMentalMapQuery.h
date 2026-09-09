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
    float screen_area = 0.0f; // área projetada na tela
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
    void setScreenSize(int w, int h) { screen_w_ = w; screen_h_ = h; }

    // Query principal: executa frustum culling + Mental Map lookup
    CameraQueryResult query(uint64_t frame_index) {
        CameraQueryResult result;
        result.frame_index = frame_index;
        auto start = std::chrono::high_resolution_clock::now();

        if (!camera_ || !mental_map_ || !poly_cache_) return result;

        // 1. Frustum culling via BasicVisibility
        VisibleSet visible = visibility_ ? visibility_->compute(*mental_map_, *camera_, *collision_)
                                         : BasicVisibility().compute(*mental_map_, *camera_, *collision_);

        // 2. Regiões da câmera (center + prediction margin)
        Vec3 cam_pos = camera_->getState().position;
        Vec3 cam_vel = camera_->getState().velocity;
        RegionID center_region = ChunkManager::worldToRegionId(cam_pos);
        result.regions.push_back(center_region);
        addPredictionRegions(result.regions, center_region, cam_pos, cam_vel);

        // 3. Para cada entidade visível, resolve polígonos via RegionPolygonCache
        for (const auto& ve : visible.entities) {
            RegionID r = ChunkManager::worldToRegionId(ve.position);
            const auto& polys = poly_cache_->getPolygons(r);
            
            for (const auto& p : polys) {
                float dist = ve.distance_to_camera;
                if (dist > view_distance_) continue;

                int lod = calculateLOD(dist);
                int priority = calculateCenterPriority(ve.position);
                float screen_area = estimateScreenArea(ve.bounds, dist);

                VisiblePolygon vp;
                vp.id = p.polygon_id;
                vp.asset_id = p.asset_id;
                vp.position = p.position;
                vp.distance = dist;
                vp.lod = lod;
                vp.priority = priority;
                vp.screen_area = screen_area;
                vp.flags = p.flags;
                result.polygons.push_back(vp);
            }
        }

        // 4. Ordena por prioridade (centro primeiro) + screen area (maior primeiro) + distância
        std::sort(result.polygons.begin(), result.polygons.end(),
            [](const VisiblePolygon& a, const VisiblePolygon& b) {
                if (a.priority != b.priority) return a.priority < b.priority;
                if (a.screen_area != b.screen_area) return a.screen_area > b.screen_area;
                return a.distance < b.distance;
            });

        // 5. Predição de margem: polígonos das regiões na direção do movimento
        addPredictedPolygons(result, cam_pos, cam_vel);

        // 6. Center priority culling: limita polígonos de baixa prioridade se exceder budget
        applyCenterPriorityBudget(result);

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
    float center_priority_radius_ = 0.3f; // raio em NDC (0-1)
    float prediction_margin_ = 2.0f;
    bool use_lod_ = true;
    float lod_near_ = 20.0f;
    float lod_far_ = 60.0f;
    int screen_w_ = 512;
    int screen_h_ = 288;
    int max_polygons_per_frame_ = 200; // budget para rascunho

    int calculateLOD(float dist) const {
        if (!use_lod_) return 0;
        if (dist < lod_near_) return 0;
        if (dist > lod_far_) return 2;
        return 1;
    }

    int calculateCenterPriority(const Vec3& pos) const {
        if (!camera_) return 1000;
        const Frustum& frustum = camera_->getFrustum();
        // Projeta posição para NDC
        Vec4 clip = camera_->getViewProjectionMatrix() * Vec4(pos, 1.0f);
        if (clip.w <= 0) return 1000;
        float ndc_x = clip.x / clip.w;
        float ndc_y = clip.y / clip.w;
        float dist_center = sqrtf(ndc_x * ndc_x + ndc_y * ndc_y);
        if (dist_center <= center_priority_radius_) return 0; // centro absoluto
        if (dist_center <= center_priority_radius_ * 2.0f) return 1; // centro expandido
        if (dist_center <= center_priority_radius_ * 3.0f) return 2; // meio
        return 3; // bordas
    }

    float estimateScreenArea(const AABB& bounds, float dist) const {
        if (dist <= 0) return 0;
        // Aproximação: área projetada ~ (tamanho/dist)^2
        float size = bounds.max.x - bounds.min.x;
        size = std::max(size, bounds.max.y - bounds.min.y);
        size = std::max(size, bounds.max.z - bounds.min.z);
        return (size * size) / (dist * dist);
    }

    void addPredictionRegions(std::vector<RegionID>& regions, RegionID center, const Vec3& cam_pos, const Vec3& cam_vel) {
        // 3x3 grid ao redor do centro
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
        // Regiões extras na direção do movimento
        if (cam_vel.length() > 0.1f) {
            Vec3 pred = cam_pos + cam_vel * prediction_margin_;
            RegionID pred_r = ChunkManager::worldToRegionId(pred);
            if (pred_r != regions[0]) regions.push_back(pred_r);
            // Vizinhos da região predita
            int px = static_cast<int>(pred_r >> 16);
            int pz = static_cast<int>(pred_r & 0xFFFF);
            for (int dz = -1; dz <= 1; dz++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int nx = px + dx;
                    int nz = pz + dz;
                    RegionID r = static_cast<RegionID>((static_cast<uint32_t>(nx) << 16) | static_cast<uint32_t>(nz));
                    regions.push_back(r);
                }
            }
        }
    }

    void addPredictedPolygons(CameraQueryResult& result, const Vec3& cam_pos, const Vec3& cam_vel) {
        if (!poly_cache_ || cam_vel.length() < 0.1f) return;
        Vec3 pred_pos = cam_pos + cam_vel * prediction_margin_;
        RegionID pred_region = ChunkManager::worldToRegionId(pred_pos);
        const auto& preds = poly_cache_->getPolygons(pred_region);
        for (const auto& p : preds) {
            float dist = (p.position - cam_pos).length();
            if (dist > view_distance_) continue;
            VisiblePolygon vp;
            vp.id = p.polygon_id;
            vp.asset_id = p.asset_id;
            vp.position = p.position;
            vp.distance = dist;
            vp.lod = calculateLOD(dist);
            vp.priority = 4; // prioridade baixa (predito)
            vp.screen_area = estimateScreenArea(AABB{p.position - Vec3(1,1,1), p.position + Vec3(1,1,1)}, dist);
            vp.flags = p.flags;
            result.polygons.push_back(vp);
        }
    }

    void applyCenterPriorityBudget(CameraQueryResult& result) {
        if (static_cast<int>(result.polygons.size()) <= max_polygons_per_frame_) return;
        // Mantém: prioridade 0/1 (centro), prioridade 2/3 (meio/bordas) proporcionalmente
        std::vector<VisiblePolygon> kept;
        kept.reserve(max_polygons_per_frame_);
        int p0_quota = max_polygons_per_frame_ * 50 / 100;
        int p1_quota = max_polygons_per_frame_ * 30 / 100;
        int p2_quota = max_polygons_per_frame_ * 15 / 100;
        int p3_quota = max_polygons_per_frame_ * 5 / 100;
        for (const auto& p : result.polygons) {
            int quota = (p.priority == 0) ? p0_quota : (p.priority == 1) ? p1_quota : (p.priority == 2) ? p2_quota : p3_quota;
            if (quota > 0) {
                kept.push_back(p);
                if (p.priority == 0) p0_quota--;
                else if (p.priority == 1) p1_quota--;
                else if (p.priority == 2) p2_quota--;
                else p3_quota--;
            }
        }
        result.polygons.swap(kept);
    }
};

} // namespace core
} // namespace mgd