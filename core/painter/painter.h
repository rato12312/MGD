#pragma once

#include "framebuffer/Framebuffer.h"
#include "framebuffer/DepthBuffer.h"
#include "material/MaterialSystem.h"
#include "texture/TextureSystem.h"
#include "lighting/LightSystem.h"
#include "sky/SkyRenderer.h"
#include "rasterizer/Rasterizer.h"
#include "FrameArena.h"
#include "RenderVertex.h"
#include "RenderEntity.h"
#include "RenderBatch.h"
#include "RenderSettings.h"
#include "RenderStats.h"
#include "RenderFrameInput.h"
#include "RenderFrameOutput.h"
#include "../camera/Camera.h"
#include "../visibility/VisibleSet.h"
#include <vector>

namespace mgd {

class Painter {
    Framebuffer framebuffer;
    DepthBuffer depth_buffer;
    Rasterizer rasterizer;
    MaterialSystem materials;
    TextureSystem textures;
    LightSystem lighting;
    SkyRenderer sky_renderer;
    FrameArena arena;
    RenderSettings settings;

public:
    void initialize(int width, int height);
    void shutdown();

    RenderFrameOutput render(const RenderFrameInput& input);

    Framebuffer& getFramebuffer();
    DepthBuffer& getDepthBuffer();
    MaterialSystem& getMaterialSystem();
    TextureSystem& getTextureSystem();
    LightSystem& getLightSystem();
    SkyRenderer& getSkyRenderer();
    RenderSettings& getSettings();

    void resize(int w, int h);

private:
    std::vector<RenderEntity> prepareRenderEntities(const VisibleSet& visible, IRenderResourceProvider* resources);
    std::vector<RenderBatch> batchEntities(const std::vector<RenderEntity>& entities);
    void rasterizeBatches(const std::vector<RenderBatch>& batches,
                          const std::vector<RenderEntity>& entities,
                          const Camera& camera, IRenderResourceProvider* resources,
                          RenderStats& stats);
    Mat4 buildModelMatrix(const Transform& transform) const;
};

} // namespace mgd
