#include "DepthBuffer.h"
#include <algorithm>

namespace mgd {

DepthBuffer::DepthBuffer(int w, int h) {
    resize(w, h);
}

void DepthBuffer::resize(int w, int h) {
    width_ = w;
    height_ = h;
    data_.resize(static_cast<size_t>(w) * h);
}

void DepthBuffer::clear(float value) {
    std::fill(data_.begin(), data_.end(), value);
}

bool DepthBuffer::test(int x, int y, float depth) const {
    size_t idx = static_cast<size_t>(y) * width_ + x;
    return depth < data_[idx];
}

void DepthBuffer::write(int x, int y, float depth) {
    size_t idx = static_cast<size_t>(y) * width_ + x;
    data_[idx] = depth;
}

float DepthBuffer::get(int x, int y) const {
    size_t idx = static_cast<size_t>(y) * width_ + x;
    return data_[idx];
}

int DepthBuffer::width() const {
    return width_;
}

int DepthBuffer::height() const {
    return height_;
}

bool DepthBuffer::inBounds(int x, int y) const {
    return x >= 0 && x < width_ && y >= 0 && y < height_;
}

} // namespace mgd
