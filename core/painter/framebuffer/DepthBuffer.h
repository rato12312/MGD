#pragma once

#include <vector>

namespace mgd {

class DepthBuffer {
    int width_ = 0;
    int height_ = 0;
    std::vector<float> data_;

public:
    DepthBuffer() = default;
    DepthBuffer(int w, int h);

    void resize(int w, int h);
    void clear(float value = 1.0f);
    bool test(int x, int y, float depth) const;
    void write(int x, int y, float depth);
    float get(int x, int y) const;
    int width() const;
    int height() const;
    bool inBounds(int x, int y) const;
};

} // namespace mgd
