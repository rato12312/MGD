#pragma once

#ifdef MGD_HAS_SDL2

#include "../painter/framebuffer/Framebuffer.h"
#include <SDL.h>
#include <string>
#include <functional>
#include <chrono>

namespace mgd {

class Renderer {
public:
    struct Config {
        std::string title = "MGD Engine";
        int width = 800;
        int height = 600;
        bool fullscreen = false;
        bool vsync = true;
        int target_fps = 60;
    };

    Renderer() = default;
    ~Renderer();

    bool initialize(const Config& config = Config());
    void shutdown();

    bool isRunning() const { return running_; }
    void stop() { running_ = false; }

    void run();

    using UpdateCallback = std::function<void(float dt)>;
    using RenderCallback = std::function<void()>;
    using InputCallback = std::function<void(const SDL_Event& event)>;
    using ResizeCallback = std::function<void(int w, int h)>;

    void setUpdateCallback(UpdateCallback cb) { update_cb_ = cb; }
    void setRenderCallback(RenderCallback cb) { render_cb_ = cb; }
    void setInputCallback(InputCallback cb) { input_cb_ = cb; }
    void setResizeCallback(ResizeCallback cb) { resize_cb_ = cb; }

    SDL_Window* getWindow() const { return window_; }
    SDL_Renderer* getSDLRenderer() const { return sdl_renderer_; }

    Framebuffer& getFramebuffer() { return framebuffer_; }
    const Framebuffer& getFramebuffer() const { return framebuffer_; }

    int width() const { return config_.width; }
    int height() const { return config_.height; }
    float deltaTime() const { return delta_time_; }
    float currentFPS() const { return current_fps_; }
    int frameCount() const { return frame_count_; }

private:
    void mainLoop();
    void processEvents();
    void calculateFPS();

    Config config_;
    SDL_Window* window_ = nullptr;
    SDL_Renderer* sdl_renderer_ = nullptr;

    Framebuffer framebuffer_;

    bool running_ = false;
    bool initialized_ = false;

    UpdateCallback update_cb_;
    RenderCallback render_cb_;
    InputCallback input_cb_;
    ResizeCallback resize_cb_;

    std::chrono::high_resolution_clock::time_point last_time_;
    float delta_time_ = 0.0f;
    int frame_count_ = 0;
    float current_fps_ = 0.0f;
    float fps_accumulator_ = 0.0f;
    int fps_frame_count_ = 0;
};

} // namespace mgd

#endif // MGD_HAS_SDL2
