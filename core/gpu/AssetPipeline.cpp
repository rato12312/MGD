// Asset Pipeline Integration — upload de assets para Vulkan

#include "AssetPipeline.h"
#include "VulkanContext.h"
#include <cstring>
#include <algorithm>

namespace mgd {
namespace gpu {

bool AssetPipeline::init(VulkanContext* ctx, core::AssetRegistry* registry, core::Infector* infector) {
    ctx_ = ctx;
    registry_ = registry;
    infector_ = infector;
    createDefaultSampler();
    return true;
}

void AssetPipeline::shutdown() {
    for (auto& m : meshes_) {
        if (m.vertex_buffer) m.vertex_buffer.reset();
        if (m.index_buffer) m.index_buffer.reset();
    }
    for (auto& t : textures_) {
        if (t.image) t.image.reset();
        if (t.sampler) t.sampler.reset();
    }
    default_sampler_.reset();
}

bool AssetPipeline::uploadAllAssets() {
    if (!registry_) return false;
    
    // Upload meshes
    for (const auto& [id, mesh] : registry_->meshes()) {
        uploadMesh(mesh);
    }
    
    // Upload textures
    for (const auto& [id, tex] : registry_->textures()) {
        uploadTexture(tex);
    }
    
    // Upload materials
    for (const auto& [id, mat] : registry_->materials()) {
        uploadMaterial(mat);
    }
    
    return true;
}

uint32_t AssetPipeline::uploadMesh(const core::MeshRecord& mesh) {
    GpuMesh gpu_mesh;
    if (!uploadMeshData(mesh, gpu_mesh)) return UINT32_MAX;
    
    uint32_t id = static_cast<uint32_t>(meshes_.size());
    meshes_.push_back(std::move(gpu_mesh));
    return id;
}

uint32_t AssetPipeline::uploadTexture(const core::TextureRecord& tex) {
    GpuTexture gpu_tex;
    if (!uploadTextureData(tex, gpu_tex)) return UINT32_MAX;
    
    uint32_t id = static_cast<uint32_t>(textures_.size());
    textures_.push_back(std::move(gpu_tex));
    return id;
}

uint32_t AssetPipeline::uploadMaterial(const core::MaterialRecord& mat) {
    GpuMaterial gpu_mat;
    gpu_mat.base_color_factor = mat.base_color_factor;
    gpu_mat.roughness_factor = mat.roughness_factor;
    gpu_mat.metallic_factor = mat.metallic_factor;
    gpu_mat.alpha_mode = mat.alpha_mode;
    gpu_mat.alpha_cutoff = mat.alpha_cutoff;
    
    // Resolve texture IDs
    if (mat.base_color_texture != INVALID_TEXTURE_ID && mat.base_color_texture < textures_.size())
        gpu_mat.base_color_texture = mat.base_color_texture;
    if (mat.normal_texture != INVALID_TEXTURE_ID && mat.normal_texture < textures_.size())
        gpu_mat.normal_texture = mat.normal_texture;
    if (mat.roughness_texture != INVALID_TEXTURE_ID && mat.roughness_texture < textures_.size())
        gpu_mat.roughness_texture = mat.roughness_texture;
    if (mat.metallic_texture != INVALID_TEXTURE_ID && mat.metallic_texture < textures_.size())
        gpu_mat.metallic_texture = mat.metallic_texture;
    
    uint32_t id = static_cast<uint32_t>(materials_.size());
    materials_.push_back(gpu_mat);
    return id;
}

bool AssetPipeline::uploadMeshData(const core::MeshRecord& mesh, GpuMesh& out) {
    if (!ctx_ || mesh.positions.empty()) return false;
    
    // Vertex buffer: positions + normals + uvs + tangents
    struct Vertex {
        Vec3 pos;
        Vec3 normal;
        Vec2 uv;
        Vec4 tangent;
    };
    
    std::vector<Vertex> vertices;
    vertices.reserve(mesh.positions.size());
    for (size_t i = 0; i < mesh.positions.size(); ++i) {
        Vertex v;
        v.pos = mesh.positions[i];
        v.normal = i < mesh.normals.size() ? mesh.normals[i] : Vec3(0,1,0);
        v.uv = i < mesh.uvs.size() ? mesh.uvs[i] : Vec2(0,0);
        v.tangent = i < mesh.tangents.size() ? mesh.tangents[i] : Vec4(1,0,0,1);
        vertices.push_back(v);
    }
    
    // Create vertex buffer
    size_t vb_size = vertices.size() * sizeof(Vertex);
    auto vb = ctx_->createBuffer(vb_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (!vb) return false;
    
    // Staging buffer
    auto staging = ctx_->createBuffer(vb_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (!staging) return false;
    std::memcpy(staging->mapped, vertices.data(), vb_size);
    
    // Copy staging -> device
    VkCommandBuffer cb = ctx_->beginSingleTimeCommands();
    VkBufferCopy copy{}; copy.size = vb_size;
    vkCmdCopyBuffer(cb, staging->buffer, vb->buffer, 1, &copy);
    ctx_->endSingleTimeCommands(cb);
    
    out.vertex_buffer = std::move(vb);
    
    // Index buffer
    if (!mesh.indices.empty()) {
        size_t ib_size = mesh.indices.size() * sizeof(uint16_t);
        auto ib = ctx_->createBuffer(ib_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (!ib) return false;
        
        auto ib_staging = ctx_->createBuffer(ib_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (!ib_staging) return false;
        std::memcpy(ib_staging->mapped, mesh.indices.data(), ib_size);
        
        VkCommandBuffer cb2 = ctx_->beginSingleTimeCommands();
        VkBufferCopy copy2{}; copy2.size = ib_size;
        vkCmdCopyBuffer(cb2, ib_staging->buffer, ib->buffer, 1, &copy2);
        ctx_->endSingleTimeCommands(cb2);
        
        out.index_buffer = std::move(ib);
        out.index_count = static_cast<uint32_t>(mesh.indices.size());
        out.index_type = VK_INDEX_TYPE_UINT16;
    }
    
    // Compute bounds
    out.bounds = AABB::invalid();
    for (const auto& p : mesh.positions) {
        out.bounds = out.bounds.expanded(p);
    }
    
    total_vertex_mem_ += vertices.size() * sizeof(Vertex);
    if (out.index_buffer) total_index_mem_ += out.index_count * sizeof(uint16_t);
    
    return true;
}

bool AssetPipeline::uploadTextureData(const core::TextureRecord& tex, GpuTexture& out) {
    if (!ctx_ || tex.data.empty()) return false;
    
    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
    if (tex.format == "R8") format = VK_FORMAT_R8_UNORM;
    else if (tex.format == "RG8") format = VK_FORMAT_R8G8_UNORM;
    else if (tex.format == "RGBA16F") format = VK_FORMAT_R16G16B16A16_SFLOAT;
    
    out.width = tex.width;
    out.height = tex.height;
    out.format = format;
    
    // Create image
    auto img = ctx_->createImage(tex.width, tex.height, format,
                                 VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                 1); // mip_levels = 1 for now
    if (!img) return false;
    
    // Staging buffer for texture data
    size_t data_size = tex.data.size();
    auto staging = ctx_->createBuffer(data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (!staging) return false;
    std::memcpy(staging->mapped, tex.data.data(), data_size);
    
    // Transition image layout and copy
    VkCommandBuffer cb = ctx_->beginSingleTimeCommands();
    
    // Transition to TRANSFER_DST
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = img->image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
    
    // Copy buffer to image
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {static_cast<uint32_t>(img->extent.width), static_cast<uint32_t>(img->extent.height), 1};
    vkCmdCopyBufferToImage(cb, staging->buffer, img->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    
    // Transition to SHADER_READ_ONLY
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
    
    ctx_->endSingleTimeCommands(cb);
    
    // Create sampler
    out.image = std::move(img);
    out.sampler = default_sampler_;
    
    total_texture_mem_ += out.width * out.height * 4; // approximate
    
    return true;
}

void AssetPipeline::createDefaultSampler() {
    if (!ctx_) return;
    default_sampler_ = ctx_->createSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);
}

} // namespace gpu
} // namespace mgd