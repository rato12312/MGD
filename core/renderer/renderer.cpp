#include "renderer.h"

#ifdef MGD_HAS_SDL2

#include <iostream>

namespace mgd {

Renderer::~Renderer() {
    shutdown();
}

bool Renderer::initialize(const Config& config) {
    config_ = config;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return false;
    }

    Uint32 window_flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
    if (config_.fullscreen) {
        window_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }

    window_ = SDL_CreateWindow(
        config_.title.c_str(),
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        config_.width, config_.height,
        window_flags
    );
    if (!window_) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return false;
    }

    Uint32 renderer_flags = SDL_RENDERER_ACCELERATED;
    if (config_.vsync) {
        renderer_flags |= SDL_RENDERER_PRESENTVSYNC;
    }

    sdl_renderer_ = SDL_CreateRenderer(window_, -1, renderer_flags);
    if (!sdl_renderer_) {
        sdl_renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!sdl_renderer_) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window_);
        SDL_Quit();
        return false;
    }

    framebuffer_.resize(config_.width, config_.height);
    initialized_ = true;
    running_ = true;
    last_time_ = std::chrono::high_resolution_clock::now();

    std::cout << "MGD Renderer initialized: " << config_.width << "x" << config_.height << std::endl;
    return true;
}

void Renderer::shutdown() {
    if (!initialized_) return;

    if (sdl_renderer_) {
        SDL_DestroyRenderer(sdl_renderer_);
        sdl_renderer_ = nullptr;
    }
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_Quit();
    initialized_ = false;
    running_ = false;
}

void Renderer::run() {
    if (!initialized_) return;
    mainLoop();
}

void Renderer::mainLoop() {
    const float target_frame_time = 1.0f / config_.target_fps;

    while (running_) {
        auto frame_start = std::chrono::high_resolution_clock::now();

        processEvents();

        if (update_cb_) {
            update_cb_(delta_time_);
        }

        if (render_cb_) {
            render_cb_();
        }

        calculateFPS();

        auto frame_end = std::chrono::high_resolution_clock::now();
        float frame_time = std::chrono::duration<float>(frame_end - frame_start).count();

        if (frame_time < target_frame_time && !config_.vsync) {
            SDL_Delay(static_cast<Uint32>((target_frame_time - frame_time) * 1000.0f));
        }
    }
}

void Renderer::processEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            running_ = false;
        } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
            running_ = false;
        } else if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
            config_.width = event.window.data1;
            config_.height = event.window.data2;
            framebuffer_.resize(config_.width, config_.height);
            if (resize_cb_) {
                resize_cb_(config_.width, config_.height);
            }
        }

        if (input_cb_) {
            input_cb_(event);
        }
    }
}

void Renderer::calculateFPS() {
    auto now = std::chrono::high_resolution_clock::now();
    delta_time_ = std::chrono::duration<float>(now - last_time_).count();
    last_time_ = now;
    frame_count_++;

    fps_accumulator_ += delta_time_;
    fps_frame_count_++;
    if (fps_accumulator_ >= 1.0f) {
        current_fps_ = static_cast<float>(fps_frame_count_) / fps_accumulator_;
        fps_accumulator_ = 0.0f;
        fps_frame_count_ = 0;
    }
}

} // namespace mgd

#endif // MGD_HAS_SDL2
