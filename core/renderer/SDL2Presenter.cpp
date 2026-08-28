#include "SDL2Presenter.h"

#ifdef MGD_HAS_SDL2

#include <iostream>

namespace mgd {

SDL2Presenter::SDL2Presenter(SDL_Window* window, SDL_Renderer* renderer, int width, int height)
    : window_(window), renderer_(renderer), width_(width), height_(height) {
    createTexture(width, height);
}

SDL2Presenter::~SDL2Presenter() {
    destroyTexture();
}

void SDL2Presenter::present(const IFramebuffer& fb) {
    if (!texture_ || !renderer_) return;

    int fbW = fb.width();
    int fbH = fb.height();

    if (fbW != width_ || fbH != height_) {
        destroyTexture();
        createTexture(fbW, fbH);
        width_ = fbW;
        height_ = fbH;
    }

    SDL_UpdateTexture(texture_, nullptr, fb.data(), fb.stride());
    SDL_RenderClear(renderer_);
    SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
    SDL_RenderPresent(renderer_);
}

void SDL2Presenter::onResize(int w, int h) {
    if (w == width_ && h == height_) return;
    destroyTexture();
    createTexture(w, h);
    width_ = w;
    height_ = h;
}

void SDL2Presenter::createTexture(int w, int h) {
    if (!renderer_ || w <= 0 || h <= 0) return;
    texture_ = SDL_CreateTexture(
        renderer_,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING,
        w, h
    );
    if (!texture_) {
        std::cerr << "SDL2Presenter: Failed to create texture: " << SDL_GetError() << std::endl;
    }
}

void SDL2Presenter::destroyTexture() {
    if (texture_) {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
    }
}

} // namespace mgd

#endif // MGD_HAS_SDL2
