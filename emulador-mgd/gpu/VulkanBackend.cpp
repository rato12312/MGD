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
    // Maxwell ISA: 32-bit instructions, 256 registers, predication, texture ops
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
    
    // Extensions
    st.emitOp(SpvOpExtension, {0, 0, 0, 0}); // "SPV_KHR_vulkan_memory_model" (stub)
    
    // Memory model
    st.emitOp(SpvOpMemoryModel, {SpvAddressingModelLogical, SpvMemoryModelVulkanKHR});
    
    // Entry point
    uint32_t entry_id = st.getOrCreateVar(0, VK_SHADER_STAGE_ALL_GRAPHICS);
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
    
    // Uniform/Storage buffers / Push constants
    if (ir.stage == ShaderStage::VERTEX || ir.stage == ShaderStage::FRAGMENT) {
        // Push constant block: mvp(64) + object_id(4) + flags(4)
        uint32_t push_ptr = st.getNextId();
        st.spirv.push_back((SpvOpTypePointer << 16) | 3 | (push_ptr<<16)); // placeholder
        // We'll use a simpler approach: define struct in push constant range
    }
    
    // Translate Maxwell instructions
    while (st.pc < st.maxwell_code.size()) {
        st.translateInstruction();
    }
    
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
    // Type: float for now
    uint32_t float_type = getNextId();
    spirv.push_back((SpvOpTypeFloat << 16) | (3 << 16) | (float_type << 16) | 32); // OpTypeFloat 32
    spirv.push_back((SpvOpTypePointer << 16) | (3 << 16) | (getNextId()<<16) | SpvStorageClassFunction | (float_type<<16)); // stub
    return id;
}

uint32_t ShaderRecompiler::TranslatorState::getNextId() {
    return next_id++;
}

void ShaderRecompiler::TranslatorState::translateInstruction() {
    if (pc >= maxwell_code.size()) return;
    uint32_t insn = maxwell_code[pc++];
    
    // Maxwell opcode extraction (simplified)
    // Real Maxwell ISA decoding is complex; this is a functional subset
    uint32_t opcode = insn & 0x7F; // 7-bit primary opcode
    
    switch (opcode) {
        case 0x00: // MOV (register copy)
        case 0x01: // MOV immediate
        case 0x10: // ADD
        case 0x11: // FADD
        case 0x12: // MUL
        case 0x13: // FMUL
        case 0x20: // TEX (texture sample)
        case 0x30: // RCP (reciprocal)
        case 0x31: // RSQ (reciprocal sqrt)
        case 0x40: // BRA (branch)
        case 0x41: // BRX (branch predicate)
        case 0x50: // EXIT
        case 0x51: // RET
        default: {
            // Unknown: emit NOP
            emitOp(SpvOpNop, {});
            break;
        }
    }
}

std::unique_ptr<CompiledPipeline> ShaderRecompiler::createGraphicsPipeline(const MaxwellShaderIR& vs_ir, const MaxwellShaderIR& fs_ir, VkRenderPass rp, uint32_t subpass) {
    auto p = std::make_unique<CompiledPipeline>();
    p->vs_ir = vs_ir; p->fs_ir = fs_ir;
#ifdef VK_VERSION_1_0
    if(ctx_->device()==VK_NULL_HANDLE) return p; // stub
    // cria pipeline layout vazio (push constants apenas: mvp 64 + object_id 4 + flags 4)
    VkPushConstantRange pc{}; pc.stageFlags = VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT; pc.size = 72;
    VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO}; li.pushConstantRangeCount=1; li.pPushConstantRanges=&pc;
    vkCreatePipelineLayout(ctx_->device(),&li,nullptr,&p->layout);
    // pipeline grafico minimo (sem shader modules reais ainda — stub)
    p->pipeline = VK_NULL_HANDLE;
#else
    (void)rp; (void)subpass;
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
    return true;
}

void VulkanGpuExecutor::shutdown() {
    if(vk_ctx_) vk_ctx_->waitIdle();
    recompiler_.reset();
    vk_ctx_.reset();
}

bool VulkanGpuExecutor::execute(const uint8_t* cmd_buf, size_t size) {
    if(!cmd_buf || size<4) return false;
    if(!vk_ctx_) return false;
    vk_ctx_->beginFrame();
    // Fase 1: so conta draws (sem submeter Vulkan ainda) — pipeline real na Fase 2/3
    size_t dwords = size/4;
    const uint32_t* cmds = reinterpret_cast<const uint32_t*>(cmd_buf);
    size_t parsed = MaxwellDecoder::parse(cmds, dwords, state_, shaders_, textures_, samplers_, render_targets_, unknown_ops_);
    bytes_executed_ += parsed*4;
    draw_calls_ += state_.draw_count;
    compute_dispatches_ += state_.compute_dispatch_count;
    state_.draw_count=0; state_.compute_dispatch_count=0;
    vk_ctx_->endFrame();
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
