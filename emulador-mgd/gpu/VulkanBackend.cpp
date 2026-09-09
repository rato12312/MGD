#include "VulkanBackend.h"
#include <cstring>
#include <vector>
#include <array>

namespace mgd {
namespace gpu {

// ========== VULKAN CONTEXT (rascunho minimo: offscreen 0.4x, 1 pass, tile memory) ==========
VulkanContext::VulkanContext() {}
VulkanContext::~VulkanContext() { shutdown(); }

bool VulkanContext::init(const char* app_name, void* window_handle) {
#ifdef VK_VERSION_1_0
    if (!createInstance(app_name)) return false;
    if (!pickPhysicalDevice()) return false;
    if (!createLogicalDevice()) return false;
    if (window_handle && !createSurface(window_handle)) return false;
    if (window_handle && !createSwapchain()) {}
    if (!createRenderPass()) return false;
    if (!createFramebuffers()) return false;
    if (!createCommandPool()) return false;
    if (!createCommandBuffers()) return false;
    if (!createSyncObjects()) return false;
    return true;
#else
    (void)app_name; (void)window_handle;
    return true; // sem SDK: stub ok (CI nao quebra)
#endif
}

void VulkanContext::shutdown() {
#ifdef VK_VERSION_1_0
    if (device_ != VK_NULL_HANDLE) vkDeviceWaitIdle(device_);
    for (auto f : in_flight_fences_) if (f) vkDestroyFence(device_, f, nullptr);
    for (auto s : render_finished_semaphores_) if (s) vkDestroySemaphore(device_, s, nullptr);
    for (auto s : image_available_semaphores_) if (s) vkDestroySemaphore(device_, s, nullptr);
    if (command_pool_ != VK_NULL_HANDLE) vkDestroyCommandPool(device_, command_pool_, nullptr);
    for (auto fb : framebuffers_) if (fb) vkDestroyFramebuffer(device_, fb, nullptr);
    if (render_pass_ != VK_NULL_HANDLE) vkDestroyRenderPass(device_, render_pass_, nullptr);
    for (auto v : swapchain_image_views_) if (v) vkDestroyImageView(device_, v, nullptr);
    if (swapchain_ != VK_NULL_HANDLE) vkDestroySwapchainKHR(device_, swapchain_, nullptr);
    if (surface_ != VK_NULL_HANDLE) vkDestroySurfaceKHR(instance_, surface_, nullptr);
    if (device_ != VK_NULL_HANDLE) vkDestroyDevice(device_, nullptr);
    if (instance_ != VK_NULL_HANDLE) vkDestroyInstance(instance_, nullptr);
#endif
    instance_ = VK_NULL_HANDLE; device_ = VK_NULL_HANDLE;
}

bool VulkanContext::beginFrame() {
#ifdef VK_VERSION_1_0
    if (device_ == VK_NULL_HANDLE) return true; // stub
    vkWaitForFences(device_, 1, &in_flight_fences_[current_frame_], VK_TRUE, UINT64_MAX);
    // offscreen rascunho: nao precisa acquireNextImage
    vkResetFences(device_, 1, &in_flight_fences_[current_frame_]);
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(command_buffers_[current_frame_], &bi);
    frame_started_ = true;
    return true;
#else
    return true;
#endif
}

void VulkanContext::endFrame() {
#ifdef VK_VERSION_1_0
    if (!frame_started_) return;
    vkEndCommandBuffer(command_buffers_[current_frame_]);
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1;
    si.pCommandBuffers = &command_buffers_[current_frame_];
    vkQueueSubmit(graphics_queue_, 1, &si, in_flight_fences_[current_frame_]);
    current_frame_ = (current_frame_ + 1) % 2;
    frame_started_ = false;
#endif
}

void VulkanContext::waitIdle() {
#ifdef VK_VERSION_1_0
    if (device_ != VK_NULL_HANDLE) vkDeviceWaitIdle(device_);
#endif
}

std::unique_ptr<VulkanBuffer> VulkanContext::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props) {
    auto b = std::make_unique<VulkanBuffer>();
    b->size = size; b->usage = usage; b->props = props;
#ifdef VK_VERSION_1_0
    if (device_ == VK_NULL_HANDLE) return b; // stub
    VkBufferCreateInfo ci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    ci.size = size; ci.usage = usage; ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device_, &ci, nullptr, &b->buffer) != VK_SUCCESS) return nullptr;
    VkMemoryRequirements mr{}; vkGetBufferMemoryRequirements(device_, b->buffer, &mr);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = mr.size;
    // acha tipo compativel
    VkPhysicalDeviceMemoryProperties mp{}; vkGetPhysicalDeviceMemoryProperties(physical_device_, &mp);
    for (uint32_t i=0;i<mp.memoryTypeCount;i++) if ((mr.memoryTypeBits & (1u<<i)) && (mp.memoryTypes[i].propertyFlags & props)==props) { ai.memoryTypeIndex=i; break; }
    if (vkAllocateMemory(device_, &ai, nullptr, &b->memory)!=VK_SUCCESS) { vkDestroyBuffer(device_, b->buffer,nullptr); return nullptr; }
    vkBindBufferMemory(device_, b->buffer, b->memory, 0);
    if (props & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) vkMapMemory(device_, b->memory, 0, size, 0, &b->mapped);
#endif
    return b;
}

std::unique_ptr<VulkanImage> VulkanContext::createImage(uint32_t w, uint32_t h, VkFormat format, VkImageUsageFlags usage, uint32_t mips) {
    auto img = std::make_unique<VulkanImage>();
    img->format = format; img->extent = {w,h,1}; img->mip_levels = mips;
#ifdef VK_VERSION_1_0
    if (device_ == VK_NULL_HANDLE) return img;
    VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ci.imageType = VK_IMAGE_TYPE_2D; ci.format = format; ci.extent = {w,h,1}; ci.mipLevels = mips; ci.arrayLayers = 1;
    ci.samples = VK_SAMPLE_COUNT_1_BIT; ci.tiling = VK_IMAGE_TILING_OPTIMAL; ci.usage = usage; ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE; ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(device_, &ci, nullptr, &img->image)!=VK_SUCCESS) return nullptr;
    VkMemoryRequirements mr{}; vkGetImageMemoryRequirements(device_, img->image, &mr);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO}; ai.allocationSize = mr.size;
    VkPhysicalDeviceMemoryProperties mp{}; vkGetPhysicalDeviceMemoryProperties(physical_device_, &mp);
    for (uint32_t i=0;i<mp.memoryTypeCount;i++) if ((mr.memoryTypeBits & (1u<<i)) && (mp.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) { ai.memoryTypeIndex=i; break; }
    if (vkAllocateMemory(device_, &ai, nullptr, &img->memory)!=VK_SUCCESS) { vkDestroyImage(device_, img->image,nullptr); return nullptr; }
    vkBindImageMemory(device_, img->image, img->memory, 0);
    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = img->image; vi.viewType = VK_IMAGE_VIEW_TYPE_2D; vi.format = format;
    vi.subresourceRange.aspectMask = (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    vi.subresourceRange.levelCount = mips; vi.subresourceRange.layerCount = 1;
    vkCreateImageView(device_, &vi, nullptr, &img->view);
#endif
    return img;
}

std::unique_ptr<VulkanSampler> VulkanContext::createSampler(VkFilter minF, VkFilter magF, VkSamplerAddressMode wrap) {
    auto s = std::make_unique<VulkanSampler>();
#ifdef VK_VERSION_1_0
    if (device_ == VK_NULL_HANDLE) return s;
    VkSamplerCreateInfo ci{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    ci.magFilter = magF; ci.minFilter = minF; ci.addressModeU = wrap; ci.addressModeV = wrap; ci.addressModeW = wrap;
    ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST; ci.maxLod = 0;
    vkCreateSampler(device_, &ci, nullptr, &s->sampler);
#endif
    return s;
}

VkCommandBuffer VulkanContext::beginSingleTimeCommands() {
#ifdef VK_VERSION_1_0
    if (device_ == VK_NULL_HANDLE) return VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool = command_pool_; ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; ai.commandBufferCount = 1;
    VkCommandBuffer cb; vkAllocateCommandBuffers(device_, &ai, &cb);
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cb, &bi);
    return cb;
#else
    return VK_NULL_HANDLE;
#endif
}

void VulkanContext::endSingleTimeCommands(VkCommandBuffer cb) {
#ifdef VK_VERSION_1_0
    if (cb==VK_NULL_HANDLE || device_==VK_NULL_HANDLE) return;
    vkEndCommandBuffer(cb);
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO}; si.commandBufferCount=1; si.pCommandBuffers=&cb;
    vkQueueSubmit(graphics_queue_,1,&si,VK_NULL_HANDLE); vkQueueWaitIdle(graphics_queue_);
    vkFreeCommandBuffers(device_, command_pool_,1,&cb);
#else
    (void)cb;
#endif
}

// ---- internos (stubs honestos se sem Vulkan) ----
bool VulkanContext::createInstance(const char* app_name) {
#ifdef VK_VERSION_1_0
    VkApplicationInfo ai{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    ai.pApplicationName = app_name; ai.applicationVersion = VK_MAKE_VERSION(1,0,0);
    ai.pEngineName = "MGD Odyssey"; ai.engineVersion = VK_MAKE_VERSION(1,0,0); ai.apiVersion = VK_API_VERSION_1_1;
    VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; ci.pApplicationInfo = &ai;
    return vkCreateInstance(&ci,nullptr,&instance_)==VK_SUCCESS;
#else
    (void)app_name; return true;
#endif
}
bool VulkanContext::pickPhysicalDevice() {
#ifdef VK_VERSION_1_0
    uint32_t n=0; vkEnumeratePhysicalDevices(instance_,&n,nullptr);
    if(n==0) return false;
    std::vector<VkPhysicalDevice> ds(n); vkEnumeratePhysicalDevices(instance_,&n,ds.data());
    physical_device_=ds[0];
    // acha filas
    uint32_t qn=0; vkGetPhysicalDeviceQueueFamilyProperties(physical_device_,&qn,nullptr);
    std::vector<VkQueueFamilyProperties> qs(qn); vkGetPhysicalDeviceQueueFamilyProperties(physical_device_,&qn,qs.data());
    for(uint32_t i=0;i<qn;i++){ if(qs[i].queueFlags & VK_QUEUE_GRAPHICS_BIT){ graphics_queue_family_=i; compute_queue_family_=i; break; } }
    return graphics_queue_family_!=UINT32_MAX;
#else
    return true;
#endif
}
bool VulkanContext::createLogicalDevice() {
#ifdef VK_VERSION_1_0
    float prio=1.0f;
    VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO}; qi.queueFamilyIndex=graphics_queue_family_; qi.queueCount=1; qi.pQueuePriorities=&prio;
    VkPhysicalDeviceFeatures feats{};
    VkDeviceCreateInfo ci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO}; ci.queueCreateInfoCount=1; ci.pQueueCreateInfos=&qi; ci.pEnabledFeatures=&feats;
    if(vkCreateDevice(physical_device_,&ci,nullptr,&device_)!=VK_SUCCESS) return false;
    vkGetDeviceQueue(device_,graphics_queue_family_,0,&graphics_queue_);
    compute_queue_=graphics_queue_; present_queue_=graphics_queue_;
    return true;
#else
    return true;
#endif
}
bool VulkanContext::createSurface(void*) { return true; }
bool VulkanContext::createSwapchain() { return true; }
bool VulkanContext::createRenderPass() {
#ifdef VK_VERSION_1_0
    if(device_==VK_NULL_HANDLE) return true;
    // Rascunho: 1 color R8G8B8A8 + 1 depth D16, 0.4x, single subpass, tile memory (LOAD_CLEAR, STORE_STORE)
    VkAttachmentDescription color{}; color.format = VK_FORMAT_R8G8B8A8_UNORM; color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; color.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkAttachmentDescription depth{}; depth.format = VK_FORMAT_D16_UNORM; depth.samples = VK_SAMPLE_COUNT_1_BIT;
    depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    VkAttachmentReference cr{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference dr{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sub{}; sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; sub.colorAttachmentCount=1; sub.pColorAttachments=&cr; sub.pDepthStencilAttachment=&dr;
    std::array<VkAttachmentDescription,2> atts{color,depth};
    VkRenderPassCreateInfo ci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO}; ci.attachmentCount=2; ci.pAttachments=atts.data(); ci.subpassCount=1; ci.pSubpasses=&sub;
    return vkCreateRenderPass(device_,&ci,nullptr,&render_pass_)==VK_SUCCESS;
#else
    return true;
#endif
}
bool VulkanContext::createFramebuffers() {
#ifdef VK_VERSION_1_0
    if(device_==VK_NULL_HANDLE) return true;
    // offscreen 512x288 (0.4x 1280x720)
    auto color = createImage(512,288,VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT);
    auto depth = createImage(512,288,VK_FORMAT_D16_UNORM, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    if(!color||!depth) return false;
    // guarda views temporario p/ framebuffer
    VkImageView views[2] = {color->view, depth->view};
    VkFramebufferCreateInfo ci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    ci.renderPass = render_pass_; ci.attachmentCount=2; ci.pAttachments=views; ci.width=512; ci.height=288; ci.layers=1;
    VkFramebuffer fb; if(vkCreateFramebuffer(device_,&ci,nullptr,&fb)!=VK_SUCCESS) return false;
    framebuffers_.push_back(fb);
    // mantem imagens vivas (vaza de proposito no rascunho; Painter vai gerenciar)
    (void)color.release(); (void)depth.release();
    return true;
#else
    return true;
#endif
}
bool VulkanContext::createCommandPool() {
#ifdef VK_VERSION_1_0
    if(device_==VK_NULL_HANDLE) return true;
    VkCommandPoolCreateInfo ci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO}; ci.queueFamilyIndex=graphics_queue_family_; ci.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    return vkCreateCommandPool(device_,&ci,nullptr,&command_pool_)==VK_SUCCESS;
#else
    return true;
#endif
}
bool VulkanContext::createCommandBuffers() {
#ifdef VK_VERSION_1_0
    if(device_==VK_NULL_HANDLE) return true;
    command_buffers_.resize(2);
    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO}; ai.commandPool=command_pool_; ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; ai.commandBufferCount=2;
    return vkAllocateCommandBuffers(device_,&ai,command_buffers_.data())==VK_SUCCESS;
#else
    return true;
#endif
}
bool VulkanContext::createSyncObjects() {
#ifdef VK_VERSION_1_0
    if(device_==VK_NULL_HANDLE) return true;
    image_available_semaphores_.resize(2); render_finished_semaphores_.resize(2); in_flight_fences_.resize(2);
    VkSemaphoreCreateInfo si{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO}; fi.flags=VK_FENCE_CREATE_SIGNALED_BIT;
    for(int i=0;i<2;i++){ vkCreateSemaphore(device_,&si,nullptr,&image_available_semaphores_[i]); vkCreateSemaphore(device_,&si,nullptr,&render_finished_semaphores_[i]); vkCreateFence(device_,&fi,nullptr,&in_flight_fences_[i]); }
#endif
    return true;
}

// ========== SHADER RECOMPILER (Maxwell -> SPIR-V, rascunho: shaders fixos minimos) ==========
ShaderRecompiler::ShaderRecompiler(VulkanContext* ctx): ctx_(ctx) {}

bool ShaderRecompiler::compileShader(const MaxwellShaderIR& ir, std::vector<uint32_t>& out_spirv) {
    // Maxwell bytecode -> SPIR-V translator
    // Maxwell ISA: 32-bit instructions, 256 registers (R0-R255), 8 predicates (P0-P7), texture ops
    // Output: SPIR-V 1.3 (Vulkan 1.1+)
    if (ir.bytecode.empty()) return false;
    
    TranslatorState st;
    st.stage = ir.stage;
    st.maxwell_code = ir.bytecode;
    st.pc = 0;
    st.next_id = 1;
    
    // SPIR-V header
    st.spirv.push_back(0x07230203); // Magic
    st.spirv.push_back(0x00010000); // Version 1.0
    st.spirv.push_back(0x00080001); // Generator: MGD 1.0
    st.spirv.push_back(0x00000000); // Bound (will fix later)
    st.spirv.push_back(0x00000000); // Schema
    
    // Capabilities
    st.emitOp(SpvOpCapability, {SpvCapabilityShader});
    if (ir.stage == ShaderStage::VERTEX) st.emitOp(SpvOpCapability, {SpvCapabilityGeometry});
    if (ir.stage == ShaderStage::COMPUTE) st.emitOp(SpvOpCapability, {SpvCapabilityKernel});
    st.emitOp(SpvOpCapability, {SpvCapabilityImageQuery});
    st.emitOp(SpvOpCapability, {SpvCapabilitySampleRateShading});
    st.emitOp(SpvOpCapability, {SpvCapabilitySampled1D});
    st.emitOp(SpvOpCapability, {SpvCapabilitySampled2D});
    st.emitOp(SpvOpCapability, {SpvCapabilitySampled3D});
    st.emitOp(SpvOpCapability, {SpvCapabilitySampledCube});
    st.emitOp(SpvOpCapability, {SpvCapabilitySampledBuffer});
    st.emitOp(SpvOpCapability, {SpvCapabilityStorageImage});
    st.emitOp(SpvOpCapability, {SpvCapabilityImageBuffer});
    st.emitOp(SpvOpCapability, {SpvCapabilityStorageBuffer});
    st.emitOp(SpvOpCapability, {SpvCapabilityClipDistance});
    st.emitOp(SpvOpCapability, {SpvCapabilityCullDistance});
    
    // Extensions
    st.emitOp(SpvOpExtension, {0, 0, 0, 0}); // "SPV_KHR_vulkan_memory_model"
    st.emitOp(SpvOpExtension, {0, 0, 0, 0}); // "SPV_KHR_shader_draw_parameters"
    st.emitOp(SpvOpExtension, {0, 0, 0, 0}); // "SPV_EXT_descriptor_indexing"
    
    // Memory model
    st.emitOp(SpvOpMemoryModel, {SpvAddressingModelLogical, SpvMemoryModelVulkanKHR});
    
    // Entry point
    uint32_t entry_id = st.getNextId();
    std::vector<uint32_t> entry_ops = {static_cast<uint32_t>(ir.stage), entry_id};
    const char* name = "main";
    for (char c : name) entry_ops.push_back(static_cast<uint32_t>(c));
    entry_ops.push_back(0); // null terminator
    st.spirv.push_back((SpvOpEntryPoint << 16) | ((entry_ops.size()+1) & 0xFFFF));
    for (uint32_t op : entry_ops) st.spirv.push_back(op);
    
    // Execution mode
    if (ir.stage == ShaderStage::VERTEX) {
        st.emitOp(SpvOpExecutionMode, {entry_id, SpvExecutionModeOriginUpperLeft});
    } else if (ir.stage == ShaderStage::FRAGMENT) {
        st.emitOp(SpvOpExecutionMode, {entry_id, SpvExecutionModeOriginUpperLeft});
        st.emitOp(SpvOpExecutionMode, {entry_id, SpvExecutionModeDepthReplacing});
    } else if (ir.stage == ShaderStage::COMPUTE) {
        st.emitOp(SpvOpExecutionMode, {entry_id, SpvExecutionModeLocalSize, 8, 8, 1});
    }
    
    // Debug name
    st.emitOp(SpvOpName, {entry_id, 'm','a','i','n',0});
    
    // Built-in types
    uint32_t void_type = st.getNextId();
    st.spirv.push_back((SpvOpTypeVoid << 16) | (2 << 16) | (void_type << 16));
    
    uint32_t bool_type = st.getNextId();
    st.spirv.push_back((SpvOpTypeBool << 16) | (2 << 16) | (bool_type << 16));
    
    uint32_t int32_type = st.getNextId();
    st.spirv.push_back((SpvOpTypeInt << 16) | (3 << 16) | (int32_type << 16) | 32 | (1 << 16)); // signed 32-bit
    
    uint32_t uint32_type = st.getNextId();
    st.spirv.push_back((SpvOpTypeInt << 16) | (3 << 16) | (uint32_type << 16) | 32 | (0 << 16)); // unsigned 32-bit
    
    uint32_t int16_type = st.getNextId();
    st.spirv.push_back((SpvOpTypeInt << 16) | (3 << 16) | (int16_type << 16) | 16 | (1 << 16));
    
    uint32_t float32_type = st.getNextId();
    st.spirv.push_back((SpvOpTypeFloat << 16) | (3 << 16) | (float32_type << 16) | 32);
    
    uint32_t float16_type = st.getNextId();
    st.spirv.push_back((SpvOpTypeFloat << 16) | (3 << 16) | (float16_type << 16) | 16);
    
    // Vector types
    auto makeVectorType = [&](uint32_t base, int count) {
        uint32_t vec_type = st.getNextId();
        st.spirv.push_back((SpvOpTypeVector << 16) | (3 << 16) | (vec_type << 16) | (base << 16) | (count & 0xFFFF));
        return vec_type;
    };
    
    uint32_t vec2f = makeVectorType(float32_type, 2);
    uint32_t vec3f = makeVectorType(float32_type, 3);
    uint32_t vec4f = makeVectorType(float32_type, 4);
    uint32_t vec2i = makeVectorType(int32_type, 2);
    uint32_t vec3i = makeVectorType(int32_type, 3);
    uint32_t vec4i = makeVectorType(int32_type, 4);
    uint32_t vec2u = makeVectorType(uint32_type, 2);
    uint32_t vec3u = makeVectorType(uint32_type, 3);
    uint32_t vec4u = makeVectorType(uint32_type, 4);
    
    // Matrix types
    auto makeMatrixType = [&](uint32_t vec_type, int columns) {
        uint32_t mat_type = st.getNextId();
        st.spirv.push_back((SpvOpTypeMatrix << 16) | (3 << 16) | (mat_type << 16) | (vec_type << 16) | (columns & 0xFFFF));
        return mat_type;
    };
    
    uint32_t mat2x2 = makeMatrixType(vec2f, 2);
    uint32_t mat3x3 = makeMatrixType(vec3f, 3);
    uint32_t mat4x4 = makeMatrixType(vec4f, 4);
    uint32_t mat2x3 = makeMatrixType(vec2f, 3);
    uint32_t mat3x2 = makeMatrixType(vec3f, 2);
    uint32_t mat2x4 = makeMatrixType(vec2f, 4);
    uint32_t mat4x2 = makeMatrixType(vec4f, 2);
    uint32_t mat3x4 = makeMatrixType(vec3f, 4);
    uint32_t mat4x3 = makeVectorType(vec4f, 3);
    
    // Sampler type
    uint32_t sampler_type = st.getNextId();
    st.spirv.push_back((SpvOpTypeSampler << 16) | (2 << 16) | (sampler_type << 16));
    
    // Image types (sampled)
    auto makeImageType = [&](int dim, bool depth, bool arrayed, bool ms) {
        uint32_t img_type = st.getNextId();
        uint32_t dim_val = 0;
        switch(dim) {
            case 1: dim_val = SpvDim1D; break;
            case 2: dim_val = SpvDim2D; break;
            case 3: dim_val = SpvDim3D; break;
            case 0: dim_val = SpvDimCube; break;
            default: dim_val = SpvDim2D;
        }
        uint32_t depth_val = depth ? 1 : 0;
        uint32_t array_val = arrayed ? 1 : 0;
        uint32_t ms_val = ms ? 1 : 0;
        uint32_t sampled = 1; // sampled
        uint32_t format = 0; // Unknown
        st.spirv.push_back((SpvOpTypeImage << 16) | (9 << 16) | (img_type << 16) | (float32_type << 16) | (dim_val << 16) | (depth_val << 16) | (array_val << 16) | (ms_val << 16) | (sampled << 16) | (format << 16));
        return img_type;
    };
    
    uint32_t img2d = makeImageType(2, false, false, false);
    uint32_t img2d_array = makeImageType(2, false, true, false);
    uint32_t img3d = makeImageType(3, false, false, false);
    uint32_t imgcube = makeImageType(0, false, false, false);
    uint32_t img2d_depth = makeImageType(2, true, false, false);
    uint32_t img2d_ms = makeImageType(2, false, false, true);
    
    // Sampler
    st.spirv.push_back((SpvOpTypeSampler << 16) | (2 << 16) | (sampler_type << 16));
    
    // Sampled image types
    auto makeSampledImage = [&](uint32_t img_type) {
        uint32_t sampled_type = st.getNextId();
        st.spirv.push_back((SpvOpTypeSampledImage << 16) | (3 << 16) | (sampled_type << 16) | (img_type << 16));
        return sampled_type;
    };
    
    uint32_t sampled_img2d = makeSampledImage(img2d);
    uint32_t sampled_img2d_array = makeSampledImage(img2d_array);
    uint32_t sampled_img3d = makeSampledImage(img3d);
    uint32_t sampled_imgcube = makeSampledImage(imgcube);
    uint32_t sampled_img2d_depth = makeSampledImage(img2d_depth);
    
    // Storage image types (for compute)
    auto makeStorageImage = [&](uint32_t img_type, uint32_t format) {
        uint32_t storage_type = st.getNextId();
        st.spirv.push_back((SpvOpTypeImage << 16) | (9 << 16) | (storage_type << 16) | (format << 16) | (0 << 16) | (0 << 16) | (0 << 16) | (0 << 16) | (2 << 16) | (0 << 16)); // Storage
        return storage_type;
    };
    
    uint32_t storage_img2d_rgba8 = makeStorageImage(img2d, 0); // rgba8
    uint32_t storage_img2d_rgba16f = makeStorageImage(img2d, 0); // rgba16f
    uint32_t storage_img2d_r32f = makeStorageImage(img2d, 0); // r32f
    
    // Sampler
    st.spirv.push_back((SpvOpTypeSampler << 16) | (2 << 16) | (sampler_type << 16));
    
    // Struct for push constants (MVP + object_id + flags + lod + pad)
    // MVP mat4x4 (64 bytes) + object_id(4) + flags(4) + lod(4) + pad(4) = 80 bytes
    uint32_t push_constant_struct = st.getNextId();
    std::vector<uint32_t> push_members = {mat4x4, uint32_type, uint32_type, uint32_type, uint32_type};
    st.spirv.push_back((SpvOpTypeStruct << 16) | ((push_members.size() + 1) << 16) | (push_constant_struct << 16));
    for (uint32_t m : push_members) st.spirv.push_back(m);
    
    // Push constant pointer
    uint32_t push_ptr_type = st.getNextId();
    st.spirv.push_back((SpvOpTypePointer << 16) | (4 << 16) | (push_ptr_type << 16) | (SpvStorageClassPushConstant << 16) | (push_constant_struct << 16));
    
    // Push constant variable
    uint32_t push_var = st.getNextId();
    st.spirv.push_back((SpvOpVariable << 16) | (4 << 16) | (push_var << 16) | (push_ptr_type << 16) | (SpvStorageClassPushConstant << 16));
    st.emitOp(SpvOpName, {push_var, 'P','C',0});
    
    // ===== Interface Variables (Vertex/Fragment I/O) =====
    // Vertex inputs
    uint32_t in_pos = st.getNextId();
    st.spirv.push_back((SpvOpVariable << 16) | (5 << 16) | (in_pos << 16) | (vec3f << 16) | (SpvStorageClassInput << 16));
    st.emitOp(SpvOpName, {in_pos, 'i','n','_','p','o','s',0});
    st.emitOp(SpvOpDecorate, {in_pos, SpvDecorationLocation, 0});
    
    // Vertex outputs / Fragment inputs
    uint32_t out_pos = st.getNextId();
    st.spirv.push_back((SpvOpVariable << 16) | (5 << 16) | (out_pos << 16) | (vec4f << 16) | (SpvStorageClassOutput << 16));
    st.emitOp(SpvOpName, {out_pos, 'g','l','_','P','o','s','i','t','i','o','n',0});
    st.emitOp(SpvOpDecorate, {out_pos, SpvDecorationBuiltIn, SpvBuiltInPosition});
    
    uint32_t out_obj_id = st.getNextId();
    st.spirv.push_back((SpvOpVariable << 16) | (5 << 16) | (out_obj_id << 16) | (uint32_type << 16) | (SpvStorageClassOutput << 16));
    st.emitOp(SpvOpName, {out_obj_id, 'o','u','t','_','o','b','j','_','i','d',0});
    st.emitOp(SpvOpDecorate, {out_obj_id, SpvDecorationLocation, 1});
    
    // Fragment outputs
    uint32_t out_color = st.getNextId();
    st.spirv.push_back((SpvOpVariable << 16) | (5 << 16) | (out_color << 16) | (vec4f << 16) | (SpvStorageClassOutput << 16));
    st.emitOp(SpvOpName, {out_color, 'o','u','t','_','c','o','l','o','r',0});
    st.emitOp(SpvOpDecorate, {out_color, SpvDecorationLocation, 0});
    
    uint32_t out_depth = st.getNextId();
    st.spirv.push_back((SpvOpVariable << 16) | (5 << 16) | (out_depth << 16) | (float32_type << 16) | (SpvStorageClassOutput << 16));
    st.emitOp(SpvOpName, {out_depth, 'o','u','t','_','d','e','p','t','h',0});
    st.emitOp(SpvOpDecorate, {out_depth, SpvDecorationBuiltIn, SpvBuiltInFragDepth});
    
    uint32_t out_obj_id_frag = st.getNextId();
    st.spirv.push_back((SpvOpVariable << 16) | (5 << 16) | (out_obj_id_frag << 16) | (uint32_type << 16) | (SpvStorageClassOutput << 16));
    st.emitOp(SpvOpName, {out_obj_id_frag, 'o','u','t','_','o','b','j','_','i','d',0});
    st.emitOp(SpvOpDecorate, {out_obj_id_frag, SpvDecorationLocation, 1});
    
    // Descriptor set bindings (textures/samplers)
    uint32_t rough_color_var = st.getNextId();
    st.spirv.push_back((SpvOpVariable << 16) | (5 << 16) | (rough_color_var << 16) | (sampled_img2d << 16) | (SpvStorageClassUniformConstant << 16));
    st.emitOp(SpvOpName, {rough_color_var, 'r','o','u','g','h','_','c','o','l','o','r',0});
    st.emitOp(SpvOpDecorate, {rough_color_var, SpvDecorationDescriptorSet, 0});
    st.emitOp(SpvOpDecorate, {rough_color_var, SpvDecorationBinding, 0});
    
    uint32_t rough_depth_var = st.getNextId();
    st.spirv.push_back((SpvOpVariable << 16) | (5 << 16) | (rough_depth_var << 16) | (img2d_depth << 16) | (SpvStorageClassUniformConstant << 16));
    st.emitOp(SpvOpName, {rough_depth_var, 'r','o','u','g','h','_','d','e','p','t','h',0});
    st.emitOp(SpvOpDecorate, {rough_depth_var, SpvDecorationDescriptorSet, 0});
    st.emitOp(SpvOpDecorate, {rough_depth_var, SpvDecorationBinding, 1});
    
    uint32_t rough_obj_id_var = st.getNextId();
    st.spirv.push_back((SpvOpVariable << 16) | (5 << 16) | (rough_obj_id_var << 16) | (sampled_img2d << 16) | (SpvStorageClassUniformConstant << 16));
    st.emitOp(SpvOpName, {rough_obj_id_var, 'r','o','u','g','h','_','o','b','j','_','i','d',0});
    st.emitOp(SpvOpDecorate, {rough_obj_id_var, SpvDecorationDescriptorSet, 0});
    st.emitOp(SpvOpDecorate, {rough_obj_id_var, SpvDecorationBinding, 2});
    
    uint32_t prev_frame_var = st.getNextId();
    st.spirv.push_back((SpvOpVariable << 16) | (5 << 16) | (prev_frame_var << 16) | (sampled_img2d << 16) | (SpvStorageClassUniformConstant << 16));
    st.emitOp(SpvOpName, {prev_frame_var, 'p','r','e','v','_','f','r','a','m','e',0});
    st.emitOp(SpvOpDecorate, {prev_frame_var, SpvDecorationDescriptorSet, 0});
    st.emitOp(SpvOpDecorate, {prev_frame_var, SpvDecorationBinding, 3});
    
    uint32_t motion_vectors_var = st.getNextId();
    st.spirv.push_back((SpvOpVariable << 16) | (5 << 16) | (motion_vectors_var << 16) | (sampled_img2d << 16) | (SpvStorageClassUniformConstant << 16));
    st.emitOp(SpvOpName, {motion_vectors_var, 'm','o','t','i','o','n','_','v','e','c','t','o','r','s',0});
    st.emitOp(SpvOpDecorate, {motion_vectors_var, SpvDecorationDescriptorSet, 0});
    st.emitOp(SpvOpDecorate, {motion_vectors_var, SpvDecorationBinding, 4});
    
    uint32_t prev_obj_id_var = st.getNextId();
    st.spirv.push_back((SpvOpVariable << 16) | (5 << 16) | (prev_obj_id_var << 16) | (sampled_img2d << 16) | (SpvStorageClassUniformConstant << 16));
    st.emitOp(SpvOpName, {prev_obj_id_var, 'p','r','e','v','_','o','b','j','_','i','d',0});
    st.emitOp(SpvOpDecorate, {prev_obj_id_var, SpvDecorationDescriptorSet, 0});
    st.emitOp(SpvOpDecorate, {prev_obj_id_var, SpvDecorationBinding, 5});
    
    // Output image (storage image for compute shader output)
    uint32_t output_image_var = st.getNextId();
    st.spirv.push_back((SpvOpVariable << 16) | (5 << 16) | (output_image_var << 16) | (storage_img2d_rgba8 << 16) | (SpvStorageClassUniform << 16));
    st.emitOp(SpvOpName, {output_image_var, 'o','u','t','_','i','m','a','g','e',0});
    st.emitOp(SpvOpDecorate, {output_image_var, SpvDecorationDescriptorSet, 0});
    st.emitOp(SpvOpDecorate, {output_image_var, SpvDecorationBinding, 6});
    
    // Push constant variable
    uint32_t push_var = st.getNextId();
    st.spirv.push_back((SpvOpVariable << 16) | (4 << 16) | (push_var << 16) | (push_ptr_type << 16) | (SpvStorageClassPushConstant << 16));
    st.emitOp(SpvOpName, {push_var, 'P','C',0});
    
    // Function type for main
    uint32_t void_func_type = st.getNextId();
    st.spirv.push_back((SpvOpTypeFunction << 16) | (3 << 16) | (void_func_type << 16) | (void_type << 16));
    
    // Entry point
    std::vector<uint32_t> entry_ops = {static_cast<uint32_t>(ir.stage), entry_id};
    const char* entry_name = "main";
    for (char c : entry_name) entry_ops.push_back(static_cast<uint32_t>(c));
    entry_ops.push_back(0);
    st.spirv.push_back((SpvOpEntryPoint << 16) | ((entry_ops.size()+1) & 0xFFFF));
    for (uint32_t op : entry_ops) st.spirv.push_back(op);
    
    // Execution mode
    if (ir.stage == ShaderStage::VERTEX) {
        st.emitOp(SpvOpExecutionMode, {entry_id, SpvExecutionModeOriginUpperLeft});
    } else if (ir.stage == ShaderStage::FRAGMENT) {
        st.emitOp(SpvOpExecutionMode, {entry_id, SpvExecutionModeOriginUpperLeft});
        st.emitOp(SpvOpExecutionMode, {entry_id, SpvExecutionModeDepthReplacing});
    } else if (ir.stage == ShaderStage::COMPUTE) {
        st.emitOp(SpvOpExecutionMode, {entry_id, SpvExecutionModeLocalSize, 8, 8, 1});
    }
    
    // Debug name
    st.emitOp(SpvOpName, {entry_id, 'm','a','i','n',0});
    
    // Debug names for types
    st.emitOp(SpvOpName, {void_type, 'v','o','i','d',0});
    st.emitOp(SpvOpName, {bool_type, 'b','o','o','l',0});
    st.emitOp(SpvOpName, {int32_type, 'i','n','t',0});
    st.emitOp(SpvOpName, {uint32_type, 'u','i','n','t',0});
    st.emitOp(SpvOpName, {float32_type, 'f','l','o','a','t',0});
    st.emitOp(SpvOpName, {float16_type, 'h','a','l','f',0});
    st.emitOp(SpvOpName, {vec2f, 'v','e','c','2',0});
    st.emitOp(SpvOpName, {vec3f, 'v','e','c','3',0});
    st.emitOp(SpvOpName, {vec4f, 'v','e','c','4',0});
    st.emitOp(SpvOpName, {mat4x4, 'm','a','t','4',0});
    st.emitOp(SpvOpName, {sampler_type, 's','a','m','p','l','e','r',0});
    st.emitOp(SpvOpName, {img2d, 'i','m','g','2','d',0});
    
    // Function main
    uint32_t main_label = st.getNextId();
    st.spirv.push_back((SpvOpFunction << 16) | (4 << 16) | (void_type << 16) | (entry_id << 16) | (void_func_type << 16) | (0 << 16));
    st.spirv.push_back((SpvOpLabel << 16) | (2 << 16) | (main_label << 16));
    
    // Function end marker (will be overwritten)
    uint32_t func_end_marker = st.getNextId();
    
    // Now translate Maxwell instructions
    while (st.pc < st.maxwell_code.size()) {
        st.translateInstruction();
    }
    
    // Function return and end
    st.emitOp(SpvOpReturn, {});
    st.spirv.push_back((SpvOpFunctionEnd << 16) | (1 << 16));
    
    // Fix bound (max ID + 1)
    st.spirv[3] = st.next_id + 10;
    
    out_spirv = std::move(st.spirv);
    return true;
}

// TranslatorState implementation
void ShaderRecompiler::TranslatorState::emitOp(uint32_t opcode, const std::vector<uint32_t>& operands) {
    uint32_t word_count = static_cast<uint32_t>(operands.size()) + 1;
    spirv.push_back((opcode << 16) | (word_count & 0xFFFF));
    for (uint32_t op : operands) spirv.push_back(op);
}

uint32_t ShaderRecompiler::TranslatorState::getOrCreateVar(uint32_t maxwell_reg, VkShaderStageFlags stage) {
    auto it = reg_to_id.find(maxwell_reg);
    if (it != reg_to_id.end()) return it->second;
    uint32_t id = next_id++;
    reg_to_id[maxwell_reg] = id;
    // Type: float for now (default) - Maxwell registers are typeless but we default to float
    uint32_t float_type = getNextId();
    st.spirv.push_back((SpvOpTypeFloat << 16) | (3 << 16) | (float_type << 16) | 32); // OpTypeFloat 32
    uint32_t ptr_type = getNextId();
    st.spirv.push_back((SpvOpTypePointer << 16) | (4 << 16) | (ptr_type << 16) | (SpvStorageClassFunction << 16) | (float_type << 16));
    return id;
}

uint32_t ShaderRecompiler::TranslatorState::getNextId() {
    return next_id++;
}

void ShaderRecompiler::TranslatorState::translateInstruction() {
    if (pc >= maxwell_code.size()) return;
    uint32_t insn = maxwell_code[pc++];
    
    // Maxwell ISA decoding - simplified but functional subset
    // Maxwell ISA: 32-bit instructions, opcode in bits [6:0], various formats
    // Reference: envydis/envytools Maxwell ISA documentation
    
    uint32_t opcode = insn & 0x7F; // 7-bit primary opcode
    
    // Extract common fields
    uint32_t pred = (insn >> 7) & 0x7;      // Predicate register (P0-P7)
    uint32_t cc = (insn >> 10) & 0x7;       // Condition code
    uint32_t dst = (insn >> 13) & 0xFF;     // Destination register (R0-R255)
    uint32_t src0 = (insn >> 21) & 0xFF;    // Source 0 register
    uint32_t src1 = (insn >> 29) & 0xFF;    // Source 1 register (or immediate)
    uint32_t src2 = (insn >> 37) & 0xFF;    // Source 2 register (for 3-src ops)
    
    // Immediate extraction (various formats)
    uint32_t imm8 = (insn >> 13) & 0xFF;
    int32_t simm8 = static_cast<int8_t>(imm8);
    uint32_t imm16 = (insn >> 13) & 0xFFFF;
    int32_t simm16 = static_cast<int16_t>(imm16);
    uint32_t imm20 = (insn >> 12) & 0xFFFFF;
    int32_t simm20 = (insn & 0x80000) ? (static_cast<int32_t>(imm20) | 0xFFF00000) : static_cast<int32_t>(imm20);
    
    // Predicate handling
    uint32_t pred_reg = pred;
    bool has_pred = pred != 7; // P7 = always true (no predication)
    
    auto getVar = [&](uint32_t reg) -> uint32_t {
        return getOrCreateVar(reg, stage);
    };
    
    auto getConst = [&](float val) -> uint32_t {
        uint32_t id = getNextId();
        st.spirv.push_back((SpvOpConstant << 16) | (4 << 16) | (float32_type << 16) | (id << 16));
        uint32_t bits = *reinterpret_cast<const uint32_t*>(&val);
        st.spirv.push_back(bits);
        return id;
    };
    
    auto getConstInt = [&](int32_t val) -> uint32_t {
        uint32_t id = getNextId();
        st.spirv.push_back((SpvOpConstant << 16) | (4 << 16) | (int32_type << 16) | (id << 16));
        st.spirv.push_back(static_cast<uint32_t>(val));
        return id;
    };
    
    auto getConstUInt = [&](uint32_t val) -> uint32_t {
        uint32_t id = getNextId();
        st.spirv.push_back((SpvOpConstant << 16) | (4 << 16) | (uint32_type << 16) | (id << 16));
        st.spirv.push_back(val);
        return id;
    };
    
    auto getReg = [&](uint32_t reg) -> uint32_t {
        return getOrCreateVar(reg);
    };
    
    auto emitBinaryOp = [&](uint32_t spv_op, uint32_t dst, uint32_t src_a, uint32_t src_b) {
        uint32_t result = getNextId();
        st.emitOp(spv_op, {float32_type, result, getVar(src_a), getVar(src_b)});
        // Store result in destination register
        reg_to_id[dst] = result;
    };
    
    auto emitUnaryOp = [&](uint32_t spv_op, uint32_t dst, uint32_t src) {
        uint32_t result = getNextId();
        st.emitOp(spv_op, {float32_type, result, getVar(src)});
        reg_to_id[dst] = result;
    };
    
    // Handle predication
    auto emitPredicated = [&](auto&& emit_fn) {
        if (has_pred) {
            uint32_t pred_id = getVar(pred_reg);
            // In SPIR-V, we'd use OpSelectionMerge + OpBranchConditional
            // For simplicity, we'll use OpSelect with predicate
            // Real implementation would use proper control flow
        }
        emit_fn();
    };
    
    switch (opcode) {
        // ===== Data Movement =====
        case 0x00: { // MOV (register copy)
            uint32_t src_var = getVar(src0);
            reg_to_id[dst] = getVar(src0);
            break;
        }
        case 0x01: { // MOV immediate (8-bit)
            uint32_t c = getConst(static_cast<float>(simm8));
            reg_to_id[dst] = c;
            break;
        }
        case 0x02: { // MOV 32-bit immediate (via 20-bit immediate)
            uint32_t c = getConst(static_cast<float>(simm20));
            reg_to_id[dst] = c;
            break;
        }
        
        // ===== Integer Arithmetic =====
        case 0x10: // IADD
            emitBinaryOp(SpvOpIAdd, dst, src0, src1);
            break;
        case 0x11: // IADD immediate
            emitBinaryOp(SpvOpIAdd, dst, src0, getConst(static_cast<float>(simm8)));
            break;
        case 0x12: // ISUB
            emitBinaryOp(SpvOpISub, dst, src0, src1);
            break;
        case 0x13: // IMUL
            emitBinaryOp(SpvOpIMul, dst, src0, src1);
            break;
        case 0x14: // IMAD (multiply-add)
            {
                uint32_t mul = getNextId();
                st.emitOp(SpvOpIMul, {int32_type, getNextId(), getVar(src0), getVar(src1)});
                st.emitOp(SpvOpIAdd, {int32_type, getNextId(), mul, getVar(src2)});
                reg_to_id[dst] = mul;
            }
            break;
        case 0x15: // IMUL immediate
            emitBinaryOp(SpvOpIMul, dst, src0, getConst(static_cast<float>(simm8)));
            break;
        case 0x16: // IMAD immediate
            break;
            
        // ===== Floating Point Arithmetic =====
        case 0x10: case 0x11: // FADD
            emitBinaryOp(SpvOpFAdd, dst, src0, src1);
            break;
        case 0x12: case 0x13: // FADD immediate
            emitBinaryOp(SpvOpFAdd, dst, src0, getConst(static_cast<float>(simm8)));
            break;
        case 0x20: // FSUB
            emitBinaryOp(SpvOpFSub, dst, src0, src1);
            break;
        case 0x21: // FSUB immediate
            emitBinaryOp(SpvOpFSub, dst, src0, getConst(static_cast<float>(simm8)));
            break;
        case 0x22: // FMUL
            emitBinaryOp(SpvOpFMul, dst, src0, src1);
            break;
        case 0x23: // FMUL immediate
            emitBinaryOp(SpvOpFMul, dst, src0, getConst(static_cast<float>(simm8)));
            break;
        case 0x24: // FMAD (fused multiply-add)
            {
                uint32_t mul = getNextId();
                st.emitOp(SpvOpFMul, {float32_type, getNextId(), getVar(src0), getVar(src1)});
                st.emitOp(SpvOpFAdd, {float32_type, getNextId(), mul, getVar(src2)});
                reg_to_id[dst] = mul;
            }
            break;
        case 0x25: // FMUL immediate
            emitBinaryOp(SpvOpFMul, dst, src0, getConst(static_cast<float>(simm8)));
            break;
        case 0x26: // FMAD immediate
            break;
        case 0x30: // FDIV
            emitBinaryOp(SpvOpFDiv, dst, src0, src1);
            break;
        case 0x31: // RCP (reciprocal)
            emitUnaryOp(SpvOpFDiv, dst, getConst(1.0f)); // 1.0 / x
            break;
        case 0x32: // RSQ (reciprocal sqrt)
            {
                uint32_t sqrt_id = getNextId();
                // sqrt(x) via sqrt instruction if available, else approximate
                st.emitOp(SpvOpSqrt, {float32_type, getNextId(), getVar(src0)});
                uint32_t rcp = getNextId();
                st.emitOp(SpvOpFDiv, {float32_type, getNextId(), getConst(1.0f), src2});
                reg_to_id[dst] = rcp;
            }
            break;
        case 0x33: // SQRT
            emitUnaryOp(SpvOpSqrt, dst, src0);
            break;
        case 0x34: // RSQ
            {
                uint32_t sqrt_id = getNextId();
                st.emitOp(SpvOpSqrt, {float32_type, getNextId(), getVar(src0)});
                uint32_t rcp = getNextId();
                st.emitOp(SpvOpFDiv, {float32_type, getNextId(), getConst(1.0f), src2});
                reg_to_id[dst] = rcp;
            }
            break;
            
        // ===== Transcendental =====
        case 0x40: // SIN
            emitUnaryOp(SpvOpSin, dst, src0);
            break;
        case 0x41: // COS
            emitUnaryOp(SpvOpCos, dst, src0);
            break;
        case 0x42: // LG2 (log2)
            emitUnaryOp(SpvOpLog2, dst, src0);
            break;
        case 0x43: // EX2 (exp2)
            emitUnaryOp(SpvOpExp2, dst, src0);
            break;
            
        // ===== Comparison =====
        case 0x50: // SETP (set predicate)
            {
                uint32_t cmp_op = 0;
                switch (cc) {
                    case 0: cmp_op = SpvOpFOrdEqual; break;    // EQ
                    case 1: cmp_op = SpvOpFOrdNotEqual; break; // NE
                    case 2: cmp_op = SpvOpFOrdLessThan; break; // LT
                    case 3: cmp_op = SpvOpFOrdGreaterThan; break; // GT
                    case 4: cmp_op = SpvOpFOrdLessThanEqual; break; // LE
                    case 5: cmp_op = SpvOpFOrdGreaterThanEqual; break; // GE
                    default: cmp_op = SpvOpFOrdEqual;
                }
                uint32_t result = getNextId();
                st.emitOp(cmp_op, {bool_type, result, getVar(src0), getVar(src1)});
                reg_to_id[dst] = result; // Store in predicate register
            }
            break;
            
        // ===== Logic =====
        case 0x60: // AND
            emitBinaryOp(SpvOpBitwiseAnd, dst, src0, src1);
            break;
        case 0x61: // OR
            emitBinaryOp(SpvOpBitwiseOr, dst, src0, src1);
            break;
        case 0x62: // XOR
            emitBinaryOp(SpvOpBitwiseXor, dst, src0, src1);
            break;
        case 0x63: // NOT
            emitUnaryOp(SpvOpBitwiseNot, dst, src0);
            break;
            
        // ===== Shift =====
        case 0x70: // SHL
            emitBinaryOp(SpvOpShiftLeftLogical, dst, src0, src1);
            break;
        case 0x71: // SHR
            emitBinaryOp(SpvOpShiftRightLogical, dst, src0, src1);
            break;
        case 0x72: // ASR (arithmetic shift right)
            emitBinaryOp(SpvOpShiftRightArithmetic, dst, src0, src1);
            break;
            
        // ===== Texture =====
        case 0x20: // TEX (texture sample)
            {
                // TEX dst, src0, src1 (src0 = coords, src1 = texture handle)
                uint32_t coord = getVar(src0);
                uint32_t tex = getVar(src1);
                uint32_t result = getNextId();
                // OpImageSampleImplicitLod
                st.emitOp(SpvOpImageSampleImplicitLod, {sampled_img2d, result, tex, coord, 0, 0});
                reg_to_id[dst] = result;
            }
            break;
        case 0x21: // TEXL (LOD bias)
        case 0x22: // TXD (derivatives)
        case 0x23: // TXF (texel fetch)
            break;
            
        // ===== Control Flow =====
        case 0x40: // BRA (branch)
            {
                int32_t offset = simm20;
                // SPIR-V: OpBranch with label
                uint32_t target_label = getNextId();
                // In real implementation, would map PC offset to SPIR-V label
                st.emitOp(SpvOpBranch, {target_label});
            }
            break;
        case 0x41: // BRX (branch predicate)
            {
                uint32_t pred = getVar(pred_reg);
                int32_t offset = simm20;
                // OpBranchConditional
                uint32_t true_label = getNextId();
                uint32_t false_label = getNextId();
                st.emitOp(SpvOpBranchConditional, {getVar(pred_reg), true_label, false_label, 0, 0});
            }
            break;
        case 0x42: // CALL
        case 0x43: // RET
            st.emitOp(SpvOpReturn, {});
            break;
        case 0x50: // EXIT
            st.emitOp(SpvOpKill, {});
            break;
        case 0x51: // RET
            st.emitOp(SpvOpReturn, {});
            break;
            
        // ===== Conversion =====
        case 0x60: // F2I (float to int)
            {
                uint32_t result = getNextId();
                st.emitOp(SpvOpConvertFToS, {int32_type, getNextId(), getVar(src0)});
                reg_to_id[dst] = result;
            }
            break;
        case 0x61: // I2F (int to float)
            {
                uint32_t result = getNextId();
                st.emitOp(SpvOpConvertSToF, {float32_type, getNextId(), getVar(src0)});
                reg_to_id[dst] = result;
            }
            break;
        case 0x61: // F2U
            {
                uint32_t result = getNextId();
                st.emitOp(SpvOpConvertFToU, {uint32_type, getNextId(), getVar(src0)});
                reg_to_id[dst] = result;
            }
            break;
        case 0x62: // U2F
            {
                uint32_t result = getNextId();
                st.emitOp(SpvOpConvertUToF, {float32_type, getNextId(), getVar(src0)});
                reg_to_id[dst] = result;
            }
            break;
            
        // ===== Bitfield =====
        case 0x80: // BFI (bitfield insert)
        case 0x81: // BFE (bitfield extract)
        case 0x82: // POPC
        case 0x83: // LSB
        case 0x84: // SHF (shift)
            break;

        // ===== Memory (LD/ST global/shared/local) =====
        case 0x90: // LD.E (load global)
            {
                uint32_t result = getNextId();
                st.emitOp(SpvOpLoad, {uint32_type, result, getVar(src0)});
                reg_to_id[dst] = result;
            }
            break;
        case 0x91: // ST.E (store global)
            st.emitOp(SpvOpStore, {getVar(dst), getVar(src0)});
            break;
        case 0x92: // LDS (load shared)
            {
                uint32_t result = getNextId();
                st.emitOp(SpvOpLoad, {uint32_type, result, getVar(src0)});
                reg_to_id[dst] = result;
            }
            break;
        case 0x93: // STS (store shared)
            st.emitOp(SpvOpStore, {getVar(dst), getVar(src0)});
            break;
        case 0x94: // ATOM (atomic)
            {
                uint32_t result = getNextId();
                st.emitOp(SpvOpAtomicIAdd, {uint32_type, result, getVar(src0), getVar(src1), getVar(src0)});
                reg_to_id[dst] = result;
            }
            break;
        case 0x95: // MEMBAR (memory barrier)
            st.emitOp(SpvOpMemoryBarrier, {SpvScopeDevice, SpvMemorySemanticsAcquireReleaseMask});
            break;
        case 0x96: // SYNC (control barrier)
            st.emitOp(SpvOpControlBarrier, {SpvScopeWorkgroup, SpvScopeWorkgroup, SpvMemorySemanticsAcquireReleaseMask});
            break;
        case 0x97: // TEXS / TLD (texture load)
            {
                uint32_t result = getNextId();
                st.emitOp(SpvOpImageFetch, {vec4f, result, getVar(src1), getVar(src0)});
                reg_to_id[dst] = result;
            }
            break;
            
        // ===== Special =====
        case 0x00: // MOV (register)
            reg_to_id[dst] = getVar(src0);
            break;
        case 0x01: // MOV immediate
            {
                uint32_t c = getConst(static_cast<float>(simm8));
                reg_to_id[dst] = c;
            }
            break;
        case 0x02: // MOV 32-bit
            {
                uint32_t c = getConst(static_cast<float>(simm20));
                reg_to_id[dst] = c;
            }
            break;
        case 0x03: // MOV predicate
            reg_to_id[dst] = getVar(src0);
            break;
            
        default: {
            // Unknown: emit NOP
            emitOp(SpvOpNop, {});
            break;
        }
    }
    
    // Handle predication for all instructions
    if (has_pred && pred_reg != 7) {
        // In real implementation, would wrap instruction in conditional
        // For now, we just note the predication
    }
    
    // Handle 3-source instructions (FMAD, IMAD, etc.)
    if (opcode >= 0x14 && opcode <= 0x16) { // IMAD variants
        // Already handled above
    }
    if (opcode >= 0x24 && opcode <= 0x26) { // FMAD variants
        // Already handled above
    }
}

std::unique_ptr<CompiledPipeline> ShaderRecompiler::createGraphicsPipeline(const MaxwellShaderIR& vs_ir, const MaxwellShaderIR& fs_ir, VkRenderPass rp, uint32_t subpass) {
    auto p = std::make_unique<CompiledPipeline>();
    p->vs_ir = vs_ir; p->fs_ir = fs_ir;
#ifdef VK_VERSION_1_0
    if(ctx_->device()==VK_NULL_HANDLE) return p;
    // Compile shaders to SPIR-V
    std::vector<uint32_t> vs_spirv, fs_spirv;
    compileShader(vs_ir, vs_spirv);
    compileShader(fs_ir, fs_spirv);
    
    // Create shader modules
    VkShaderModuleCreateInfo vsm{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    vsm.codeSize = vs_spirv.size() * 4; vsm.pCode = vs_spirv.data();
    vkCreateShaderModule(ctx_->device(), &vsm, nullptr, &p->vs_module);
    vsm.codeSize = fs_spirv.size() * 4; vsm.pCode = fs_spirv.data();
    vkCreateShaderModule(ctx_->device(), &vsm, nullptr, &p->fs_module);
    
    // Pipeline layout with push constants: mvp(64) + object_id(4) + flags(4) + lod(4) + pad(4) = 80 bytes
    VkPushConstantRange pc{};
    pc.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pc.offset = 0; pc.size = 80;
    VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    li.pushConstantRangeCount = 1; li.pPushConstantRanges = &pc;
    vkCreatePipelineLayout(ctx_->device(), &li, nullptr, &p->layout);
    
    // Graphics pipeline: vertex input (position only), flat shading, depth test
    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}; stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT; stages[0].module = p->vs_module; stages[0].pName = "main";
    stages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}; stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module = p->fs_module; stages[1].pName = "main";
    
    // Vertex input: position only (vec3)
    VkVertexInputBindingDescription vib{}; vib.binding = 0; vib.stride = 12; vib.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    VkVertexInputAttributeDescription via{}; via.location = 0; via.binding = 0; via.format = VK_FORMAT_R32G32B32_SFLOAT; via.offset = 0;
    VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vi.vertexBindingDescriptionCount = 1; vi.pVertexBindingDescriptions = &vib;
    vi.vertexAttributeDescriptionCount = 1; vi.pVertexAttributeDescriptions = &via;
    
    VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    
    VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vp.viewportCount = 1; vp.scissorCount = 1;
    
    VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.polygonMode = VK_POLYGON_MODE_FILL; rs.cullMode = VK_CULL_MODE_BACK_BIT; rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.0f;
    
    VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    
    // Depth test enable
    VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    ds.depthTestEnable = VK_TRUE; ds.depthWriteEnable = VK_TRUE; ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    
    // Color blend: flat color (no blending)
    VkPipelineColorBlendAttachmentState ca{}; ca.blendEnable = VK_FALSE; ca.colorWriteMask = 0xF;
    VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cb.attachmentCount = 1; cb.pAttachments = &ca;
    
    VkDynamicState dyn[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dy{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dy.dynamicStateCount = 2; dy.pDynamicStates = dyn;
    
    VkGraphicsPipelineCreateInfo gp{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    gp.stageCount = 2; gp.pStages = stages;
    gp.pVertexInputState = &vi; gp.pInputAssemblyState = &ia;
    gp.pViewportState = &vp; gp.pRasterizationState = &rs;
    gp.pMultisampleState = &ms; gp.pDepthStencilState = &ds;
    gp.pColorBlendState = &cb; gp.pDynamicState = &dy;
    gp.layout = p->layout; gp.renderPass = rp; gp.subpass = subpass;
    
    vkCreateGraphicsPipelines(ctx_->device(), VK_NULL_HANDLE, 1, &gp, nullptr, &p->pipeline);
#endif
    return p;
}

std::unique_ptr<CompiledPipeline> ShaderRecompiler::createComputePipeline(const MaxwellShaderIR& cs_ir) {
    auto p = std::make_unique<CompiledPipeline>();
    p->cs_ir = cs_ir;
    return p;
}

CompiledPipeline* ShaderRecompiler::getOrCreateGraphicsPipeline(uint64_t vs_hash, uint64_t fs_hash, VkRenderPass rp) {
    uint64_t key = vs_hash ^ (fs_hash<<1);
    auto it = graphics_pipelines_.find(key);
    if(it!=graphics_pipelines_.end()) return it->second.get();
    MaxwellShaderIR vs{ShaderStage::VERTEX,{},{}, "main"}, fs{ShaderStage::FRAGMENT,{},{}, "main"};
    auto p = createGraphicsPipeline(vs,fs,rp,0);
    auto* raw = p.get();
    graphics_pipelines_[key]=std::move(p);
    return raw;
}

CompiledPipeline* ShaderRecompiler::getOrCreateComputePipeline(uint64_t cs_hash) {
    auto it = compute_pipelines_.find(cs_hash);
    if(it!=compute_pipelines_.end()) return it->second.get();
    MaxwellShaderIR cs{ShaderStage::COMPUTE,{},{}, "main"};
    auto p = createComputePipeline(cs);
    auto* raw = p.get();
    compute_pipelines_[cs_hash]=std::move(p);
    return raw;
}

// ========== VULKAN GPU EXECUTOR (rascunho) ==========
VulkanGpuExecutor::VulkanGpuExecutor() {}
VulkanGpuExecutor::~VulkanGpuExecutor() { shutdown(); }

bool VulkanGpuExecutor::init(void* window_handle) {
    vk_ctx_ = std::make_unique<VulkanContext>();
    if(!vk_ctx_->init("MGD Odyssey", window_handle)) return false;
    recompiler_ = std::make_unique<ShaderRecompiler>(vk_ctx_.get());
    fb_mgr_ = std::make_unique<FramebufferManager>(512, 288, 1280, 720);
    painter_ = std::make_unique<PainterCompute>();
    if(!painter_->init(vk_ctx_.get(), fb_mgr_.get())) return false;
    asset_pipeline_ = std::make_unique<AssetPipeline>();
    return true;
}

void VulkanGpuExecutor::setAssetRegistry(core::AssetRegistry* registry, core::Infector* infector) {
    if (asset_pipeline_) asset_pipeline_->init(vk_ctx_.get(), registry, infector);
}

bool VulkanGpuExecutor::uploadAllAssets() {
    return asset_pipeline_ && asset_pipeline_->uploadAllAssets();
}

void VulkanGpuExecutor::shutdown() {
    if(vk_ctx_) vk_ctx_->waitIdle();
    asset_pipeline_.reset();
    painter_.reset();
    fb_mgr_.reset();
    recompiler_.reset();
    vk_ctx_.reset();
}

bool VulkanGpuExecutor::execute(const uint8_t* cmd_buf, size_t size) {
    if(!cmd_buf || size<4) return false;
    if(!vk_ctx_) return false;
    
    // Begin frame for rascunho
    fb_mgr_->beginFrame(state_.steps);
    vk_ctx_->beginFrame();
    
    // Execute rascunho command buffer
    size_t dwords = size/4;
    const uint32_t* cmds = reinterpret_cast<const uint32_t*>(cmd_buf);
    size_t parsed = MaxwellDecoder::parse(cmds, dwords, state_, shaders_, textures_, samplers_, render_targets_, unknown_ops_);
    bytes_executed_ += parsed*4;
    draw_calls_ += state_.draw_count;
    compute_dispatches_ += state_.compute_dispatch_count;
    state_.draw_count=0; state_.compute_dispatch_count=0;
    
    vk_ctx_->endFrame();
    
    // Execute Painter compute (rascunho -> final)
    if (painter_ && fb_mgr_) {
        auto* rough_hist = fb_mgr_->getCurrentHistory();
        auto* prev_hist = fb_mgr_->getPrevHistory();
        if (rough_hist && rough_hist->valid) {
            VkCommandBuffer cb = vk_ctx_->beginSingleTimeCommands();
            painter_->execute(rough_hist, prev_hist, cb);
            vk_ctx_->endSingleTimeCommands(cb);
        }
    }
    
    fb_mgr_->endFrame(
        rough_hist ? rough_hist->obj_id : std::vector<uint32_t>{},
        rough_hist ? rough_hist->depth : std::vector<uint16_t>{},
        std::vector<Vec3>{} // obj_positions - seria preenchido pelo rascunho real
    );
    
    return parsed>0;
}

uint32_t VulkanGpuExecutor::createTexture(uint32_t w, uint32_t h, TexFormat fmt, const uint8_t* data, size_t size) {
    uint32_t id = next_tex_id_++;
    VkFormat vkfmt = VK_FORMAT_R8G8B8A8_UNORM;
    if(fmt==TexFormat::RGBA16F) vkfmt = VK_FORMAT_R16G16B16A16_SFLOAT;
    else if(fmt==TexFormat::R8) vkfmt = VK_FORMAT_R8_UNORM;
    else if(fmt==TexFormat::D24S8) vkfmt = VK_FORMAT_D24_UNORM_S8_UINT;
    auto img = vk_ctx_ ? vk_ctx_->createImage(w,h,vkfmt, VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT) : nullptr;
    if(img) textures_[id]=std::move(img);
    (void)data; (void)size;
    return id;
}

uint32_t VulkanGpuExecutor::createSampler(uint32_t min_f, uint32_t mag_f, uint32_t wrap_s, uint32_t wrap_t) {
    uint32_t id = next_samp_id_++;
    VkFilter minF = min_f ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    VkFilter magF = mag_f ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    VkSamplerAddressMode wrap = (wrap_s==0) ? VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    auto s = vk_ctx_ ? vk_ctx_->createSampler(minF, magF, wrap) : std::make_unique<VulkanSampler>();
    if(s) samplers_[id]=std::move(s);
    (void)wrap_t;
    return id;
}

uint32_t VulkanGpuExecutor::createRenderTarget(uint32_t w, uint32_t h) {
    uint32_t id = next_rt_id_++;
    auto img = vk_ctx_ ? vk_ctx_->createImage(w,h,VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT) : std::make_unique<VulkanImage>();
    if(img) render_targets_[id]=std::move(img);
    return id;
}

uint32_t VulkanGpuExecutor::createShader(ShaderStage stage, const std::vector<uint32_t>& bytecode) {
    uint32_t id = next_shader_id_++;
    Shader s; s.stage = stage; s.bytecode = bytecode;
    shaders_[id]=s;
    // compila para SPIR-V via recompiler (cache)
    MaxwellShaderIR ir{stage, bytecode, {}, "main"};
    std::vector<uint32_t> spirv;
    if(recompiler_) recompiler_->compileShader(ir, spirv);
    return id;
}

} // namespace gpu
} // namespace mgd
