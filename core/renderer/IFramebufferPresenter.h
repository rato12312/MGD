#pragma once

#include "../painter/framebuffer/IFramebuffer.h"

namespace mgd {

class IFramebufferPresenter {
public:
    virtual ~IFramebufferPresenter() = default;
    virtual void present(const IFramebuffer& fb) = 0;
    virtual void onResize(int w, int h) = 0;
};

} // namespace mgd
