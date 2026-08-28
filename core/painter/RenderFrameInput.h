#pragma once

#include "../camera/Camera.h"
#include "../visibility/VisibleSet.h"
#include "resources/IRenderResourceProvider.h"
#include "RenderSettings.h"

namespace mgd {

struct RenderFrameInput {
    const Camera* camera = nullptr;
    const VisibleSet* visible_set = nullptr;
    IRenderResourceProvider* resources = nullptr;
    RenderSettings settings;
};

} // namespace mgd
