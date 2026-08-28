#pragma once

#include "../framebuffer/Framebuffer.h"
#include "../../common/RGBA.h"

namespace mgd {

class SkyRenderer {
    RGBA top_color = {30, 80, 160, 255};
    RGBA bottom_color = {100, 140, 200, 255};

public:
    void render(Framebuffer& fb) const;
    void setColors(const RGBA& top, const RGBA& bottom);
};

} // namespace mgd
