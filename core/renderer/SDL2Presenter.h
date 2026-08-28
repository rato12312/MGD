#pragma once

#include "IFramebufferPresenter.h"

#ifdef MGD_HAS_SDL2
#include <SDL.h>

namespace mgd {

class SDL2Presenter : public IFramebufferPresenter {
public:
    SDL2Presenter(SDL_Window* window, SDL_Renderer* renderer, int width, int height);
    ~SDL2Presenter() override;

    void present(const IFramebuffer& fb) override;
    void onResize(int w, int h) override;

private:
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
    int width_ = 0;
    int height_ = 0;

    void createTexture(int w, int h);
    void destroyTexture();
};

} // namespace mgd

#endif // MGD_HAS_SDL2
