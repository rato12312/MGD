#include "Painter.h"
#include "../mental_map/Entity.h"
#include "rasterizer/VertexProcessor.h"
#include "debug/DebugRenderer.h"
#include <algorithm>
#include <cmath>
#include <chrono>

namespace mgd {

void Painter::initialize(int width, int height) {
    settings.width = width;
    settings.height = height;
    framebuffer.resize(width, height);
    depth_buffer.resize(width, height);
}

void Painter::shutdown() {
    materials.clear();
    textures.clear();
    lighting.clearLights();
    framebuffer.resize(0, 0);
    depth_buffer.resize(0, 0);
}

void Painter::resize(int w, int h) {
    settings.width = w;
    settings.height = h;
    framebuffer.resize(w, h);
    depth_buffer.resize(w, h);
}

Framebuffer& Painter::getFramebuffer() { return framebuffer; }
DepthBuffer& Painter::getDepthBuffer() { return depth_buffer; }
MaterialSystem& Painter::getMaterialSystem() { return materials; }
TextureSystem& Painter::getTextureSystem() { return textures; }
LightSystem& Painter::getLightSystem() { return lighting; }
SkyRenderer& Painter::getSkyRenderer() { return sky_renderer; }
RenderSettings& Painter::getSettings() { return settings; }

Mat4 Painter::buildModelMatrix(const Transform& transform) const {
    Mat4 t = Mat4::translate(transform.position);
    Mat4 r = Mat4::rotation(transform.rotation_euler);
    Mat4 s = Mat4::scale(transform.scale);
    return t * r * s;
}

std::vector<RenderEntity> Painter::prepareRenderEntities(const VisibleSet& visible, IRenderResourceProvider* resources) {
    std::vector<RenderEntity> entities;
    entities.reserve(visible.entities.size());

    for (const auto& ve : visible.entities) {
        RenderEntity re;
        re.id = ve.id;
        re.transform.position = ve.position;
        re.transform.rotation_euler = {0.0f, 0.0f, 0.0f};
        re.transform.scale = ve.scale;
        re.mesh_id = 1;
        re.material_id = static_cast<MaterialID>(ve.material_id);
        re.bounds = ve.bounds;
        re.flags = ve.flags;
        entities.push_back(re);
    }

    return entities;
}

std::vector<RenderBatch> Painter::batchEntities(const std::vector<RenderEntity>& entities) {
    std::vector<RenderBatch> batches;

    for (uint32_t i = 0; i < static_cast<uint32_t>(entities.size()); ++i) {
        const auto& re = entities[i];
        bool found = false;
        for (auto& batch : batches) {
            if (batch.material == re.material_id && batch.mesh == re.mesh_id) {
                batch.entity_indices.push_back(i);
                found = true;
                break;
            }
        }
        if (!found) {
            RenderBatch batch;
            batch.material = re.material_id;
            batch.mesh = re.mesh_id;
            batch.entity_indices.push_back(i);
            batches.push_back(batch);
        }
    }

    return batches;
}

void Painter::rasterizeBatches(const std::vector<RenderBatch>& batches,
                               const std::vector<RenderEntity>& entities,
                               const Camera& camera, IRenderResourceProvider* resources,
                               RenderStats& stats) {
    if (!resources) return;

    const Mat4& viewProj = camera.getViewProjectionMatrix();
    int sw = framebuffer.width();
    int sh = framebuffer.height();

    for (const auto& batch : batches) {
        const MeshData* mesh = resources->getMesh(batch.mesh);
        if (!mesh) continue;

        const TextureData* tex = nullptr;
        const MaterialRecord* matPtr = nullptr;
        MaterialRecord defaultMat;

        if (batch.material != INVALID_MATERIAL_ID) {
            matPtr = resources->getMaterial(batch.material);
            if (!matPtr) matPtr = materials.get(batch.material);
        }
        if (!matPtr) {
            matPtr = &defaultMat;
            defaultMat = materials.getDefault();
        }

        if (matPtr->texture_id != INVALID_TEXTURE_ID) {
            tex = resources->getTexture(matPtr->texture_id);
            if (!tex) tex = textures.get(matPtr->texture_id);
        }
        if (!tex) {
            // Use default texture only if material has a texture reference
            // but it couldn't be found — leave tex as nullptr to use vertex colors
        }

        stats.draw_calls++;

        for (uint32_t idx : batch.entity_indices) {
            const RenderEntity& re = entities[idx];
            Mat4 model = buildModelMatrix(re.transform);
            Mat4 mvp = viewProj * model;

            stats.triangles_submitted += static_cast<uint32_t>(mesh->indices.size() / 3);

            for (size_t i = 0; i + 2 < mesh->indices.size(); i += 3) {
                const RenderVertex& sv0 = mesh->vertices[mesh->indices[i]];
                const RenderVertex& sv1 = mesh->vertices[mesh->indices[i + 1]];
                const RenderVertex& sv2 = mesh->vertices[mesh->indices[i + 2]];

                // Transform to clip space, clip against the frustum there,
                // then perspective divide + screen mapping (clip must happen
                // BEFORE the divide to be correct).
                VertexProcessor::ClipVertex cv0 = VertexProcessor::toClipSpace(sv0, mvp);
                VertexProcessor::ClipVertex cv1 = VertexProcessor::toClipSpace(sv1, mvp);
                VertexProcessor::ClipVertex cv2 = VertexProcessor::toClipSpace(sv2, mvp);

                auto clipResult = VertexProcessor::clipTriangle(cv0, cv1, cv2);
                if (clipResult.vertices.size() < 3) continue;

                stats.triangles_clipped += static_cast<uint32_t>(clipResult.vertices.size()) - 2;
                // Note: fan triangulation of a convex polygon (Sutherland-Hodgman
                // output keeps the polygon convex and consistently wound).

                for (size_t ci = 1; ci + 1 < clipResult.vertices.size(); ++ci) {
                    RenderVertex s0 = VertexProcessor::toScreen(clipResult.vertices[0], sw, sh);
                    RenderVertex s1 = VertexProcessor::toScreen(clipResult.vertices[ci], sw, sh);
                    RenderVertex s2 = VertexProcessor::toScreen(clipResult.vertices[ci + 1], sw, sh);
                    uint32_t written = Rasterizer::rasterizeTriangle(
                        s0,
                        s1,
                        s2,
                        framebuffer,
                        depth_buffer,
                        tex,
                        *matPtr,
                        settings.backface_culling,
                        settings.depth_test
                    );
                    stats.triangles_rasterized++;
                    stats.pixels_written += written;
                    stats.pixels_tested += written; // approx (inside pixels)
                }
            }
        }
    }
}

RenderFrameOutput Painter::render(const RenderFrameInput& input) {
    auto startTime = std::chrono::high_resolution_clock::now();

    RenderFrameOutput output;
    output.framebuffer = &framebuffer;

    if (input.settings.width != settings.width || input.settings.height != settings.height) {
        resize(input.settings.width, input.settings.height);
    }
    settings = input.settings;

    framebuffer.clear(settings.clear_color);
    depth_buffer.clear(1.0f);

    sky_renderer.render(framebuffer);

    if (!input.camera || !input.visible_set || !input.resources) {
        output.errors.push_back("Missing camera, visible set, or resource provider");
        return output;
    }

    auto visStart = std::chrono::high_resolution_clock::now();
    auto renderEntities = prepareRenderEntities(*input.visible_set, input.resources);
    auto visEnd = std::chrono::high_resolution_clock::now();
    output.stats.visibility_time_ms = std::chrono::duration<float, std::milli>(visEnd - visStart).count();

    auto batches = batchEntities(renderEntities);

    auto rasterStart = std::chrono::high_resolution_clock::now();
    rasterizeBatches(batches, renderEntities, *input.camera, input.resources, output.stats);
    auto rasterEnd = std::chrono::high_resolution_clock::now();
    output.stats.raster_time_ms = std::chrono::duration<float, std::milli>(rasterEnd - rasterStart).count();

    if (settings.debug_mode == DebugMode::WIREFRAME) {
        // Re-render in wireframe mode: clear and redraw wireframe
        framebuffer.clear(settings.clear_color);
        depth_buffer.clear(1.0f);

        const Mat4& vp = input.camera->getViewProjectionMatrix();
        for (const auto& re : renderEntities) {
            const MeshData* mesh = input.resources->getMesh(re.mesh_id);
            if (!mesh) continue;
            Mat4 model = buildModelMatrix(re.transform);
            Mat4 mvp = vp * model;
            DebugRenderer::renderWireframe(framebuffer, mesh->vertices, mesh->indices,
                                           mvp, framebuffer.width(), framebuffer.height());
        }
    } else if (settings.debug_mode == DebugMode::DEPTH) {
        DebugRenderer::renderDepth(framebuffer, depth_buffer);
    } else if (settings.debug_mode == DebugMode::BOUNDS) {
        const Mat4& vp = input.camera->getViewProjectionMatrix();
        for (const auto& re : renderEntities) {
            Mat4 model = buildModelMatrix(re.transform);
            Mat4 mvp = vp * model;
            DebugRenderer::renderBounds(framebuffer, re.bounds, mvp,
                                        framebuffer.width(), framebuffer.height(),
                                        {0, 255, 0, 255});
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    output.stats.frame_time_ms = std::chrono::duration<float, std::milli>(endTime - startTime).count();

    return output;
}

} // namespace mgd
