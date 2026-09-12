#include "VulkanBackend.h"
#include <cstring>
#include <vector>
#include <array>
#include "core/common/Vec3.h"

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

bool VulkanContext::initFromExisting(VkDevice device, VkPhysicalDevice physical_device,
                                     VkQueue graphics_queue, VkQueue present_queue,
                                     VkSurfaceKHR surface, VkSwapchainKHR swapchain) {
    #ifdef VK_VERSION_1_0
    device_ = device;
    physical_device_ = physical_device;
    graphics_queue_ = graphics_queue;
    present_queue_ = present_queue;
    surface_ = surface;
    swapchain_ = swapchain;
    
    // Descobre queue families
    uint32_t qn = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &qn, nullptr);
    std::vector<VkQueueFamilyProperties> qs(qn);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &qn, qs.data());
    for (uint32_t i = 0; i < qn; i++) {
        if (qs[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphics_queue_family_ = i;
            compute_queue_family_ = i;
            break;
        }
    }
    
    // Cria render pass, framebuffers, command pool, etc.
    if (!createRenderPass()) return false;
    if (!createFramebuffers()) return false;
    if (!createCommandPool()) return false;
    if (!createCommandBuffers()) return false;
    if (!createSyncObjects()) return false;
    
    return true;
    #else
    return true;
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
bool VulkanContext::createSwapchain() {
#ifdef VK_VERSION_1_0
    if(device_==VK_NULL_HANDLE) return true;
    
    // Swapchain para apresentação na tela (full resolution)
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device_, surface_, &caps);
    
    VkFormat swapchain_format = VK_FORMAT_B8G8R8A8_SRGB;
    VkColorSpaceKHR color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    
    VkExtent2D extent = {finalW_, finalH_};
    if (caps.currentExtent.width != UINT32_MAX) {
        extent = caps.currentExtent;
    } else {
        extent = {finalW_, finalH_};
    }
    
    uint32_t image_count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && image_count > caps.maxImageCount) {
        image_count = caps.maxImageCount;
    }
    
    VkSwapchainCreateInfoKHR ci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    ci.surface = surface_;
    ci.minImageCount = image_count;
    ci.imageFormat = swapchain_format;
    ci.imageColorSpace = color_space;
    ci.imageExtent = extent;
    ci.imageArrayLayers = 1;
    ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.preTransform = caps.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode = VK_PRESENT_MODE_FIFO_KHR; // VSync
    ci.clipped = VK_TRUE;
    ci.oldSwapchain = swapchain_;
    
    VkSwapchainKHR old_swapchain = swapchain_;
    if (vkCreateSwapchainKHR(device_, &ci, nullptr, &swapchain_) != VK_SUCCESS) {
        return false;
    }
    
    // Destroy old swapchain
    if (old_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device_, old_swapchain, nullptr);
    }
    
    // Get swapchain images
    uint32_t image_count = 0;
    vkGetSwapchainImagesKHR(device_, swapchain_, &image_count, nullptr);
    swapchain_images_.resize(image_count);
    vkGetSwapchainImagesKHR(device_, swapchain_, &image_count, swapchain_images_.data());
    
    // Create image views
    swapchain_image_views_.resize(image_count);
    for (uint32_t i = 0; i < image_count; i++) {
        VkImageViewCreateInfo iv{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        iv.image = swapchain_images_[i];
        iv.viewType = VK_IMAGE_VIEW_TYPE_2D;
        iv.format = swapchain_format_;
        iv.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        iv.subresourceRange.levelCount = 1;
        iv.subresourceRange.layerCount = 1;
        if (vkCreateImageView(device_, &iv, nullptr, &swapchain_image_views_[i]) != VK_SUCCESS) {
            return false;
        }
    }
    
    return true;
#else
    return true;
#endif
}
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
    
    // Se temos swapchain images, usa elas para framebuffers de apresentação
    if (!swapchain_images_.empty()) {
        framebuffers_.resize(swapchain_images_.size());
        for (size_t i = 0; i < swapchain_images_.size(); i++) {
            VkImageView attachments[] = {swapchain_image_views_[i]};
            VkFramebufferCreateInfo fbci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
            fbci.renderPass = render_pass_;
            fbci.attachmentCount = 1;
            fbci.pAttachments = &swapchain_image_views_[i];
            fbci.width = swapchain_extent_.width;
            fbci.height = swapchain_extent_.height;
            fbci.layers = 1;
            VkFramebuffer fb;
            if (vkCreateFramebuffer(device_, &fbci, nullptr, &fb) != VK_SUCCESS) return false;
            framebuffers_.push_back(fb);
        }
        return true;
    }
    
    // Fallback: offscreen rascunho (para rascunho 0.4x)
    auto color = createImage(roughW_, roughH_, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT);
    auto depth = createImage(roughW_, roughH_, VK_FORMAT_D16_UNORM, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    if(!color||!depth) return false;
    VkImageView views[2] = {color->view, depth->view};
    VkFramebufferCreateInfo fbci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fbci.renderPass = render_pass_; fbci.attachmentCount=2; fbci.pAttachments=views; fbci.width=roughW_; fbci.height=roughH_; fbci.layers=1;
    VkFramebuffer fb; if(vkCreateFramebuffer(device_,&fbci,nullptr,&fb)!=VK_SUCCESS) return false;
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

void VulkanContext::setResolution(uint32_t roughW, uint32_t roughH, uint32_t finalW, uint32_t finalH) {
    roughW_ = roughW; roughH_ = roughH; finalW_ = finalW; finalH_ = finalH;
#ifdef VK_VERSION_1_0
    if(device_ != VK_NULL_HANDLE) {
        // Se temos swapchain, recria com nova resolução
        if (!swapchain_images_.empty()) {
            recreateSwapchain();
        } else {
            for(auto fb: framebuffers_) vkDestroyFramebuffer(device_, fb, nullptr);
            framebuffers_.clear();
            createFramebuffers();
        }
    }
#endif
}
}

bool VulkanContext::recreateSwapchain() {
#ifdef VK_VERSION_1_0
    if(device_ == VK_NULL_HANDLE) return true;
    
    // Destroi framebuffers antigos
    for(auto fb: framebuffers_) vkDestroyFramebuffer(device_, fb, nullptr);
    framebuffers_.clear();
    
    // Destroi image views antigas
    for(auto view : swapchain_image_views_) vkDestroyImageView(device_, view, nullptr);
    swapchain_image_views.clear();
    
    // Destroi swapchain antigo
    if (swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
    
    // Recria swapchain
    if (!createSwapchain()) return false;
    
    // Recria framebuffers
    if (!createFramebuffers()) return false;
    
    return true;
#else
    return true;
#endif
}

// SPIR-V opcode constants (minimal subset)
enum SpvOp : uint32_t {
    SpvOpNop = 0,
    SpvOpCapability = 17,
    SpvOpExtension = 18,
    SpvOpMemoryModel = 19,
    SpvOpEntryPoint = 20,
    SpvOpExecutionMode = 21,
    SpvOpName = 22,
    SpvOpTypeVoid = 192,
    SpvOpTypeBool = 193,
    SpvOpTypeInt = 194,
    SpvOpTypeFloat = 195,
    SpvOpTypeVector = 196,
    SpvOpTypeMatrix = 197,
    SpvOpTypeSampler = 198,
    SpvOpTypeImage = 199,
    SpvOpTypeSampledImage = 200,
    SpvOpTypeStruct = 201,
    SpvOpTypePointer = 202,
    SpvOpTypeFunction = 203,
    SpvOpConstant = 204,
    SpvOpVariable = 205,
    SpvOpDecorate = 71,
    SpvOpFunction = 215,
    SpvOpLabel = 216,
    SpvOpReturn = 217,
    SpvOpFunctionEnd = 218,
    SpvOpLoad = 63,
    SpvOpStore = 64,
    SpvOpIAdd = 67,
    SpvOpISub = 68,
    SpvOpIMul = 69,
    SpvOpFAdd = 70,
    SpvOpFSub = 71,
    SpvOpFMul = 72,
    SpvOpFDiv = 73,
    SpvOpSqrt = 74,
    SpvOpSin = 75,
    SpvOpCos = 76,
    SpvOpLog2 = 77,
    SpvOpExp2 = 78,
    SpvOpConvertFToS = 104,
    SpvOpConvertSToF = 105,
    SpvOpConvertFToU = 106,
    SpvOpConvertUToF = 107,
    SpvOpBitwiseAnd = 87,
    SpvOpBitwiseOr = 88,
    SpvOpBitwiseXor = 89,
    SpvOpBitwiseNot = 90,
    SpvOpShiftLeftLogical = 91,
    SpvOpShiftRightLogical = 92,
    SpvOpShiftRightArithmetic = 93,
    SpvOpFOrdEqual = 94,
    SpvOpFOrdNotEqual = 95,
    SpvOpFOrdLessThan = 96,
    SpvOpFOrdGreaterThan = 97,
    SpvOpFOrdLessThanEqual = 98,
    SpvOpFOrdGreaterThanEqual = 99,
    SpvOpImageSampleImplicitLod = 83,
    SpvOpImageFetch = 85,
    SpvOpBranch = 219,
    SpvOpBranchConditional = 220,
    SpvOpKill = 221,
    SpvOpAtomicIAdd = 127,
    SpvOpMemoryBarrier = 222,
    SpvOpControlBarrier = 223,
};

enum SpvCapability : uint32_t {
    SpvCapabilityShader = 0,
    SpvCapabilityGeometry = 1,
    SpvCapabilityKernel = 10,
    SpvCapabilityImageQuery = 11,
    SpvCapabilitySampleRateShading = 12,
    SpvCapabilitySampled1D = 13,
    SpvCapabilitySampled2D = 14,
    SpvCapabilitySampled3D = 15,
    SpvCapabilitySampledCube = 16,
    SpvCapabilitySampledBuffer = 17,
    SpvCapabilityStorageImage = 18,
    SpvCapabilityImageBuffer = 19,
    SpvCapabilityStorageBuffer = 20,
    SpvCapabilityClipDistance = 21,
    SpvCapabilityCullDistance = 22,
};

enum SpvAddressingModel : uint32_t {
    SpvAddressingModelLogical = 0,
};

enum SpvMemoryModel : uint32_t {
    SpvMemoryModelVulkanKHR = 2,
};

enum SpvExecutionMode : uint32_t {
    SpvExecutionModeOriginUpperLeft = 4,
    SpvExecutionModeDepthReplacing = 5,
    SpvExecutionModeLocalSize = 6,
};

enum SpvStorageClass : uint32_t {
    SpvStorageClassInput = 0,
    SpvStorageClassOutput = 1,
    SpvStorageClassUniform = 2,
    SpvStorageClassUniformConstant = 3,
    SpvStorageClassPushConstant = 4,
    SpvStorageClassFunction = 5,
};

enum SpvBuiltIn : uint32_t {
    SpvBuiltInPosition = 0,
    SpvBuiltInFragDepth = 1,
};

enum SpvDecoration : uint32_t {
    SpvDecorationLocation = 0,
    SpvDecorationBinding = 1,
    SpvDecorationDescriptorSet = 2,
    SpvDecorationBuiltIn = 3,
};

enum SpvDim : uint32_t {
    SpvDim1D = 0,
    SpvDim2D = 1,
    SpvDim3D = 2,
    SpvDimCube = 3,
};

enum SpvScope : uint32_t {
    SpvScopeWorkgroup = 2,
    SpvScopeDevice = 3,
};

enum SpvMemorySemantics : uint32_t {
    SpvMemorySemanticsAcquireReleaseMask = 0x6,
};

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
    st.void_type = st.getNextId();
    st.spirv.push_back((SpvOpTypeVoid << 16) | (2 << 16) | (st.void_type << 16));
    
    st.bool_type = st.getNextId();
    st.spirv.push_back((SpvOpTypeBool << 16) | (2 << 16) | (st.bool_type << 16));
    
    st.int32_type = st.getNextId();
    st.spirv.push_back((SpvOpTypeInt << 16) | (3 << 16) | (st.int32_type << 16) | 32 | (1 << 16)); // signed 32-bit
    
    st.uint32_type = st.getNextId();
    st.spirv.push_back((SpvOpTypeInt << 16) | (3 << 16) | (st.uint32_type << 16) | 32 | (0 << 16)); // unsigned 32-bit
    
    st.int16_type = st.getNextId();
    st.spirv.push_back((SpvOpTypeInt << 16) | (3 << 16) | (st.int16_type << 16) | 16 | (1 << 16));
    
    st.float32_type = st.getNextId();
    st.spirv.push_back((SpvOpTypeFloat << 16) | (3 << 16) | (st.float32_type << 16) | 32);
    
    st.float16_type = st.getNextId();
    st.spirv.push_back((SpvOpTypeFloat << 16) | (3 << 16) | (st.float16_type << 16) | 16);
    
    // Vector types
    auto makeVectorType = [&](uint32_t base, int count) {
        uint32_t vec_type = st.getNextId();
        st.spirv.push_back((SpvOpTypeVector << 16) | (3 << 16) | (vec_type << 16) | (base << 16) | (count & 0xFFFF));
        return vec_type;
    };
    
    st.vec2f = makeVectorType(st.float32_type, 2);
    st.vec3f = makeVectorType(st.float32_type, 3);
    st.vec4f = makeVectorType(st.float32_type, 4);
    st.vec2i = makeVectorType(st.int32_type, 2);
    st.vec3i = makeVectorType(st.int32_type, 3);
    st.vec4i = makeVectorType(st.int32_type, 4);
    st.vec2u = makeVectorType(st.uint32_type, 2);
    st.vec3u = makeVectorType(st.uint32_type, 3);
    st.vec4u = makeVectorType(st.uint32_type, 4);
    
    // Matrix types
    auto makeMatrixType = [&](uint32_t vec_type, int columns) {
        uint32_t mat_type = st.getNextId();
        st.spirv.push_back((SpvOpTypeMatrix << 16) | (3 << 16) | (mat_type << 16) | (vec_type << 16) | (columns & 0xFFFF));
        return mat_type;
    };
    
    st.mat2x2 = makeMatrixType(st.vec2f, 2);
    st.mat3x3 = makeMatrixType(st.vec3f, 3);
    st.mat4x4 = makeMatrixType(st.vec4f, 4);
    st.mat2x3 = makeMatrixType(st.vec2f, 3);
    st.mat3x2 = makeMatrixType(st.vec3f, 2);
    st.mat2x4 = makeMatrixType(st.vec2f, 4);
    st.mat4x2 = makeMatrixType(st.vec4f, 2);
    st.mat3x4 = makeMatrixType(st.vec3f, 4);
    st.mat4x3 = makeMatrixType(st.vec4f, 3);
    
    // Sampler type
    st.sampler_type = st.getNextId();
    st.spirv.push_back((SpvOpTypeSampler << 16) | (2 << 16) | (st.sampler_type << 16));
    
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
        st.spirv.push_back((SpvOpTypeImage << 16) | (9 << 16) | (img_type << 16) | (st.float32_type << 16) | (dim_val << 16) | (depth_val << 16) | (array_val << 16) | (ms_val << 16) | (sampled << 16) | (format << 16));
        return img_type;
    };
    
    st.img2d = makeImageType(2, false, false, false);
    st.img2d_array = makeImageType(2, false, true, false);
    st.img3d = makeImageType(3, false, false, false);
    st.imgcube = makeImageType(0, false, false, false);
    st.img2d_depth = makeImageType(2, true, false, false);
    st.img2d_ms = makeImageType(2, false, false, true);
    
    // Sampler
    st.spirv.push_back((SpvOpTypeSampler << 16) | (2 << 16) | (st.sampler_type << 16));
    
    // Sampled image types
    auto makeSampledImage = [&](uint32_t img_type) {
        uint32_t sampled_type = st.getNextId();
        st.spirv.push_back((SpvOpTypeSampledImage << 16) | (3 << 16) | (sampled_type << 16) | (img_type << 16));
        return sampled_type;
    };
    
    st.sampled_img2d = makeSampledImage(st.img2d);
    st.sampled_img2d_array = makeSampledImage(st.img2d_array);
    st.sampled_img3d = makeSampledImage(st.img3d);
    st.sampled_imgcube = makeSampledImage(st.imgcube);
    st.sampled_img2d_depth = makeSampledImage(st.img2d_depth);
    
    // Storage image types (for compute)
    auto makeStorageImage = [&](uint32_t img_type, uint32_t format) {
        uint32_t storage_type = st.getNextId();
        st.spirv.push_back((SpvOpTypeImage << 16) | (9 << 16) | (storage_type << 16) | (format << 16) | (0 << 16) | (0 << 16) | (0 << 16) | (0 << 16) | (2 << 16) | (0 << 16)); // Storage
        return storage_type;
    };
    
    st.storage_img2d_rgba8 = makeStorageImage(st.img2d, 0); // rgba8
    st.storage_img2d_rgba16f = makeStorageImage(st.img2d, 0); // rgba16f
    st.storage_img2d_r32f = makeStorageImage(st.img2d, 0); // r32f
    
    // Sampler
    st.spirv.push_back((SpvOpTypeSampler << 16) | (2 << 16) | (st.sampler_type << 16));
    
    // Struct for push constants (MVP + object_id + flags + lod + pad)
    // MVP mat4x4 (64 bytes) + object_id(4) + flags(4) + lod(4) + pad(4) = 80 bytes
    st.push_constant_struct = st.getNextId();
    std::vector<uint32_t> push_members = {st.mat4x4, st.uint32_type, st.uint32_type, st.uint32_type, st.uint32_type};
    st.spirv.push_back((SpvOpTypeStruct << 16) | ((push_members.size() + 1) << 16) | (st.push_constant_struct << 16));
    for (uint32_t m : push_members) st.spirv.push_back(m);
    
    // Push constant pointer
    st.push_ptr_type = st.getNextId();
    st.spirv.push_back((SpvOpTypePointer << 16) | (4 << 16) | (st.push_ptr_type << 16) | (SpvStorageClassPushConstant << 16) | (st.push_constant_struct << 16));
    
    // Push constant variable
    st.push_var = st.getNextId();
    st.spirv.push_back((SpvOpVariable << 16) | (4 << 16) | (st.push_var << 16) | (st.push_ptr_type << 16) | (SpvStorageClassPushConstant << 16));
    st.emitOp(SpvOpName, {st.push_var, 'P','C',0});
    
    // ===== Interface Variables (Vertex/Fragment I/O) =====
    // Vertex inputs
    st.in_pos = st.getNextId();
    st.spirv.push_back((SpvOpVariable << 16) | (5 << 16) | (st.in_pos << 16) | (st.vec3f << 16) | (SpvStorageClassInput << 16));
    st.emitOp(SpvOpName, {st.in_pos, 'i','n','_','p','o','s',0});
    st.emitOp(SpvOpDecorate, {st.in_pos, SpvDecorationLocation, 0});
    
    // Vertex outputs / Fragment inputs
    st.out_pos = st.getNextId();
    st.spirv.push_back((SpvOpVariable << 16) | (5 << 16) | (st.out_pos << 16) | (st.vec4f << 16) | (SpvStorageClassOutput << 16));
    st.emitOp(SpvOpName, {st.out_pos, 'g','l','_','P','o','s','i','t','i','o','n',0});
    st.emitOp(SpvOpDecorate, {st.out_pos, SpvDecorationBuiltIn, SpvBuiltInPosition});
    
    st.out_obj_id = st.getNextId();
    st.spirv.push_back((SpvOpVariable << 16) | (5 << 16) | (st.out_obj_id << 16) | (st.uint32_type << 16) | (SpvStorageClassOutput << 16));
    st.emitOp(SpvOpName, {st.out_obj_id, 'o','u','t','_','o','b','j','_','i','d',0});
    st.emitOp(SpvOpDecorate, {st.out_obj_id, SpvDecorationLocation, 1});
    
    // Fragment outputs
    st.out_color = st.getNextId();
    st.spirv.push_back((SpvOpVariable << 16) | (5 << 16) | (st.out_color << 16) | (st.vec4f << 16) | (SpvStorageClassOutput << 16));
    st.emitOp(SpvOpName, {st.out_color, 'o','u','t','_','c','o','l','o','r',0});
    st.emitOp(SpvOpDecorate, {st.out_color, SpvDecorationLocation, 0});
    
    st.out_depth = st.getNextId();
    st.spirv.push_back((SpvOpVariable << 16) | (5 << 16) | (st.out_depth << 16) | (st.float32_type << 16) | (SpvStorageClassOutput << 16));
    st.emitOp(SpvOpName, {st.out_depth, 'o','u','t','_','d','e','p','t','h',0});
    st.emitOp(SpvOpDecorate, {st.out_depth, SpvDecorationBuiltIn, SpvBuiltInFragDepth});
    
    st.out_obj_id_frag = st.getNextId();
    st.spirv.push_back((SpvOpVariable << 16) | (5 << 16) | (st.out_obj_id_frag << 16) | (st.uint32_type << 16) | (SpvStorageClassOutput << 16));
    st.emitOp(SpvOpName, {st.out_obj_id_frag, 'o','u','t','_','o','b','j','_','i','d',0});
    st.emitOp(SpvOpDecorate, {st.out_obj_id_frag, SpvDecorationLocation, 1});
    
    // Descriptor set bindings (textures/samplers)
    st.rough_color_var = st.getNextId();
    st.spirv.push_back((SpvOpVariable << 16) | (5 << 16) | (st.rough_color_var << 16) | (st.sampled_img2d << 16) | (SpvStorageClassUniformConstant << 16));
    st.emitOp(SpvOpName, {st.rough_color_var, 'r','o','u','g','h','_','c','o','l','o','r',0});
    st.emitOp(SpvOpDecorate, {st.rough_color_var, SpvDecorationDescriptorSet, 0});
    st.emitOp(SpvOpDecorate, {st.rough_color_var, SpvDecorationBinding, 0});
    
    st.rough_depth_var = st.getNextId();
    st.spirv.push_back((SpvOpVariable << 16) | (5 << 16) | (st.rough_depth_var << 16) | (st.img2d_depth << 16) | (SpvStorageClassUniformConstant << 16));
    st.emitOp(SpvOpName, {st.rough_depth_var, 'r','o','u','g','h','_','d','e','p','t','h',0});
    st.emitOp(SpvOpDecorate, {st.rough_depth_var, SpvDecorationDescriptorSet, 0});
    st.emitOp(SpvOpDecorate, {st.rough_depth_var, SpvDecorationBinding, 1});
    
    st.rough_obj_id_var = st.getNextId();
    st.spirv.push_back((SpvOpVariable << 16) | (5 << 16) | (st.rough_obj_id_var << 16) | (st.sampled_img2d << 16) | (SpvStorageClassUniformConstant << 16));
    st.emitOp(SpvOpName, {st.rough_obj_id_var, 'r','o','u','g','h','_','o','b','j','_','i','d',0});
    st.emitOp(SpvOpDecorate, {st.rough_obj_id_var, SpvDecorationDescriptorSet, 0});
    st.emitOp(SpvOpDecorate, {st.rough_obj_id_var, SpvDecorationBinding, 2});
    
    st.prev_frame_var = st.getNextId();
    st.spirv.push_back((SpvOpVariable << 16) | (5 << 16) | (st.prev_frame_var << 16) | (st.sampled_img2d << 16) | (SpvStorageClassUniformConstant << 16));
    st.emitOp(SpvOpName, {st.prev_frame_var, 'p','r','e','v','_','f','r','a','m','e',0});
    st.emitOp(SpvOpDecorate, {st.prev_frame_var, SpvDecorationDescriptorSet, 0});
    st.emitOp(SpvOpDecorate, {st.prev_frame_var, SpvDecorationBinding, 3});
    
    st.motion_vectors_var = st.getNextId();
    st.spirv.push_back((SpvOpVariable << 16) | (5 << 16) | (st.motion_vectors_var << 16) | (st.sampled_img2d << 16) | (SpvStorageClassUniformConstant << 16));
    st.emitOp(SpvOpName, {st.motion_vectors_var, 'm','o','t','i','o','n','_','v','e','c','t','o','r','s',0});
    st.emitOp(SpvOpDecorate, {st.motion_vectors_var, SpvDecorationDescriptorSet, 0});
    st.emitOp(SpvOpDecorate, {st.motion_vectors_var, SpvDecorationBinding, 4});
    
    st.prev_obj_id_var = st.getNextId();
    st.spirv.push_back((SpvOpVariable << 16) | (5 << 16) | (st.prev_obj_id_var << 16) | (st.sampled_img2d << 16) | (SpvStorageClassUniformConstant << 16));
    st.emitOp(SpvOpName, {st.prev_obj_id_var, 'p','r','e','v','_','o','b','j','_','i','d',0});
    st.emitOp(SpvOpDecorate, {st.prev_obj_id_var, SpvDecorationDescriptorSet, 0});
    st.emitOp(SpvOpDecorate, {st.prev_obj_id_var, SpvDecorationBinding, 5});
    
    // Output image (storage image for compute shader output)
    st.output_image_var = st.getNextId();
    st.spirv.push_back((SpvOpVariable << 16) | (5 << 16) | (st.output_image_var << 16) | (st.storage_img2d_rgba8 << 16) | (SpvStorageClassUniform << 16));
    st.emitOp(SpvOpName, {st.output_image_var, 'o','u','t','_','i','m','a','g','e',0});
    st.emitOp(SpvOpDecorate, {st.output_image_var, SpvDecorationDescriptorSet, 0});
    st.emitOp(SpvOpDecorate, {st.output_image_var, SpvDecorationBinding, 6});
    
    // Push constant variable
    st.push_var2 = st.getNextId();
    st.spirv.push_back((SpvOpVariable << 16) | (4 << 16) | (st.push_var2 << 16) | (st.push_ptr_type << 16) | (SpvStorageClassPushConstant << 16));
    st.emitOp(SpvOpName, {st.push_var2, 'P','C',0});
    
    // Function type for main
    st.void_func_type = st.getNextId();
    st.spirv.push_back((SpvOpTypeFunction << 16) | (3 << 16) | (st.void_func_type << 16) | (st.void_type << 16));
    
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
    st.emitOp(SpvOpName, {st.void_type, 'v','o','i','d',0});
    st.emitOp(SpvOpName, {st.bool_type, 'b','o','o','l',0});
    st.emitOp(SpvOpName, {st.int32_type, 'i','n','t',0});
    st.emitOp(SpvOpName, {st.uint32_type, 'u','i','n','t',0});
    st.emitOp(SpvOpName, {st.float32_type, 'f','l','o','a','t',0});
    st.emitOp(SpvOpName, {st.float16_type, 'h','a','l','f',0});
    st.emitOp(SpvOpName, {st.vec2f, 'v','e','c','2',0});
    st.emitOp(SpvOpName, {st.vec3f, 'v','e','c','3',0});
    st.emitOp(SpvOpName, {st.vec4f, 'v','e','c','4',0});
    st.emitOp(SpvOpName, {st.mat4x4, 'm','a','t','4',0});
    st.emitOp(SpvOpName, {st.sampler_type, 's','a','m','p','l','e','r',0});
    st.emitOp(SpvOpName, {st.img2d, 'i','m','g','2','d',0});
    
    // Function main
    st.main_label = st.getNextId();
    st.spirv.push_back((SpvOpFunction << 16) | (4 << 16) | (st.void_type << 16) | (entry_id << 16) | (st.void_func_type << 16) | (0 << 16));
    st.spirv.push_back((SpvOpLabel << 16) | (2 << 16) | (st.main_label << 16));
    
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

uint32_t ShaderRecompiler::TranslatorState::getOrCreateVar(uint32_t maxwell_reg) {
    auto it = reg_to_id.find(maxwell_reg);
    if (it != reg_to_id.end()) return it->second;
    uint32_t id = getNextId();
    reg_to_id[maxwell_reg] = id;
    // Type: float for now (default) - Maxwell registers are typeless but we default to float
    uint32_t float_type = getNextId();
    spirv.push_back((SpvOpTypeFloat << 16) | (3 << 16) | (float_type << 16) | 32); // OpTypeFloat 32
    uint32_t ptr_type = getNextId();
    spirv.push_back((SpvOpTypePointer << 16) | (4 << 16) | (ptr_type << 16) | (SpvStorageClassFunction << 16) | (float_type << 16));
    return id;
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
    
    auto getVar = [this](uint32_t reg) -> uint32_t {
        return getOrCreateVar(reg);
    };
    
    auto getConst = [this](float val) -> uint32_t {
        uint32_t id = getNextId();
        spirv.push_back((SpvOpConstant << 16) | (4 << 16) | (float32_type << 16) | (id << 16));
        uint32_t bits = *reinterpret_cast<const uint32_t*>(&val);
        spirv.push_back(bits);
        return id;
    };
    
    auto getConstInt = [this](int32_t val) -> uint32_t {
        uint32_t id = getNextId();
        spirv.push_back((SpvOpConstant << 16) | (4 << 16) | (int32_type << 16) | (id << 16));
        spirv.push_back(static_cast<uint32_t>(val));
        return id;
    };
    
    auto getConstUInt = [this](uint32_t val) -> uint32_t {
        uint32_t id = getNextId();
        spirv.push_back((SpvOpConstant << 16) | (4 << 16) | (uint32_type << 16) | (id << 16));
        spirv.push_back(val);
        return id;
    };
    
    auto getReg = [this](uint32_t reg) -> uint32_t {
        return getOrCreateVar(reg);
    };
    
    auto emitBinaryOp = [this](uint32_t spv_op, uint32_t dst, uint32_t src_a, uint32_t src_b) {
        uint32_t result = getNextId();
        emitOp(spv_op, {float32_type, result, getVar(src_a), getVar(src_b)});
        // Store result in destination register
        reg_to_id[dst] = result;
    };
    
    auto emitUnaryOp = [this](uint32_t spv_op, uint32_t dst, uint32_t src) {
        uint32_t result = getNextId();
        emitOp(spv_op, {float32_type, result, getVar(src)});
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
                emitOp(SpvOpIMul, {int32_type, getNextId(), getVar(src0), getVar(src1)});
                emitOp(SpvOpIAdd, {int32_type, getNextId(), mul, getVar(src2)});
                reg_to_id[dst] = mul;
            }
            break;
        case 0x15: // IMUL immediate
            emitBinaryOp(SpvOpIMul, dst, src0, getConst(static_cast<float>(simm8)));
            break;
        case 0x16: // IMAD immediate
            break;
            
        // ===== Floating Point Arithmetic =====
        // Note: Maxwell uses same opcodes for int/float with type qualifiers
        // For simplicity, we handle float variants here (opcodes with different encoding)
        case 0x20: // FADD (float add - different encoding)
        case 0x21: // FADD immediate (float)
            emitBinaryOp(SpvOpFAdd, dst, src0, src1);
            break;
        case 0x22: // FSUB (float)
        case 0x23: // FSUB immediate (float)
            emitBinaryOp(SpvOpFSub, dst, src0, src1);
            break;
        case 0x24: // FMUL (float)
        case 0x25: // FMUL immediate (float)
            emitBinaryOp(SpvOpFMul, dst, src0, src1);
            break;
        case 0x26: // FMAD (fused multiply-add)
            {
                uint32_t mul = getNextId();
                emitOp(SpvOpFMul, {float32_type, getNextId(), getVar(src0), getVar(src1)});
                emitOp(SpvOpFAdd, {float32_type, getNextId(), mul, getVar(src2)});
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
                emitOp(SpvOpSqrt, {float32_type, getNextId(), getVar(src0)});
                uint32_t rcp = getNextId();
                emitOp(SpvOpFDiv, {float32_type, getNextId(), getConst(1.0f), src2});
                reg_to_id[dst] = rcp;
            }
            break;
        case 0x33: // SQRT
            emitUnaryOp(SpvOpSqrt, dst, src0);
            break;
        case 0x34: // RSQ
            {
                uint32_t sqrt_id = getNextId();
                emitOp(SpvOpSqrt, {float32_type, getNextId(), getVar(src0)});
                uint32_t rcp = getNextId();
                emitOp(SpvOpFDiv, {float32_type, getNextId(), getConst(1.0f), src2});
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
                emitOp(cmp_op, {bool_type, result, getVar(src0), getVar(src1)});
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
                emitOp(SpvOpImageSampleImplicitLod, {sampled_img2d, result, tex, coord, 0, 0});
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
                emitOp(SpvOpBranch, {target_label});
            }
            break;
        case 0x41: // BRX (branch predicate)
            {
                uint32_t pred = getVar(pred_reg);
                int32_t offset = simm20;
                // OpBranchConditional
                uint32_t true_label = getNextId();
                uint32_t false_label = getNextId();
                emitOp(SpvOpBranchConditional, {getVar(pred_reg), true_label, false_label, 0, 0});
            }
            break;
        case 0x42: // CALL
        case 0x43: // RET
            emitOp(SpvOpReturn, {});
            break;
        case 0x50: // EXIT
            emitOp(SpvOpKill, {});
            break;
        case 0x51: // RET
            emitOp(SpvOpReturn, {});
            break;
            
        // ===== Conversion =====
        case 0x60: // F2I (float to int)
            {
                uint32_t result = getNextId();
                emitOp(SpvOpConvertFToS, {int32_type, getNextId(), getVar(src0)});
                reg_to_id[dst] = result;
            }
            break;
        case 0x61: // I2F (int to float)
            {
                uint32_t result = getNextId();
                emitOp(SpvOpConvertSToF, {float32_type, getNextId(), getVar(src0)});
                reg_to_id[dst] = result;
            }
            break;
        case 0x61: // F2U
            {
                uint32_t result = getNextId();
                emitOp(SpvOpConvertFToU, {uint32_type, getNextId(), getVar(src0)});
                reg_to_id[dst] = result;
            }
            break;
        case 0x62: // U2F
            {
                uint32_t result = getNextId();
                emitOp(SpvOpConvertUToF, {float32_type, getNextId(), getVar(src0)});
                reg_to_id[dst] = result;
            }
            break;
            
// ===== Memory (LD/ST global/shared/local) =====
        case 0x90: // LD.E (load global)
        case 0x92: // LDG (load global with cache modifier)
        case 0x93: // LDG.CA (constant cache)
        case 0x93: // LDG.CG (global cache)
        case 0x94: // LDG.CS (streaming cache)
        case 0x95: // LDG.WT (write-through)
        case 0x96: // LDG.WB (write-back)
            {
                uint32_t result = getNextId();
                emitOp(SpvOpLoad, {uint32_type, result, getVar(src0)});
                reg_to_id[dst] = result;
            }
            break;
        case 0x91: // ST.E (store global)
        case 0x95: // STG (store global with cache)
        case 0x96: // STG.WB (write-back)
        case 0x97: // STG.WT (write-through)
            emitOp(SpvOpStore, {getVar(dst), getVar(src0)});
            break;
        case 0x92: // LDS (load shared)
            {
                uint32_t result = getNextId();
                emitOp(SpvOpLoad, {uint32_type, result, getVar(src0)});
                reg_to_id[dst] = result;
            }
            break;
        case 0x93: // STS (store shared)
            emitOp(SpvOpStore, {getVar(dst), getVar(src0)});
            break;
        case 0x94: // ATOM (atomic)
            {
                uint32_t result = getNextId();
                emitOp(SpvOpAtomicIAdd, {uint32_type, result, getVar(src0), getVar(src1), getVar(src0)});
                reg_to_id[dst] = result;
            }
            break;
        // ===== Atomic Operations (ATOMS) =====
        case 0x99: // ATOMS.ADD
            {
                uint32_t result = getNextId();
                emitOp(SpvOpAtomicIAdd, {uint32_type, result, getVar(src0), getVar(src1), getVar(src0)});
                reg_to_id[dst] = result;
            }
            break;
        case 0x9A: // ATOMS.CAS (compare-and-swap)
            {
                uint32_t result = getNextId();
                emitOp(SpvOpAtomicCompareExchange, {uint32_type, result, getVar(src0), getVar(src1), getVar(src2)});
                reg_to_id[dst] = result;
            }
            break;
        case 0x9B: // ATOMS.EXCH (exchange)
            {
                uint32_t result = getNextId();
                emitOp(SpvOpAtomicExchange, {uint32_type, result, getVar(src0), getVar(src1)});
                reg_to_id[dst] = result;
            }
            break;
        case 0x9C: // ATOMS.MIN
            {
                uint32_t result = getNextId();
                emitOp(SpvOpAtomicSMin, {int32_type, getNextId(), getVar(src0), getVar(src1)});
                reg_to_id[dst] = result;
            }
            break;
        case 0x9D: // ATOMS.MAX
            {
                uint32_t result = getNextId();
                emitOp(SpvOpAtomicSMax, {int32_type, getNextId(), getVar(src0), getVar(src1)});
                reg_to_id[dst] = result;
            }
            break;
        case 0x9E: // ATOMS.AND
            {
                uint32_t result = getNextId();
                emitOp(SpvOpAtomicAnd, {uint32_type, getNextId(), getVar(src0), getVar(src1)});
                reg_to_id[dst] = result;
            }
            break;
        case 0x9F: // ATOMS.OR
            {
                uint32_t result = getNextId();
                emitOp(SpvOpAtomicOr, {uint32_type, getNextId(), getVar(src0), getVar(src1)});
                reg_to_id[dst] = result;
            }
            break;
        case 0xA0: // ATOMS.XOR
            {
                uint32_t result = getNextId();
                emitOp(SpvOpAtomicXor, {uint32_type, getNextId(), getVar(src0), getVar(src1)});
                reg_to_id[dst] = result;
            }
            break;
        case 0xA1: // ATOMS.INC
            {
                uint32_t result = getNextId();
                emitOp(SpvOpAtomicIAdd, {uint32_type, result, getVar(src0), getConstUInt(1), getVar(src0)});
                reg_to_id[dst] = result;
            }
            break;
        case 0xA2: // ATOMS.DEC
            {
                uint32_t result = getNextId();
                emitOp(SpvOpAtomicISub, {uint32_type, result, getVar(src0), getConstInt(1), getVar(src0)});
                reg_to_id[dst] = result;
            }
            break;
        case 0x91: // ST.E (store global)
        case 0x95: // STG (store global with cache)
        case 0x96: // STG.WB (write-back)
        case 0x97: // STG.WT (write-through)
            emitOp(SpvOpStore, {getVar(dst), getVar(src0)});
            break;
        case 0x92: // LDS (load shared)
            {
                uint32_t result = getNextId();
                emitOp(SpvOpLoad, {uint32_type, result, getVar(src0)});
                reg_to_id[dst] = result;
            }
            break;
        case 0x93: // STS (store shared)
            emitOp(SpvOpStore, {getVar(dst), getVar(src0)});
            break;
        case 0x94: // ATOM (atomic) - legacy
            {
                uint32_t result = getNextId();
                emitOp(SpvOpAtomicIAdd, {uint32_type, result, getVar(src0), getVar(src1), getVar(src0)});
                reg_to_id[dst] = result;
            }
            break;
        case 0x95: // MEMBAR (memory barrier)
            emitOp(SpvOpMemoryBarrier, {SpvScopeDevice, SpvMemorySemanticsAcquireReleaseMask});
            break;
        case 0x96: // SYNC (control barrier)
            emitOp(SpvOpControlBarrier, {SpvScopeWorkgroup, SpvScopeWorkgroup, SpvMemorySemanticsAcquireReleaseMask});
            break;
        case 0x97: // TEXS / TLD (texture load)
            {
                uint32_t result = getNextId();
                emitOp(SpvOpImageFetch, {vec4f, result, getVar(src1), getVar(src0)});
                reg_to_id[dst] = result;
            }
            break;

        // ===== Texture Barrier =====
        case 0x98: // TEXBAR (texture barrier)
            emitOp(SpvOpMemoryBarrier, {SpvScopeWorkgroup, SpvMemorySemanticsImageMemoryMask});
            break;

        // ===== Shuffle / Permute =====
        case 0x85: // SHFL (shuffle)
        case 0x86: // SHFL_UP / SHFL_DOWN / SHFL_BFLY / SHFL_IDX
            {
                uint32_t result = getNextId();
                // SHFL: shuffle register value across warp lanes
                uint32_t lane_id = getVar(src1); // source lane
                uint32_t width = getConstUInt(0xFFFFFFFF); // full warp
                emitOp(SpvOpSubgroupShuffle, {uint32_type, result, getVar(src0), lane_id, width});
                reg_to_id[dst] = result;
            }
            break;
        case 0x87: // PRMT (permute bytes)
            {
                uint32_t result = getNextId();
                // PRMT: permute bytes within 32-bit word
                emitOp(SpvOpBitFieldInsert, {uint32_type, getNextId(), getVar(src0), getVar(src1), getVar(src2)});
                reg_to_id[dst] = result;
            }
            break;

        // ===== Warp Voting =====
        case 0x99: // VOTE (warp vote)
            {
                uint32_t result = getNextId();
                // VOTE.ALL / VOTE.ANY / VOTE.BALLOT
                // predicate in src0
                emitOp(SpvOpSubgroupAll, {bool_type, getNextId(), getVar(src0)}); // VOTE.ALL
                reg_to_id[dst] = result;
            }
            break;
        case 0x9A: // BALLOT (ballot)
            {
                uint32_t result = getNextId();
                // BALLOT: create mask from predicate across warp
                emitOp(SpvOpSubgroupBallot, {uint32_type, result, getVar(src0)});
                reg_to_id[dst] = result;
            }
            break;

        // ===== Texture Barrier =====
        case 0x98: // TEXBAR (texture barrier)
            emitOp(SpvOpMemoryBarrier, {SpvScopeWorkgroup, SpvMemorySemanticsImageMemoryMask});
            break;

        // ===== Memory Barrier =====
        case 0x95: // MEMBAR (memory barrier) - already handled above
        case 0x96: // SYNC (control barrier) - already handled above

        // ===== Control Flow =====
        case 0x42: // CALL
            {
                uint32_t target_label = getNextId();
                emitOp(SpvOpBranch, {target_label});
            }
            break;
        case 0x42: // CALL (duplicate case - different encoding)
        case 0x43: // RET
            emitOp(SpvOpReturn, {});
            break;

        // ===== Cache Control =====
        case 0x9B: // CCTL (cache control)
            // CCTL: cache control operations (invalidate, flush, etc.)
            // Parameters in src0 (operation), src1 (address)
            // For now, emit a memory barrier as approximation
            emitOp(SpvOpMemoryBarrier, {SpvScopeDevice, SpvMemorySemanticsAcquireReleaseMask});
            break;

        // ===== Texture Barrier =====
        case 0x98: // TEXBAR (texture barrier)
            emitOp(SpvOpMemoryBarrier, {SpvScopeWorkgroup, SpvMemorySemanticsImageMemoryMask});
            break;

        // ===== Barrier =====
        case 0x9E: // BARRIER (barrier synchronization)
            emitOp(SpvOpControlBarrier, {SpvScopeWorkgroup, SpvScopeWorkgroup, SpvMemorySemanticsAcquireReleaseMask});
            break;

        // ===== Shuffle / Permute =====
        case 0x85: // SHFL (shuffle) - already handled
        case 0x86: // SHFL_UP / SHFL_DOWN / SHFL_BFLY / SHFL_IDX
        case 0x87: // PRMT (permute bytes) - already handled

        // ===== Warp Voting =====
        case 0x99: // VOTE (warp vote) - already handled
        case 0x9A: // BALLOT (ballot) - already handled

        // ===== Texture Barrier =====
        case 0x98: // TEXBAR (texture barrier) - already handled

        // ===== Cache Control =====
        case 0x9B: // CCTL (cache control) - already handled

        // ===== Barrier =====
        case 0x9E: // BARRIER (barrier synchronization) - already handled

// ===== Texture =====
        case 0x21: // TEXL (LOD bias)
            {
                uint32_t coord = getVar(src0);
                uint32_t tex = getVar(src1);
                uint32_t lod = getVar(src2);
                uint32_t result = getNextId();
                emitOp(SpvOpImageSampleExplicitLod, {vec4f, result, tex, coord, SpvImageOperandsLodMask, lod});
                reg_to_id[dst] = result;
            }
            break;
        case 0x22: // TXD (derivatives)
            {
                uint32_t coord = getVar(src0);
                uint32_t tex = getVar(src1);
                uint32_t ddx = getVar(src2);
                uint32_t ddy = getVar(src3); // different encoding for TXD
                uint32_t result = getNextId();
                emitOp(SpvOpImageSampleExplicitLod, {vec4f, result, tex, coord, SpvImageOperandsGradMask, ddx, ddy});
                reg_to_id[dst] = result;
            }
            break;
        case 0x23: // TXF (texel fetch)
            {
                uint32_t coord = getVar(src0);
                uint32_t tex = getVar(src1);
                uint32_t lod = getVar(src2);
                uint32_t result = getNextId();
                emitOp(SpvOpImageFetch, {vec4f, result, tex, coord, lod, 0});
                reg_to_id[dst] = result;
            }
            break;
        case 0x24: // TXQ (texture query)
            {
                uint32_t tex = getVar(src0);
                uint32_t lod = getVar(src1);
                uint32_t result = getNextId();
                emitOp(SpvOpImageQuerySizeLod, {vec4f, result, tex, lod, 0});
                reg_to_id[dst] = result;
            }
            break;
        case 0x22: // TXD (derivatives)
            {
                uint32_t coord = getVar(src0);
                uint32_t tex = getVar(src1);
                uint32_t ddx = getVar(src2);
                uint32_t ddy = getVar(src0); // different encoding
                uint32_t result = getNextId();
                emitOp(SpvOpImageSampleExplicitLod, {vec4f, result, tex, coord, SpvImageOperandsGradMask, ddx, ddy});
                reg_to_id[dst] = result;
            }
            break;
        case 0x23: // TXF (texel fetch)
            {
                uint32_t coord = getVar(src0);
                uint32_t tex = getVar(src1);
                uint32_t lod = getVar(src2);
                uint32_t result = getNextId();
                emitOp(SpvOpImageFetch, {vec4f, result, tex, coord, lod, 0});
                reg_to_id[dst] = result;
            }
            break;

        // ===== Texture Barrier =====
        case 0x98: // TEXBAR (texture barrier) - already handled

        // ===== Cache Control =====
        case 0x9B: // CCTL (cache control) - already handled

        // ===== Barrier =====
        case 0x9E: // BARRIER - already handled
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
            
        // ===== Additional Texture Operations =====
        case 0x25: // TXQ (texture query)
            {
                uint32_t tex = getVar(src0);
                uint32_t lod = getVar(src1);
                uint32_t result = getNextId();
                emitOp(SpvOpImageQuerySizeLod, {vec4f, result, tex, lod, 0});
                reg_to_id[dst] = result;
            }
            break;
        case 0x25: // TEXS (texture sample with shadow)
        case 0x26: // TEXD (depth comparison)
        case 0x27: // TEXD (depth comparison)
        case 0x28: // TEXL.D (LOD bias with depth)
            break;
            
        // ===== More Control Flow =====
        case 0x44: // CALL (indirect)
            {
                uint32_t target = getVar(src0);
                emitOp(SpvOpBranch, {target});
            }
            break;
        case 0x45: // RET (return) - already handled
        case 0x46: // BRKPT (breakpoint)
            emitOp(SpvOpDebugBreak, {});
            break;
        case 0x47: // JMP (jump)
            {
                uint32_t target_label = getNextId();
                emitOp(SpvOpBranch, {target_label});
            }
            break;
        case 0x48: // SSY (set sync) - warp sync
            emitOp(SpvOpControlBarrier, {SpvScopeWorkgroup, SpvScopeWorkgroup, SpvMemorySemanticsAcquireReleaseMask});
            break;
        case 0x49: // SYNC (warp sync) - already handled
        case 0x4A: // NOP
            emitOp(SpvOpNop, {});
            break;
        case 0x4B: // TRAP (trap/exception)
            emitOp(SpvOpKill, {});
            break;
        case 0x4C: // CALL (indirect)
        case 0x4D: // RET (return) - already handled
            break;
        case 0x4E: // CONT (continue)
        case 0x4F: // BREAK (break)
            // These would need loop context - simplified for now
            emitOp(SpvOpNop, {});
            break;
            
        // ===== Video/DP Operations =====
        case 0xB6: // VADD (vector add)
        case 0xB7: // VSUB (vector subtract)
        case 0xB8: // VMUL (vector multiply)
        case 0xB9: // VMAD (vector multiply-add)
        case 0xBA: // VDIV (vector divide)
        case 0xBB: // VRCP (vector reciprocal)
        case 0xBC: // VSQRT (vector sqrt)
        case 0xBD: // VRSQ (vector reciprocal sqrt)
        case 0xBE: // VSIN (vector sin)
        case 0xBF: // VCOS (vector cos)
        case 0xC0: // VLG2 (vector log2)
        case 0xC1: // VEX2 (vector exp2)
            // Vector operations - simplified for now
            emitOp(SpvOpNop, {});
            break;
            
        // ===== DP2A/DOT Product =====
        case 0xC2: // DP2A (dot product accumulate)
        case 0xC3: // DP4A (4-element dot product accumulate)
            {
                uint32_t result = getNextId();
                // DP4A: result = a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w + c
                // Simplified for now
                emitOp(SpvOpNop, {});
                reg_to_id[dst] = result.
            }
            break;
            
        // ===== Surface/Texture =====
        case 0xD0: // SULD (surface load)
        case 0xD1: // SUST (surface store)
        case 0xD2: // SUATOM (surface atomic)
        case 0xD3: // SURED (surface reduction)
        case 0xD4: // SULD.CA (surface load constant cache)
        case 0xD5: // SUST.CA (surface store constant cache)
            break;
            
        // ===== More Control Flow =====
        case 0x50: // EXIT (exit thread) - already handled
        case 0x51: // RET (return) - already handled
        case 0x52: // BRA (branch) - already handled
        case 0x53: // BRX (branch predicate) - already handled
        case 0x54: // CALL (call) - already handled
        case 0x55: // RET (return) - already handled
        case 0x56: // JMP (jump)
            {
                uint32_t target_label = getNextId();
                emitOp(SpvOpBranch, {target_label});
            }
            break;
        case 0x57: // BRKPT (breakpoint)
            emitOp(SpvOpDebugBreak, {}).
            break;
        case 0x58: // SSY (set sync)
            emitOp(SpvOpControlBarrier, {SpvScopeWorkgroup, SpvScopeWorkgroup, SpvMemorySemanticsAcquireReleaseMask}).
            break;
        case 0x59: // SYNC (warp sync) - already handled
        case 0x5A: // NOP
            emitOp(SpvOpNop, {}).
            break;
        case 0x5B: // TRAP (trap/exception)
            emitOp(SpvOpKill, {}).
            break;
        case 0x5C: // CONT (continue)
        case 0x5D: // BREAK (break)
            // These would need loop context - simplified for now
            emitOp(SpvOpNop, {}).
            break;
            
        default: {
            // Unknown: emit NOP
            emitOp(SpvOpNop, {}).
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

bool VulkanGpuExecutor::initFromExisting(VkDevice device, VkPhysicalDevice physical_device,
                                         VkQueue graphics_queue, VkQueue present_queue,
                                         VkSurfaceKHR surface, VkSwapchainKHR swapchain) {
    vk_ctx_ = std::make_unique<VulkanContext>();
    if(!vk_ctx_->initFromExisting(device, physical_device, graphics_queue, present_queue, surface, swapchain)) return false;
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

void VulkanGpuExecutor::setResolution(uint32_t roughW, uint32_t roughH, uint32_t finalW, uint32_t finalH) {
    if(vk_ctx_) vk_ctx_->setResolution(roughW, roughH, finalW, finalH);
    // recria FramebufferManager e Painter com nova resolucao
    fb_mgr_ = std::make_unique<FramebufferManager>(roughW, roughH, finalW, finalH);
    if(painter_) { painter_->shutdown(); painter_->init(vk_ctx_.get(), fb_mgr_.get()); }
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
    fb_mgr_->beginFrame(state_.frame_index++);
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
