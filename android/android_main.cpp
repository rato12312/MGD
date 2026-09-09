/*
 * MGD Odyssey Android - Native Entry Point
 * 
 * Usa GameActivity (Android 12+) ou NativeActivity
 * Inicializa Vulkan, carrega assets, roda loop principal
 */

#include <android_native_app_glue.h>
#include <android/log.h>
#include <vulkan/vulkan_android.h>
#include <memory>

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
    bool initialized = false;
    bool running = false;
    uint64_t frame_count = 0;
    
    ~AndroidEngine() {
        shutdown();
    }
    
    void shutdown() {
        if (gpu) {
            gpu->shutdown();
            gpu.reset();
        }
        emulator.reset();
        if (window) {
            ANativeWindow_release(window);
            window = nullptr;
        }
        initialized = false;
    }
    
    bool init(struct android_app* app_) {
        app = app_;
        
        // Cria emulador
        emulator = std::make_unique<mgd::emu::Emulator>();
        
        // Configura switches para Android (Cheap mode)
        emulator->switches().mgd_mali = mgd::emu::MaliLevel::Edge;
        emulator->switches().mgd_translation = true;
        emulator->switches().mgd_image = true;
        
        // Inicializa GPU
        gpu = std::make_unique<mgd::gpu::VulkanGpuExecutor>();
        if (!gpu->init(app->window)) {
            LOGE("Failed to init GPU");
            return false;
        }
        
        window = app->window;
        initialized = true;
        running = true;
        
        LOGI("MGD Odyssey Android initialized");
        return true;
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
        if (gpu) {
            // Recria swapchain para nova janela
        }
    }
    
    void onWindowDestroyed() {
        if (window) {
            ANativeWindow_release(window);
            window = nullptr;
        }
    }
    
    void onInputEvent(AInputEvent* event) {
        // TODO: processar input (touch, gamepad)
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