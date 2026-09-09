/*
 * MGD Odyssey Android - Native Entry Point
 * 
 * Usa GameActivity (Android 12+) ou NativeActivity
 * Inicializa Vulkan, carrega assets, roda loop principal
 */

#include <android_native_app_glue.h>
#include <android/log.h>
#include <android/input.h>
#include <vulkan/vulkan_android.h>
#include <memory>
#include <vector>
#include <cstdio>

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
        emulator = std::make_unique<mgd::emu::Emulator>();
        emulator->switches().mgd_mali = mgd::emu::MaliLevel::Edge;
        emulator->switches().mgd_translation = true;
        emulator->switches().mgd_image = true;
        // GPU resolucao dinamica: Edge 512x288 -> 720p via Painter
        uint32_t rw, rh, fw, fh;
        emulator->getGpuResolution(rw, rh, fw, fh);
        gpu = std::make_unique<mgd::gpu::VulkanGpuExecutor>();
        if (!gpu->init(app->window)) {
            LOGE("Failed to init GPU");
            return false;
        }
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