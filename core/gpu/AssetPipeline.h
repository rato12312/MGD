#pragma once

// Asset Pipeline Integration — conecta Scanner/Infector/AssetRegistry ao Vulkan
// Carrega assets (mesh, texture, material) do jogo para GPU

#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include <cstdint>
#include <optional>

#include "core/scanner/AssetRegistry.h"
#include "core/scanner/normalize/Infector.h"
#include "core/scanner/normalize/MaterialRecord.h"
#include "core/scanner/normalize/TextureRecord.h"
#include "core/scanner/normalize/MeshRecord.h"
#include "VulkanContext.h"
#include "core/common/Types.h"
#include "core/common/AABB.h"
#include "core/common/Vec4.h"

namespace mgd {
namespace gpu {

// GPU resource wrappers
struct GpuMesh {
    std::unique_ptr<VulkanBuffer> vertex_buffer;
    std::unique_ptr<VulkanBuffer> index_buffer;
    uint32_t index_count = 0;
    VkIndexType index_type = VK_INDEX_TYPE_UINT16;
    AABB bounds{};
};

struct GpuTexture {
    std::unique_ptr<VulkanImage> image;
    std::unique_ptr<VulkanSampler> sampler;
    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
    uint32_t width = 0, height = 0;
    uint32_t mip_levels = 1;
};

struct GpuMaterial {
    uint32_t base_color_texture = UINT32_MAX;
    uint32_t normal_texture = UINT32_MAX;
    uint32_t roughness_texture = UINT32_MAX;
    uint32_t metallic_texture = UINT32_MAX;
    Vec4 base_color_factor{1,1,1,1};
    float roughness_factor = 1.0f;
    float metallic_factor = 1.0f;
    AlphaMode alpha_mode = AlphaMode::OPAQUE;
    float alpha_cutoff = 0.5f;
};

// Pipeline de asset: Scanner -> Infector -> AssetRegistry -> VulkanUploader
class AssetPipeline {
public:
    AssetPipeline() = default;
    ~AssetPipeline() = default;

    bool init(VulkanContext* ctx, core::AssetRegistry* registry, core::Infector* infector);
    void shutdown();

    // Upload de assets descobertos pelo Scanner
    bool uploadAllAssets();

    // Upload individual
    uint32_t uploadMesh(const core::MeshRecord& mesh);
    uint32_t uploadTexture(const core::TextureRecord& tex);
    uint32_t uploadMaterial(const core::MaterialRecord& mat);

    // Acesso aos recursos GPU
    const GpuMesh* getMesh(uint32_t id) const { return id < meshes_.size() ? &meshes_[id] : nullptr; }
    const GpuTexture* getTexture(uint32_t id) const { return id < textures_.size() ? &textures_[id] : nullptr; }
    const GpuMaterial* getMaterial(uint32_t id) const { return id < materials_.size() ? &materials_[id] : nullptr; }

    size_t meshCount() const { return meshes_.size(); }
    size_t textureCount() const { return textures_.size(); }
    size_t materialCount() const { return materials_.size(); }

    // Stats
    size_t totalVertexMemory() const { return total_vertex_mem_; }
    size_t totalIndexMemory() const { return total_index_mem_; }
    size_t totalTextureMemory() const { return total_texture_mem_; }

private:
    bool uploadMeshData(const core::MeshRecord& mesh, GpuMesh& out);
    bool uploadTextureData(const core::TextureRecord& tex, GpuTexture& out);
    void createDefaultSampler();

    VulkanContext* ctx_ = nullptr;
    core::AssetRegistry* registry_ = nullptr;
    core::Infector* infector_ = nullptr;

    std::vector<GpuMesh> meshes_;
    std::vector<GpuTexture> textures_;
    std::vector<GpuMaterial> materials_;
    std::unique_ptr<VulkanSampler> default_sampler_;

    size_t total_vertex_mem_ = 0;
    size_t total_index_mem_ = 0;
    size_t total_texture_mem_ = 0;
};

} // namespace gpu
} // namespace mgd