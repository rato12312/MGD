#pragma once

#include "IFramebuffer.h"
#include <vector>

namespace mgd {

class Framebuffer : public IFramebuffer {
    int width_ = 0;
    int height_ = 0;
    std::vector<uint8_t> data_;

public:
    Framebuffer() = default;
    Framebuffer(int w, int h);

    void resize(int w, int h) override;
    void clear(const RGBA& color) override;
    void setPixel(int x, int y, const RGBA& color) override;
    RGBA getPixel(int x, int y) const override;
    const uint8_t* data() const override;
    uint8_t* data() override;
    int width() const override;
    int height() const override;
    int stride() const override;
    bool inBounds(int x, int y) const;

    void drawLine(int x0, int y0, int x1, int y1, const RGBA& color);
};

} // namespace mgd
