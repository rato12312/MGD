#pragma once

// Frustum Culling Compute Shader para Mali GPU
// Offload do Mental Map para GPU: frustum culling + LOD selection + occlusion culling

#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>
#include <glm/glm.hpp>

namespace mgd {
namespace gpu {

// Push constants para o compute shader de frustum culling
struct FrustumCullPushConstants {
    // Frustum planes (6 planes * 4 floats = 64 bytes)
    glm::vec4 frustum_planes[6];
    
    // Camera info
    glm::vec3 camera_pos;
    float camera_fov_y;
    float aspect_ratio;
    float near_plane;
    float far_plane;
    
    // LOD distances
    float lod_distances[3]; // LOD0->1, LOD1->2, LOD2->3
    
    // Output buffer info
    uint32_t instance_count;
    uint32_t max_visible_instances;
    
    // Padding para alinhamento 16 bytes
    float padding[3];
    
    // Total: 6*16 + 4*4 + 4*4 + 4 + 4*3 + 4*3 = 96 + 16 + 16 + 4 + 12 + 12 = 144 bytes (alinhado a 16)
};

// Instance data para culling (GPU buffer)
struct GpuMeshInstance {
    glm::mat4 transform;        // 64 bytes
    glm::vec3 bounds_center;    // 16 bytes
    float bounds_radius;        // 4 bytes
    uint32_t mesh_id;           // 4 bytes
    uint32_t material_id;       // 4 bytes
    uint32_t flags;             // 4 bytes
    uint32_t lod_levels[3];     // 12 bytes (LOD0, LOD1, LOD2 mesh IDs)
    uint32_t flags2;            // 4 bytes (padding + flags extras)
    // Total: 108 bytes -> alinhado a 16 = 112 bytes
};

// Output do culling
struct CullingOutput {
    uint32_t visible_count;
    uint32_t instance_indices[1024]; // Max 1024 instâncias visíveis por dispatch
    uint32_t lod_levels[1024];       // LOD level per instance
    uint32_t draw_counts[4];         // Draw count per LOD level
};

// Compute shader push constants layout (deve coincidir com shader)
struct FrustumCullPushConstants {
    alignas(16) glm::vec4 frustum_planes[6];
    alignas(16) glm::vec3 camera_pos;
    float camera_fov_y;
    float aspect_ratio;
    float near_plane;
    float far_plane;
    float lod_distances[3];
    uint32_t instance_count;
    uint32_t max_visible;
    uint32_t padding[2];
};

// Frustum culling pipeline
class FrustumCullingCompute {
public:
    FrustumCullingCompute() = default;
    ~FrustumCullingCompute() { shutdown(); }

    bool init(VulkanContext* ctx, FramebufferManager* fb_mgr);
    void shutdown();

    // Executa frustum culling + LOD selection
    bool execute(VkCommandBuffer cmd,
                 VkBuffer instance_buffer,     // Input: GpuMeshInstance[]
                 uint32_t instance_count,
                 VkBuffer visible_output,      // Output: visible instance indices
                 VkBuffer lod_output,          // Output: LOD level per instance
                 VkBuffer draw_count_output,   // Output: draw count per LOD
                 const FrustumCullPushConstants& push);

    // Atualiza push constants com nova camera
    void updatePushConstants(const FrustumCullPushConstants& pc);

    VkPipelineLayout pipelineLayout() const { return pipeline_layout_; }
    VkPipeline pipeline() const { return pipeline_; }

private:
    bool createPipeline(VulkanContext* ctx);
    bool createDescriptorSets(VulkanContext* ctx);
    bool createShaders(VulkanContext* ctx);
    bool createBuffers(VulkanContext* ctx, uint32_t max_instances);

    VulkanContext* ctx_ = nullptr;
    VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout desc_layout_ = VK_NULL_HANDLE;
    VkDescriptorPool desc_pool_ = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> desc_sets_;
    VkShaderModule cs_module_ = VK_NULL_HANDLE;
    
    // Buffers para culling
    std::unique_ptr<class GpuBuffer> instance_buffer_;
    std::unique_ptr<class GpuBuffer> visible_buffer_;
    std::unique_ptr<class GpuBuffer> lod_buffer_;
    std::unique_ptr<class GpuBuffer> draw_count_buffer_;
    std::unique_ptr<class GpuBuffer> indirect_draw_buffer_;
    
    bool createShaders(VulkanContext* ctx);
    bool createPipeline(VulkanContext* ctx);
    bool createDescriptorSets(VulkanContext* ctx);
    bool createBuffers(VulkanContext* ctx);
};

} // namespace gpu
} // namespace mgd