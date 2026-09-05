#pragma once

#include "../../common/RGBA.h"
#include <cstdint>

namespace mgd {

class IFramebuffer {
public:
    virtual ~IFramebuffer() = default;
    virtual int width() const = 0;
    virtual int height() const = 0;
    virtual void clear(const RGBA& color) = 0;
    virtual void setPixel(int x, int y, const RGBA& color) = 0;
    virtual RGBA getPixel(int x, int y) const = 0;
    virtual const uint8_t* data() const = 0;
    virtual uint8_t* data() = 0;
    virtual void resize(int w, int h) = 0;
    virtual int stride() const = 0;
};

} // namespace mgd
