#include "Framebuffer.h"
#include <algorithm>
#include <cstring>

namespace mgd {

Framebuffer::Framebuffer(int w, int h) {
    resize(w, h);
}

void Framebuffer::resize(int w, int h) {
    width_ = w;
    height_ = h;
    data_.resize(static_cast<size_t>(w) * h * 4);
}

void Framebuffer::clear(const RGBA& color) {
    uint8_t* ptr = data_.data();
    size_t count = data_.size() / 4;
    for (size_t i = 0; i < count; ++i) {
        ptr[0] = color.r;
        ptr[1] = color.g;
        ptr[2] = color.b;
        ptr[3] = color.a;
        ptr += 4;
    }
}

void Framebuffer::setPixel(int x, int y, const RGBA& color) {
    size_t offset = (static_cast<size_t>(y) * width_ + x) * 4;
    data_[offset + 0] = color.r;
    data_[offset + 1] = color.g;
    data_[offset + 2] = color.b;
    data_[offset + 3] = color.a;
}

RGBA Framebuffer::getPixel(int x, int y) const {
    size_t offset = (static_cast<size_t>(y) * width_ + x) * 4;
    return {data_[offset + 0], data_[offset + 1], data_[offset + 2], data_[offset + 3]};
}

const uint8_t* Framebuffer::data() const {
    return data_.data();
}

uint8_t* Framebuffer::data() {
    return data_.data();
}

int Framebuffer::width() const {
    return width_;
}

int Framebuffer::height() const {
    return height_;
}

int Framebuffer::stride() const {
    return width_ * 4;
}

bool Framebuffer::inBounds(int x, int y) const {
    return x >= 0 && x < width_ && y >= 0 && y < height_;
}

void Framebuffer::drawLine(int x0, int y0, int x1, int y1, const RGBA& color) {
    int dx = std::abs(x1 - x0);
    int dy = -std::abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (true) {
        if (inBounds(x0, y0)) {
            setPixel(x0, y0, color);
        }
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

} // namespace mgd
