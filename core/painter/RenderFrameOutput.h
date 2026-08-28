#pragma once

#include "framebuffer/IFramebuffer.h"
#include "RenderStats.h"
#include <vector>
#include <string>

namespace mgd {

struct RenderFrameOutput {
    IFramebuffer* framebuffer = nullptr;
    RenderStats stats;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

} // namespace mgd
