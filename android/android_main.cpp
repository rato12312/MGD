/*
 * MGD Odyssey Android - Native Entry Point
 * 
 * Usa GameActivity (Android 12+) ou NativeActivity
 * Inicializa Vulkan, carrega assets, roda loop principal
 */

#include <android_native_app_glue.h>
#include <android/log.h>
#include <android/input.h>
#include <android/native_window.h>
#include <vulkan/vulkan_android.h>
#include <memory>
#include <vector>
#include <cstdio>

#include "emulador-mgd/runtime/Emulator.h"
#include "emulador-mgd/gpu/VulkanBackend.h"

#define LOG_TAG "MGD_Odyssey"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

/*
 * MGD Odyssey Android - Native Entry Point
 * 
 * Usa GameActivity (Android 12+) ou NativeActivity
 * Inicializa Vulkan, carrega assets, roda loop principal
 */

#include <android_native_app_glue.h>
#include <android/log.h>
#include <android/input.h>
#include <android/native_window.h>
#include <vulkan/vulkan_android.h>
#include <memory>
#include <vector>
#include <cstdio>
#include <set>

#include "emulador-mgd/runtime/Emulator.h"
#include "emulador-mgd/gpu/VulkanBackend.h"

#define LOG_TAG "MGD_Odyssey"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

struct AndroidEngine {
    struct android_app* app = nullptr;
    std::unique_ptr<mgd::emu::Emulator> emulator;
    std::unique_ptr<mgd::gpu::VulkanGpuExecutor> gpu;
    ANativeWindow* window = nullptr;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    bool initialized = false;
    bool running = false;
    uint64_t frame_count = 0;
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphics_queue = VK_NULL_HANDLE;
    VkQueue present_queue = VK_NULL_HANDLE;
    uint32_t graphics_queue_family = UINT32_MAX;
    uint32_t present_queue_family = UINT32_MAX;
    VkFormat swapchain_format = VK_FORMAT_B8G8R8A8_SRGB;
    VkExtent2D swapchain_extent{};
    std::vector<VkImage> swapchain_images;
    std::vector<VkImageView> swapchain_image_views;
    VkRenderPass render_pass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers;
    VkCommandPool command_pool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> command_buffers;
    std::vector<VkSemaphore> image_available_semaphores;
    std::vector<VkSemaphore> render_finished_semaphores;
    std::vector<VkFence> in_flight_fences;
    uint32_t current_frame = 0;
    uint64_t frame_count = 0;
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphics_queue = VK_NULL_HANDLE;
    VkQueue present_queue = VK_NULL_HANDLE;
    uint32_t graphics_queue_family = UINT32_MAX;
    uint32_t present_queue_family = UINT32_MAX;
    VkFormat swapchain_format = VK_FORMAT_B8G8R8A8_SRGB;
    VkExtent2D swapchain_extent{};
    std::vector<VkImage> swapchain_images;
    std::vector<VkImageView> swapchain_image_views;
    VkRenderPass render_pass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers;
    VkCommandPool command_pool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> command_buffers;
    std::vector<VkSemaphore> image_available_semaphores;
    std::vector<VkSemaphore> render_finished_semaphores;
    std::vector<VkFence> in_flight_fences;
    uint32_t current_frame = 0;
    uint64_t frame_count = 0;
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphics_queue = VK_NULL_HANDLE;
    VkQueue present_queue = VK_NULL_HANDLE;
    uint32_t graphics_queue_family = UINT32_MAX;
    uint32_t present_queue_family = UINT32_MAX;
    VkFormat swapchain_format = VK_FORMAT_B8G8R8A8_SRGB;
    VkExtent2D swapchain_extent{};
    std::vector<VkImage> swapchain_images;
    std::vector<VkImageView> swapchain_image_views;
    VkRenderPass render_pass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers;
    VkCommandPool command_pool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> command_buffers;
    std::vector<VkSemaphore> image_available_semaphores;
    std::vector<VkSemaphore> render_finished_semaphores;
    std::vector<VkFence> in_flight_fences;
    uint32_t current_frame = 0;
    uint64_t frame_count = 0;
    
    ~AndroidEngine() {
        shutdown();
    }
    
    void shutdown() {
        if (device != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(device);
        }
        if (swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(device, swapchain, nullptr);
            swapchain = VK_NULL_HANDLE;
        }
        for (auto fb : framebuffers) {
            vkDestroyFramebuffer(device, fb, nullptr);
        }
        framebuffers.clear();
        for (auto view : swapchain_image_views) {
            vkDestroyImageView(device, view, nullptr);
        }
        swapchain_image_views.clear();
        if (render_pass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(device, render_pass, nullptr);
            render_pass = VK_NULL_HANDLE;
        }
        if (command_pool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device, command_pool, nullptr);
            command_pool = VK_NULL_HANDLE;
        }
        for (auto sem : image_available_semaphores) {
            vkDestroySemaphore(device, sem, nullptr);
        }
        for (auto sem : render_finished_semaphores) {
            vkDestroySemaphore(device, sem, nullptr);
        }
        for (auto fence : in_flight_fences) {
            vkDestroyFence(device, fence, nullptr);
        }
        if (command_pool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device, command_pool, nullptr);
            command_pool = VK_NULL_HANDLE;
        }
        if (surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(instance, surface, nullptr);
            surface = VK_NULL_HANDLE;
        }
        if (device != VK_NULL_HANDLE) {
            vkDestroyDevice(device, nullptr);
            device = VK_NULL_HANDLE;
        }
        if (instance != VK_NULL_HANDLE) {
            vkDestroyInstance(instance, nullptr);
            instance = VK_NULL_HANDLE;
        }
        if (window) {
            ANativeWindow_release(window);
            window = nullptr;
        }
        emulator.reset();
        initialized = false;
    }
    
    bool createSwapchain() {
        VkSurfaceCapabilitiesKHR caps;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &caps);
        
        // Escolhe formato
        uint32_t format_count = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, formats.data());
        
        swapchain_format = formats[0].format;
        VkColorSpaceKHR color_space = formats[0].colorSpace;
        
        // Escolhe present mode
        uint32_t present_mode_count = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, nullptr);
        std::vector<VkPresentModeKHR> present_modes(present_mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, present_modes.data());
        
        VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR; // VSync
        for (auto mode : present_modes) {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                present_mode = mode;
                break;
            }
        }
        
        // Extent
        swapchain_extent = caps.currentExtent;
        if (swapchain_extent.width == UINT32_MAX) {
            swapchain_extent = {1280, 720}; // fallback
        }
        
        uint32_t image_count = caps.minImageCount + 1;
        if (caps.maxImageCount > 0 && image_count > caps.maxImageCount) {
            image_count = caps.maxImageCount;
        }
        
        VkSwapchainCreateInfoKHR ci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
        ci.surface = surface;
        ci.minImageCount = image_count;
        ci.imageFormat = swapchain_format;
        ci.imageColorSpace = formats[0].colorSpace;
        ci.imageExtent = swapchain_extent;
        ci.imageArrayLayers = 1;
        ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        ci.preTransform = caps.currentTransform;
        ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        ci.presentMode = present_mode;
        ci.clipped = VK_TRUE;
        ci.oldSwapchain = VK_NULL_HANDLE;
        
        if (vkCreateSwapchainKHR(device, &ci, nullptr, &swapchain) != VK_SUCCESS) {
            return false;
        }
        
        // Pega imagens do swapchain
        uint32_t image_count = 0;
        vkGetSwapchainImagesKHR(device, swapchain, &image_count, nullptr);
        swapchain_images.resize(image_count);
        vkGetSwapchainImagesKHR(device, swapchain, &image_count, swapchain_images.data());
        
        // Cria image views
        swapchain_image_views.resize(image_count);
        for (uint32_t i = 0; i < image_count; i++) {
            VkImageViewCreateInfo iv{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            iv.image = swapchain_images[i];
            iv.viewType = VK_IMAGE_VIEW_TYPE_2D;
            iv.format = swapchain_format;
            iv.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            iv.subresourceRange.levelCount = 1;
            iv.subresourceRange.layerCount = 1;
            vkCreateImageView(device, &iv, nullptr, &swapchain_image_views[i]);
        }
        
        // Render pass
        VkAttachmentDescription color_att{};
        color_att.format = swapchain_format;
        color_att.samples = VK_SAMPLE_COUNT_1_BIT;
        color_att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color_att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color_att.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        
        VkAttachmentReference color_ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_ref;
        
        VkRenderPassCreateInfo rpci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        rpci.attachmentCount = 1;
        rpci.pAttachments = &color_att;
        rpci.subpassCount = 1;
        rpci.pSubpasses = &subpass;
        
        if (vkCreateRenderPass(device, &rpci, nullptr, &render_pass) != VK_SUCCESS) {
            return false;
        }
        
        // Framebuffers
        framebuffers.resize(swapchain_images.size());
        for (size_t i = 0; i < swapchain_images.size(); i++) {
            VkImageView attachments[] = {swapchain_image_views[i]};
            VkFramebufferCreateInfo fbci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
            fbci.renderPass = render_pass;
            fbci.attachmentCount = 1;
            fbci.pAttachments = attachments;
            fbci.width = swapchain_extent.width;
            fbci.height = swapchain_extent.height;
            fbci.layers = 1;
            vkCreateFramebuffer(device, &fbci, nullptr, &framebuffers[i]);
        }
        
        // Command pool
        VkCommandPoolCreateInfo cpi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        cpi.queueFamilyIndex = graphics_queue_family;
        cpi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        vkCreateCommandPool(device, &cpi, nullptr, &command_pool);
        
        // Command buffers
        command_buffers.resize(2);
        VkCommandBufferAllocateInfo cbai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        cbai.commandPool = command_pool;
        cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cbai.commandBufferCount = 2;
        vkAllocateCommandBuffers(device, &cbai, command_buffers.data());
        
        // Sync objects
        image_available_semaphores.resize(2);
        render_finished_semaphores.resize(2);
        in_flight_fences.resize(2);
        
        VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        
        for (size_t i = 0; i < 2; i++) {
            vkCreateSemaphore(device, &sci, nullptr, &image_available_semaphores[i]);
            vkCreateSemaphore(device, &sci, nullptr, &render_finished_semaphores[i]);
            vkCreateFence(device, &fci, nullptr, &in_flight_fences[i]);
        }
        
        return true;
    }
    
    void shutdown() {
        if (device != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(device);
        }
        if (swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(device, swapchain, nullptr);
            swapchain = VK_NULL_HANDLE;
        }
        for (auto fb : framebuffers) {
            vkDestroyFramebuffer(device, fb, nullptr);
        }
        framebuffers.clear();
        for (auto view : swapchain_image_views) {
            vkDestroyImageView(device, view, nullptr);
        }
        swapchain_image_views.clear();
        if (render_pass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(device, render_pass, nullptr);
            render_pass = VK_NULL_HANDLE;
        }
        if (command_pool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device, command_pool, nullptr);
            command_pool = VK_NULL_HANDLE;
        }
        for (auto sem : image_available_semaphores) {
            vkDestroySemaphore(device, sem, nullptr);
        }
        for (auto sem : render_finished_semaphores) {
            vkDestroySemaphore(device, sem, nullptr);
        }
        for (auto fence : in_flight_fences) {
            vkDestroyFence(device, fence, nullptr);
        }
        if (command_pool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device, command_pool, nullptr);
            command_pool = VK_NULL_HANDLE;
        }
        if (surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(instance, surface, nullptr);
            surface = VK_NULL_HANDLE;
        }
        if (device != VK_NULL_HANDLE) {
            vkDestroyDevice(device, nullptr);
            device = VK_NULL_HANDLE;
        }
        if (instance != VK_NULL_HANDLE) {
            vkDestroyInstance(instance, nullptr);
            instance = VK_NULL_HANDLE;
        }
        if (window) {
            ANativeWindow_release(window);
            window = nullptr;
        }
        emulator.reset();
        initialized = false;
    }
    
    bool init(struct android_app* app_) {
        app = app_;
        emulator = std::make_unique<mgd::emu::Emulator>();
        emulator->switches().mgd_mali = mgd::emu::MaliLevel::Edge;
        emulator->switches().mgd_translation = true;
        emulator->switches().mgd_image = true;
        // GPU resolucao dinamica: Edge 512x288 -> 720p via Painter
        uint32_t rw, rh, fw, fh;
        emulator->getGpuResolution(rw, rh, fw, fh);
        
        // Cria instância Vulkan com suporte a Android surface
        VkApplicationInfo app_info{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        app_info.pApplicationName = "MGD Odyssey";
        app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.pEngineName = "MGD Engine";
        app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.apiVersion = VK_API_VERSION_1_1;
        
        VkInstanceCreateInfo instance_info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        instance_info.pApplicationInfo = &app_info;
        const char* extensions[] = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_ANDROID_SURFACE_EXTENSION_NAME};
        instance_info.enabledExtensionCount = 2;
        instance_info.ppEnabledExtensionNames = extensions;
        
        VkResult result = vkCreateInstance(&instance_info, nullptr, &instance);
        if (result != VK_SUCCESS) {
            LOGE("Failed to create Vulkan instance: %d", result);
            return false;
        }
        
        // Cria surface Android
        VkAndroidSurfaceCreateInfoKHR surface_info{VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR};
        surface_info.window = app->window;
        if (vkCreateAndroidSurfaceKHR(instance, &surface_info, nullptr, &surface) != VK_SUCCESS) {
            LOGE("Failed to create Android surface");
            return false;
        }
        
        // Seleciona physical device
        uint32_t device_count = 0;
        vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
        if (device_count == 0) {
            LOGE("No Vulkan physical devices found");
            return false;
        }
        std::vector<VkPhysicalDevice> devices(device_count);
        vkEnumeratePhysicalDevices(instance, &device_count, devices.data());
        physical_device = devices[0]; // Simplificado: pega o primeiro
        
        // Acha filas
        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);
        std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families.data());
        for (uint32_t i = 0; i < queue_family_count; i++) {
            if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                graphics_queue_family = i;
                break;
            }
        }
        
        // Verifica suporte a present
        VkBool32 present_support = false;
        for (uint32_t i = 0; i < queue_family_count; i++) {
            vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &present_support);
            if (present_support) {
                present_queue_family = i;
                break;
            }
        }
        
        if (graphics_queue_family == UINT32_MAX || present_queue_family == UINT32_MAX) {
            LOGE("Queue families not found");
            return false;
        }
        
        // Cria logical device
        float queue_priority = 1.0f;
        std::vector<VkDeviceQueueCreateInfo> queue_infos;
        std::set<uint32_t> unique_queue_families = {graphics_queue_family, present_queue_family};
        for (uint32_t qf : unique_queue_families) {
            VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
            qi.queueFamilyIndex = qf;
            qi.queueCount = 1;
            qi.pQueuePriorities = &queue_priority;
            queue_infos.push_back(qi);
        }
        
        VkPhysicalDeviceFeatures features{};
        features.samplerAnisotropy = VK_TRUE;
        features.fillModeNonSolid = VK_TRUE;
        
        VkDeviceCreateInfo device_info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        device_info.queueCreateInfoCount = static_cast<uint32_t>(queue_infos.size());
        device_info.pQueueCreateInfos = queue_infos.data();
        device_info.pEnabledFeatures = &features;
        
        const char* device_extensions[] = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_KHR_ANDROID_SURFACE_EXTENSION_NAME
        };
        device_info.enabledExtensionCount = 2;
        device_info.ppEnabledExtensionNames = device_extensions;
        device_info.queueCreateInfoCount = static_cast<uint32_t>(queue_infos.size());
        device_info.pQueueCreateInfos = queue_infos.data();
        device_info.pEnabledFeatures = &features;
        
        if (vkCreateDevice(physical_device, &device_info, nullptr, &device) != VK_SUCCESS) {
            LOGE("Failed to create logical device");
            return false;
        }
        
        vkGetDeviceQueue(device, graphics_queue_family, 0, &graphics_queue);
        vkGetDeviceQueue(device, present_queue_family, 0, &present_queue);
        
        // Cria swapchain
        if (!createSwapchain()) {
            LOGE("Failed to create swapchain");
            return false;
        }
        
        // Inicializa GPU executor com device já criado
        gpu = std::make_unique<mgd::gpu::VulkanGpuExecutor>();
        if (!gpu->initFromExisting(device, physical_device, graphics_queue, present_queue, surface, swapchain)) {
            LOGE("Failed to init GPU executor");
            return false;
        }
        
        // Cria emulador
        emulator = std::make_unique<mgd::emu::Emulator>();
        emulator->switches().mgd_mali = mgd::emu::MaliLevel::Edge;
        emulator->switches().mgd_translation = true;
        emulator->switches().mgd_image = true;
        // GPU resolucao dinamica: Edge 512x288 -> 720p via Painter
        uint32_t rw, rh, fw, fh;
        emulator->getGpuResolution(rw, rh, fw, fh);
        gpu->setResolution(rw, rh, fw, fh);
        
        window = app->window;
        // Tenta carregar jogo e keys automaticamente
        loadGame();
        initialized = true;
        running = true;
        
        LOGI("MGD Odyssey Android initialized (%ux%u -> %ux%u)", rw, rh, fw, fh);
        return true;
    }

    bool loadGame() {
        const char* nspPath = "/sdcard/MGD/game.nsp";
        const char* keysDir = "/sdcard/MGD/keys/";
        // Carrega keys (prod.keys / title.keys) se existirem
        char prodKeys[256]; snprintf(prodKeys, sizeof(prodKeys), "%sprod.keys", keysDir);
        FILE* kf = fopen(prodKeys, "rb");
        if (kf) {
            uint8_t key[16] = {0};
            if (fread(key, 1, 16, kf) == 16) emulator->keys().setSlot(0, key);
            fclose(kf);
            LOGI("Loaded prod.keys");
        }
        // Carrega NSP
        FILE* f = fopen(nspPath, "rb");
        if (!f) { LOGI("No game at %s, aguardando usuario", nspPath); return false; }
        fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
        std::vector<uint8_t> buf(sz);
        if (fread(buf.data(), 1, sz, f) != (size_t)sz) { fclose(f); return false; }
        fclose(f);
        // Detecta versao e aplica offsets
        auto ver = mgd::emu::OdysseyVersion::V150; // fallback
        emulator->setOdysseyOffsets(mgd::emu::makeOdysseyOffsets(ver));
        bool ok = emulator->bootNsp(buf.data(), buf.size());
        if (ok) LOGI("Game booted, entry=0x%llx", (unsigned long long)emulator->cpu().pc());
        else LOGE("bootNsp failed - verifique keys e NSP");
        return ok;
    }
    
    void runFrame() {
        if (!initialized || !running) return;
        
        // Frame do emulador
        char frame_path[64];
        snprintf(frame_path, sizeof(frame_path), "/sdcard/mgd_frame_%llu.ppm", frame_count);
        
        bool ok = emulator->frame(frame_path, 64);
        if (ok) {
            frame_count++;
        }
        
        // TODO: apresentar frame final na tela via Vulkan swapchain
    }
    
    void onWindowCreated(ANativeWindow* new_window) {
        if (window) ANativeWindow_release(window);
        window = new_window;
        if (gpu && window) {
            uint32_t rw, rh, fw, fh;
            emulator->getGpuResolution(rw, rh, fw, fh);
            gpu->setResolution(rw, rh, fw, fh);
            LOGI("Window created %p, swapchain %ux%u", window, rw, rh);
        }
    }
    
    void onWindowDestroyed() {
        if (gpu) gpu->shutdown();
        if (window) { ANativeWindow_release(window); window = nullptr; }
    }
    
    void onInputEvent(AInputEvent* event) {
        if (!emulator) return;
        int32_t type = AInputEvent_getType(event);
        if (type == AINPUT_EVENT_TYPE_MOTION) {
            int32_t action = AMotionEvent_getAction(event);
            int32_t act = action & AMOTION_EVENT_ACTION_MASK;
            size_t count = AMotionEvent_getPointerCount(event);
            for (size_t i = 0; i < count; i++) {
                float x = AMotionEvent_getX(event, i);
                float y = AMotionEvent_getY(event, i);
                // Mapeia toque para HID (stick esquerdo + botoes)
                if (act == AMOTION_EVENT_ACTION_DOWN || act == AMOTION_EVENT_ACTION_MOVE) {
                    emulator->kernel().hid().press(0, 1 << 0); // A
                    // Envia posicao touch como hid state (x,y normalizado)
                } else if (act == AMOTION_EVENT_ACTION_UP) {
                    emulator->kernel().hid().release(0, 1 << 0);
                }
            }
        } else if (type == AINPUT_EVENT_TYPE_KEY) {
            int32_t code = AKeyEvent_getKeyCode(event);
            int32_t action = AKeyEvent_getAction(event);
            uint32_t btn = 0;
            if (code == AKEYCODE_BUTTON_A) btn = 1 << 0;
            else if (code == AKEYCODE_BUTTON_B) btn = 1 << 1;
            else if (code == AKEYCODE_DPAD_UP) btn = 1 << 4;
            else if (code == AKEYCODE_DPAD_DOWN) btn = 1 << 5;
            if (action == AKEY_EVENT_ACTION_DOWN) emulator->kernel().hid().press(0, btn);
            else if (action == AKEY_EVENT_ACTION_UP) emulator->kernel().hid().release(0, btn);
        }
    }
};

static AndroidEngine g_engine;

static void handle_cmd(struct android_app* app, int32_t cmd) {
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            if (app->window) {
                g_engine.onWindowCreated(app->window);
            }
            break;
        case APP_CMD_TERM_WINDOW:
            g_engine.onWindowDestroyed();
            break;
        case APP_CMD_DESTROY:
            g_engine.shutdown();
            break;
        case APP_CMD_GAINED_FOCUS:
            g_engine.running = true;
            break;
        case APP_CMD_LOST_FOCUS:
            g_engine.running = false;
            break;
        default:
            break;
    }
}

static int32_t handle_input(struct android_app* app, AInputEvent* event) {
    g_engine.onInputEvent(event);
    return 0;
}

void android_main(struct android_app* app) {
    app->onAppCmd = handle_cmd;
    app->onInputEvent = handle_input;
    
    LOGI("MGD Odyssey android_main started");
    
    if (!g_engine.init(app)) {
        LOGE("Failed to initialize engine");
        return;
    }
    
    // Main loop
    int events;
    struct android_poll_source* source;
    while (!app->destroyRequested) {
        // Processa eventos
        while (ALooper_pollAll(g_engine.running ? 0 : -1, nullptr, &events, (void**)&source) >= 0) {
            if (source) source->process(app, source);
        }
        
        if (g_engine.running) {
            g_engine.runFrame();
        }
    }
    
    g_engine.shutdown();
    LOGI("MGD Odyssey android_main exited");
}