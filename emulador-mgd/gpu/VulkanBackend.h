// GPU Vulkan Backend — backend real para execução Maxwell no host GPU
// Compila shaders Maxwell → SPIR-V, cria pipelines Vulkan, executa command buffers

#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include <optional>

// Vulkan headers (precisa do Vulkan SDK)
#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#elif __linux__
#define VK_USE_PLATFORM_XCB_KHR
#elif __ANDROID__
#define VK_USE_PLATFORM_ANDROID_KHR
#endif
#include <vulkan/vulkan.h>

#include "Gpu.h"
#include "core/gpu/FramebufferManager.h"
#include "core/gpu/PainterCompute.h"
#include "core/gpu/AssetPipeline.h"

namespace mgd {
namespace gpu {

// Forward declarations
class VulkanContext;
class ShaderRecompiler;
class PipelineCache;

// Maxwell shader IR (intermediate representation antes de SPIR-V)
struct MaxwellShaderIR {
    ShaderStage stage;
    std::vector<uint32_t> maxwell_bytecode;
    std::vector<uint32_t> spirv; // compilado
    std::string entry_point = "main";
    // Reflection info
    struct Binding {
        uint32_t set = 0;
        uint32_t binding = 0;
        VkDescriptorType type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        VkShaderStageFlags stages = 0;
        uint32_t count = 1;
    };
    std::vector<Binding> bindings;
    struct PushConstantRange {
        VkShaderStageFlags stages = 0;
        uint32_t offset = 0;
        uint32_t size = 0;
    };
    std::vector<PushConstantRange> push_constants;
};

// Vulkan resources wrappers
struct VulkanBuffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
    VkBufferUsageFlags usage = 0;
    VkMemoryPropertyFlags props = 0;
    void* mapped = nullptr;
};

struct VulkanImage {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkExtent3D extent{};
    uint32_t mip_levels = 1;
    uint32_t array_layers = 1;
};

struct VulkanSampler {
    VkSampler sampler = VK_NULL_HANDLE;
};

// Pipeline state compilado
struct CompiledPipeline {
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkDescriptorSetLayout desc_set_layout = VK_NULL_HANDLE;
    std::vector<VkDescriptorSetLayout> desc_set_layouts;
    std::vector<VkDescriptorPool> desc_pools;
    std::vector<VkDescriptorSet> desc_sets;
    ShaderRecompiler* recompiler = nullptr;
    MaxwellShaderIR vs_ir;
    MaxwellShaderIR fs_ir;
    MaxwellShaderIR cs_ir;
};

// ========== VULKAN CONTEXT ==========
class VulkanContext {
public:
    VulkanContext();
    ~VulkanContext();

    bool init(const char* app_name, void* window_handle = nullptr);
    void shutdown();

    // Frame synchronization
    bool beginFrame();
    void endFrame();
    void waitIdle();

    // Resource creation
    std::unique_ptr<VulkanBuffer> createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props);
    std::unique_ptr<VulkanImage> createImage(uint32_t w, uint32_t h, VkFormat format, VkImageUsageFlags usage, uint32_t mip_levels = 1);
    std::unique_ptr<VulkanSampler> createSampler(VkFilter min_filter, VkFilter mag_filter, VkSamplerAddressMode wrap_mode);

    // Command buffer management
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer cmd);

    void setResolution(uint32_t roughW, uint32_t roughH, uint32_t finalW, uint32_t finalH);

    // Getters
    VkInstance instance() const { return instance_; }
    VkPhysicalDevice physicalDevice() const { return physical_device_; }
    VkDevice device() const { return device_; }
    VkQueue graphicsQueue() const { return graphics_queue_; }
    VkQueue computeQueue() const { return compute_queue_; }
    VkQueue presentQueue() const { return present_queue_; }
    uint32_t graphicsQueueFamily() const { return graphics_queue_family_; }
    uint32_t computeQueueFamily() const { return compute_queue_family_; }
    VkSurfaceKHR surface() const { return surface_; }
    VkSwapchainKHR swapchain() const { return swapchain_; }
    VkFormat swapchainFormat() const { return swapchain_format_; }
    VkExtent2D swapchainExtent() const { return swapchain_extent_; }
    const std::vector<VkImageView>& swapchainImageViews() const { return swapchain_image_views_; }
    VkRenderPass renderPass() const { return render_pass_; }
    const std::vector<VkFramebuffer>& framebuffers() const { return framebuffers_; }
    uint32_t currentFrame() const { return current_frame_; }
    VkCommandBuffer currentCommandBuffer() const { return command_buffers_[current_frame_]; }

private:
    bool createInstance(const char* app_name);
    bool pickPhysicalDevice();
    bool createLogicalDevice();
    bool createSurface(void* window_handle);
    bool createSwapchain();
    bool createRenderPass();
    bool createFramebuffers();
    bool createCommandPool();
    bool createCommandBuffers();
    bool createSyncObjects();

    // Vulkan objects
    VkInstance instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue graphics_queue_ = VK_NULL_HANDLE;
    VkQueue compute_queue_ = VK_NULL_HANDLE;
    VkQueue present_queue_ = VK_NULL_HANDLE;
    uint32_t graphics_queue_family_ = UINT32_MAX;
    uint32_t compute_queue_family_ = UINT32_MAX;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapchain_format_ = VK_FORMAT_B8G8R8A8_SRGB;
    VkExtent2D swapchain_extent_{};
    std::vector<VkImage> swapchain_images_;
    std::vector<VkImageView> swapchain_image_views_;
    VkRenderPass render_pass_ = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers_;
    VkCommandPool command_pool_ = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> command_buffers_;
    std::vector<VkSemaphore> image_available_semaphores_;
    std::vector<VkSemaphore> render_finished_semaphores_;
    std::vector<VkFence> in_flight_fences_;
    uint32_t current_frame_ = 0;
    bool frame_started_ = false;
    uint32_t roughW_ = 512, roughH_ = 288, finalW_ = 1280, finalH_ = 720;
};

// ========== SHADER RECOMPILER (Maxwell → SPIR-V) ==========
class ShaderRecompiler {
public:
    ShaderRecompiler(VulkanContext* ctx);
    ~ShaderRecompiler() = default;

    // Compila shader Maxwell bytecode para SPIR-V
    bool compileShader(const MaxwellShaderIR& ir, std::vector<uint32_t>& out_spirv);

    // Cria pipeline gráfico/compute
    std::unique_ptr<CompiledPipeline> createGraphicsPipeline(const MaxwellShaderIR& vs_ir, const MaxwellShaderIR& fs_ir,
                                                              VkRenderPass render_pass, uint32_t subpass = 0);
    std::unique_ptr<CompiledPipeline> createComputePipeline(const MaxwellShaderIR& cs_ir);

    // Cache de pipelines compilados
    CompiledPipeline* getOrCreateGraphicsPipeline(uint64_t vs_hash, uint64_t fs_hash,
                                                   VkRenderPass render_pass);
    CompiledPipeline* getOrCreateComputePipeline(uint64_t cs_hash);

private:
    VulkanContext* ctx_;
    std::unordered_map<uint64_t, std::unique_ptr<CompiledPipeline>> graphics_pipelines_;
    std::unordered_map<uint64_t, std::unique_ptr<CompiledPipeline>> compute_pipelines_;

    // Maxwell → SPIR-V translation
    bool translateMaxwellToSPIRV(const std::vector<uint32_t>& maxwell_code, ShaderStage stage,
                                  std::vector<uint32_t>& spirv, MaxwellShaderIR& ir);

    // Instruction translation helpers
    struct TranslatorState {
        std::vector<uint32_t> spirv;
        std::vector<uint32_t> maxwell_code;
        size_t pc = 0;
        uint32_t next_id = 1;
        std::unordered_map<uint32_t, uint32_t> reg_to_id; // Maxwell reg -> SPIR-V id
        std::unordered_map<uint32_t, uint32_t> pred_regs; // predicate registers
        ShaderStage stage;
        
        // SPIR-V type IDs
        uint32_t void_type = 0;
        uint32_t bool_type = 0;
        uint32_t int32_type = 0;
        uint32_t uint32_type = 0;
        uint32_t int16_type = 0;
        uint32_t float32_type = 0;
        uint32_t float16_type = 0;
        uint32_t vec2f = 0, vec3f = 0, vec4f = 0;
        uint32_t vec2i = 0, vec3i = 0, vec4i = 0;
        uint32_t vec2u = 0, vec3u = 0, vec4u = 0;
        uint32_t mat2x2 = 0, mat3x3 = 0, mat4x4 = 0;
        uint32_t mat2x3 = 0, mat3x2 = 0, mat2x4 = 0, mat4x2 = 0, mat3x4 = 0, mat4x3 = 0;
        uint32_t sampler_type = 0;
        uint32_t img2d = 0, img2d_array = 0, img3d = 0, imgcube = 0, img2d_depth = 0, img2d_ms = 0;
        uint32_t sampled_img2d = 0, sampled_img2d_array = 0, sampled_img3d = 0, sampled_imgcube = 0, sampled_img2d_depth = 0;
        uint32_t storage_img2d_rgba8 = 0, storage_img2d_rgba16f = 0, storage_img2d_r32f = 0;
        uint32_t push_constant_struct = 0;
        uint32_t push_ptr_type = 0;
        uint32_t push_var = 0;
        uint32_t push_var2 = 0;
        uint32_t void_func_type = 0;
        uint32_t in_pos = 0;
        uint32_t out_pos = 0;
        uint32_t out_obj_id = 0;
        uint32_t out_color = 0;
        uint32_t out_depth = 0;
        uint32_t out_obj_id_frag = 0;
        uint32_t rough_color_var = 0;
        uint32_t rough_depth_var = 0;
        uint32_t rough_obj_id_var = 0;
        uint32_t prev_frame_var = 0;
        uint32_t motion_vectors_var = 0;
        uint32_t prev_obj_id_var = 0;
        uint32_t output_image_var = 0;
        uint32_t main_label = 0;
        
        // Methods
        void emitOp(uint32_t opcode, const std::vector<uint32_t>& operands);
        uint32_t getNextId() { return next_id++; }
        uint32_t getOrCreateVar(uint32_t maxwell_reg);
        void translateInstruction();
    };

    void emitOp(TranslatorState& st, uint32_t opcode, const std::vector<uint32_t>& operands);
    uint32_t getOrCreateVar(TranslatorState& st, uint32_t maxwell_reg, VkShaderStageFlags stage);
    void translateInstruction(TranslatorState& st);
};

// ========== GPU EXECUTOR VULKAN (substitui GpuExecutor stub) ==========
class VulkanGpuExecutor {
public:
    VulkanGpuExecutor();
    ~VulkanGpuExecutor();

    bool init(void* window_handle = nullptr);
    void shutdown();

    // Executa command buffer Maxwell (tradução + submit Vulkan)
    bool execute(const uint8_t* cmd_buf, size_t size);

    // Resource creation (chamado via IPC do jogo)
    uint32_t createTexture(uint32_t w, uint32_t h, TexFormat fmt, const uint8_t* data, size_t size);
    uint32_t createSampler(uint32_t min_f, uint32_t mag_f, uint32_t wrap_s, uint32_t wrap_t);
    uint32_t createRenderTarget(uint32_t w, uint32_t h);
    uint32_t createShader(ShaderStage stage, const std::vector<uint32_t>& bytecode);

    // Asset Pipeline
    void setAssetRegistry(core::AssetRegistry* registry, core::Infector* infector);
    bool uploadAllAssets();
    void setResolution(uint32_t roughW, uint32_t roughH, uint32_t finalW, uint32_t finalH);

    // Stats
    uint64_t totalDrawCalls() const { return draw_calls_; }
    uint64_t totalComputeDispatches() const { return compute_dispatches_; }
    uint64_t totalBytesExecuted() const { return bytes_executed_; }

private:
    std::unique_ptr<VulkanContext> vk_ctx_;
    std::unique_ptr<ShaderRecompiler> recompiler_;
    std::unique_ptr<PipelineCache> pipeline_cache_;
    std::unique_ptr<FramebufferManager> fb_mgr_;
    std::unique_ptr<PainterCompute> painter_;
    std::unique_ptr<AssetPipeline> asset_pipeline_;

    GpuState state_;
    std::unordered_map<uint32_t, Shader> shaders_;
    std::unordered_map<uint32_t, std::unique_ptr<VulkanImage>> textures_;
    std::unordered_map<uint32_t, std::unique_ptr<VulkanSampler>> samplers_;
    std::unordered_map<uint32_t, std::unique_ptr<VulkanImage>> render_targets_;
    std::unordered_map<uint32_t, std::unique_ptr<VulkanBuffer>> buffers_;
    std::vector<uint32_t> unknown_ops_;
    uint64_t draw_calls_ = 0;
    uint64_t compute_dispatches_ = 0;
    uint64_t bytes_executed_ = 0;
    uint32_t next_tex_id_ = 1;
    uint32_t next_samp_id_ = 1;
    uint32_t next_rt_id_ = 1;
    uint32_t next_shader_id_ = 1;
    uint32_t next_buffer_id_ = 1;

    // Command buffer execution
    bool executeDrawArrays(uint32_t vertex_count, uint32_t instance_count);
    bool executeDrawIndexed(uint32_t index_count, uint32_t instance_count, uint32_t base_vertex);
    bool executeComputeDispatch(uint32_t x, uint32_t y, uint32_t z);
    bool bindPipeline();
    bool updateDescriptors();
    bool pushConstants();
};

} // namespace gpu
} // namespace mgd