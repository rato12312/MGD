#include <iostream>
#include <fstream>
#include <memory>
#include <cmath>
#include <cstring>
#include <string>

#include "core/common/Types.h"
#include "core/common/Vec3.h"
#include "core/common/Mat4.h"
#include "core/common/AABB.h"

#include "core/mental_map/Entity.h"
#include "core/mental_map/MentalMap.h"
#include "core/mental_map/Region.h"
#include "core/mental_map/builder/MentalMapBuilder.h"

#include "core/collision/CollisionSystem.h"
#include "core/collision/shapes/CollisionShape.h"

#include "core/camera/Camera.h"
#include "core/camera/CameraController.h"

#include "core/visibility/BasicVisibility.h"

#include "core/painter/Painter.h"
#include "core/painter/RenderFrameInput.h"
#include "core/painter/RenderFrameOutput.h"
#include "core/painter/RenderSettings.h"
#include "core/painter/resources/MockResourceProvider.h"

#include "core/scanner/Scanner.h"
#include "core/scanner/adapters/SkyrimPCAdapter.h"

#ifdef MGD_HAS_SDL2
#include "core/renderer/renderer.h"
#include "core/renderer/SDL2Presenter.h"
#include <SDL.h>
#endif

using namespace mgd;

#ifdef MGD_HAS_SDL2

class Game {
public:
    bool init(int argc, char* argv[]) {
        Renderer::Config config;
        config.title = "MGD Engine - Prototype";
        config.width = 800;
        config.height = 600;
        config.vsync = true;
        config.target_fps = 60;

        if (!renderer_.initialize(config)) {
            std::cerr << "Failed to initialize renderer!" << std::endl;
            return false;
        }

        presenter_ = std::make_unique<SDL2Presenter>(
            renderer_.getWindow(), renderer_.getSDLRenderer(),
            renderer_.width(), renderer_.height()
        );

        painter_.initialize(renderer_.width(), renderer_.height());

        setupCamera();
        setupTestScene();
        setupResources();
        setupCollision();

        renderer_.setUpdateCallback([this](float dt) { onUpdate(dt); });
        renderer_.setRenderCallback([this]() { onRender(); });
        renderer_.setInputCallback([this](const SDL_Event& e) { onInput(e); });
        renderer_.setResizeCallback([this](int w, int h) { onResize(w, h); });

        std::cout << "MGD Engine started." << std::endl;
        std::cout << "WASD/Arrows: Move camera" << std::endl;
        std::cout << "Mouse: Look around" << std::endl;
        std::cout << "Q/E: Move up/down" << std::endl;
        std::cout << "ESC: Quit" << std::endl;

        return true;
    }

    void run() {
        renderer_.run();
    }

private:
    void setupCamera() {
        camera_.setPosition(Vec3(0.0f, 5.0f, 15.0f));
        camera_.lookAt(Vec3(0.0f, 0.0f, 0.0f));
        camera_.setFOV(70.0f);
        camera_.setAspectRatio(800.0f / 600.0f);
        camera_.setNearPlane(0.1f);
        camera_.setFarPlane(1000.0f);
    }

    void setupTestScene() {
        Region region0;
        region0.id = 1;
        region0.bounds = AABB({-500, -50, -500}, {500, 100, 500});
        region0.load_state = RegionLoadState::LOADED;
        mental_map_.addRegion(region0);

        addEntity(2, {0.0f, 0.0f, 0.0f}, {0, 0, 0}, {1, 1, 1},
                  {1, 1, 1}, 0xFF0000, 0, 1); // Red cube center
        addEntity(3, {5.0f, 0.0f, 0.0f}, {0, 0, 0}, {1, 1, 1},
                  {2, 2, 2}, 0x00FF00, 0, 1); // Green cube right
        addEntity(4, {-5.0f, 0.0f, 0.0f}, {0, 0, 0}, {1, 1, 1},
                  {1.5f, 1.5f, 1.5f}, 0x0000FF, 0, 1); // Blue cube left
        addEntity(5, {0.0f, 0.0f, -8.0f}, {0, 0, 0}, {1, 1, 1},
                  {3, 3, 3}, 0xFFFF00, 0, 1); // Yellow cube back
        addEntity(6, {3.0f, 2.0f, -3.0f}, {0, 0, 0}, {1, 1, 1},
                  {0.5f, 0.5f, 0.5f}, 0xFF00FF, 0, 1); // Magenta small cube

        addEntity(10, {0.0f, -1.0f, 0.0f}, {0, 0, 0}, {0, 0, 0},
                  {50, 0.2f, 50}, 0x808080, 0, 1); // Ground plane
    }

    void addEntity(EntityID id, Vec3 pos, Vec3 rot, Vec3 scale,
                   Vec3 half_extents, uint32_t color_hex, uint32_t visual_ref, RegionID region) {
        MentalEntity e;
        e.id = id;
        e.resource_id = 1;
        e.collision_id = id;
        e.transform.position = pos;
        e.transform.rotation_euler = rot;
        e.transform.scale = scale;
        e.state = EntityState::ACTIVE;
        e.visibility = VisibilityState::UNCHECKED;
        e.region_id = region;
        e.visual_ref = visual_ref;
        e.flags = 0;

        e.bounds.aabb = AABB(pos - half_extents, pos + half_extents);

        mental_map_.addEntity(std::move(e));
    }

    void setupResources() {
        if (!resource_provider_.getMesh(0)) {
            MockResourceProvider temp;
            const MeshData* mesh = temp.getMesh(0);
            if (mesh) {
                resource_provider_.addMesh(0, *mesh);
            }
        }
    }

    void setupCollision() {
        collision_.syncFromMentalMap(mental_map_);
    }

    void onUpdate(float dt) {
        if (SDL_GetRelativeMouseMode() == SDL_FALSE) {
            if (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON(SDL_BUTTON_LEFT)) {
                SDL_SetRelativeMouseMode(SDL_TRUE);
            }
        }

        CameraInput input;
        input.speed = 10.0f;
        input.sensitivity = 0.003f;

        const Uint8* keys = SDL_GetKeyboardState(nullptr);

        if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]) input.forward = 1.0f;
        if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN]) input.forward = -1.0f;
        if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]) input.strafe = -1.0f;
        if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) input.strafe = 1.0f;
        if (keys[SDL_SCANCODE_Q]) input.vertical = 1.0f;
        if (keys[SDL_SCANCODE_E]) input.vertical = -1.0f;

        cam_ctrl_.setInput(input);
        cam_ctrl_.update(camera_, dt);
    }

    void onRender() {
        VisibleSet visible = visibility_.compute(mental_map_, camera_, collision_);

        RenderFrameInput input;
        input.camera = &camera_;
        input.visible_set = &visible;
        input.resources = &resource_provider_;
        input.settings = painter_.getSettings();

        RenderFrameOutput output = painter_.render(input);

        if (output.framebuffer && presenter_) {
            presenter_->present(*output.framebuffer);
        }
    }

    void onInput(const SDL_Event& event) {
        if (event.type == SDL_MOUSEMOTION && SDL_GetRelativeMouseMode()) {
            float dx = static_cast<float>(event.motion.xrel);
            float dy = static_cast<float>(event.motion.yrel);
            camera_.rotateYaw(-dx * 0.003f);
            camera_.rotatePitch(-dy * 0.003f);
        }

        if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT) {
            SDL_SetRelativeMouseMode(SDL_FALSE);
        }

        if (event.type == SDL_KEYDOWN) {
            const Uint8* keys = SDL_GetKeyboardState(nullptr);
            if (keys[SDL_SCANCODE_1]) {
                painter_.getSettings().debug_mode = DebugMode::NORMAL;
            } else if (keys[SDL_SCANCODE_2]) {
                painter_.getSettings().debug_mode = DebugMode::WIREFRAME;
            } else if (keys[SDL_SCANCODE_3]) {
                painter_.getSettings().debug_mode = DebugMode::DEPTH;
            } else if (keys[SDL_SCANCODE_4]) {
                painter_.getSettings().debug_mode = DebugMode::BOUNDS;
            }
        }
    }

    void onResize(int w, int h) {
        camera_.setAspectRatio(static_cast<float>(w) / static_cast<float>(h));
        painter_.resize(w, h);
        if (presenter_) {
            presenter_->onResize(w, h);
        }
    }

    Renderer renderer_;
    std::unique_ptr<SDL2Presenter> presenter_;

    MentalMap mental_map_;
    CollisionSystem collision_;
    Camera camera_;
    CameraController cam_ctrl_;
    BasicVisibility visibility_;

    Painter painter_;
    MockResourceProvider resource_provider_;
};

#endif // MGD_HAS_SDL2

int main(int argc, char* argv[]) {
#ifdef MGD_HAS_SDL2
    Game game;
    if (!game.init(argc, argv)) {
        return 1;
    }
    game.run();
    return 0;
#else

    // ==== Headless functional demo (no SDL2 required) ====

    // 1. Build the mental map: either scanned from a real folder (argv[1],
    //    Skyrim PC .nif) or the built-in mock scene when no argument is given.
    MentalMap mental_map;

    if (argc > 1) {
        Scanner scanner;
        SkyrimPCAdapter adapter;
        scanner.setAdapter(&adapter);
        scanner.scan(argv[1]);

        ScanReport report = scanner.getReport();
        std::cout << "Scan: " << report.entity_records << " entities, "
                  << report.resource_records << " resources, "
                  << report.collision_records << " collisions, "
                  << report.stats.errors << " errors" << std::endl;

        MentalMapBuilder builder(mental_map);
        builder.buildFromRecords(scanner.getEntityRecords());

        // TODO: records carry no world placement yet (bounds are placeholders).
        //       Arrange them in a grid so the demo output is visible.
        size_t i = 0;
        for (EntityID eid : mental_map.getActiveEntities()) {
            auto ent = mental_map.getEntity(eid);
            if (!ent) continue;
            Transform t = ent->get().transform;
            t.position = Vec3((i % 7) * 2.5f, 0.0f, -(i / 7) * 2.5f);
            mental_map.setTransform(eid, t);
            ++i;
        }
    } else {
        Region region0;
        region0.id = 1;
        region0.bounds = AABB({-500, -50, -500}, {500, 100, 500});
        region0.load_state = RegionLoadState::LOADED;
        mental_map.addRegion(region0);

        auto addEntity = [&](EntityID id, Vec3 pos, Vec3 scale, Vec3 half_extents) {
            MentalEntity e;
            e.id = id;
            e.resource_id = 1;
            e.collision_id = id;
            e.transform.position = pos;
            e.transform.rotation_euler = {0, 0, 0};
            e.transform.scale = scale;
            e.state = EntityState::ACTIVE;
            e.visibility = VisibilityState::UNCHECKED;
            e.region_id = 1;
            e.bounds.aabb = AABB(pos - half_extents, pos + half_extents);
            mental_map.addEntity(std::move(e));
        };

        addEntity(2, {0, 0, 0}, {1, 1, 1}, {1, 1, 1});       // red cube center
        addEntity(3, {5, 0, 0}, {2, 2, 2}, {2, 2, 2});       // green cube right
        addEntity(4, {-5, 0, 0}, {1.5f, 1.5f, 1.5f}, {1.5f, 1.5f, 1.5f}); // blue cube left
        addEntity(5, {0, 0, -8}, {3, 3, 3}, {3, 3, 3});      // yellow cube back
        addEntity(6, {3, 2, -3}, {0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.5f}); // magenta small
        addEntity(10, {0, -1, 0}, {50, 0.2f, 50}, {50, 0.2f, 50}); // ground plane
    }

    // 2. Camera
    Camera camera;
    camera.setPosition(Vec3(0.0f, 5.0f, 15.0f));
    camera.lookAt(Vec3(0.0f, 0.0f, 0.0f));
    camera.setFOV(70.0f);
    camera.setAspectRatio(800.0f / 600.0f);
    camera.setNearPlane(0.1f);
    camera.setFarPlane(1000.0f);

    // 3. Collision (auto-synced from the mental map)
    CollisionSystem collision;
    collision.syncFromMentalMap(mental_map);

    // 4. Visibility
    BasicVisibility visibility;
    VisibleSet visible = visibility.compute(mental_map, camera, collision);
    std::cout << "Visible: " << visible.total_visible << " of "
              << visible.total_considered << " considered" << std::endl;

    // 5. Render one frame
    Painter painter;
    painter.initialize(800, 600);

    RenderSettings settings = painter.getSettings();
    settings.width = 800;
    settings.height = 600;

    MockResourceProvider resources;

    RenderFrameInput input;
    input.camera = &camera;
    input.visible_set = &visible;
    input.resources = &resources;
    input.settings = settings;

    RenderFrameOutput output = painter.render(input);

    for (const auto& err : output.errors) {
        std::cerr << "Render error: " << err << std::endl;
    }
    // Debug headless: why pixels 0 with 78 tris rasterized?
    {
        auto vp = camera.getViewProjectionMatrix();
        std::cerr << "DEBUG headless VP m[0]=" << vp.m[0] << " m[5]=" << vp.m[5] << " m[10]=" << vp.m[10] << " m[11]=" << vp.m[11] << " m[14]=" << vp.m[14] << " m[15]=" << vp.m[15] << "\n";
        std::cerr << "DEBUG cam pos=(" << camera.getState().position.x << "," << camera.getState().position.y << "," << camera.getState().position.z << ") fwd=(" << camera.getForward().x << "," << camera.getForward().y << "," << camera.getForward().z << ")\n";
        if (!visible.entities.empty()) {
            auto &ve = visible.entities[0];
            std::cerr << "DEBUG first visible id=" << ve.id << " pos=(" << ve.position.x << "," << ve.position.y << "," << ve.position.z << ") dist=" << ve.distance_to_camera << "\n";
            Vec4 clip = vp.transformPoint(Vec4(ve.position, 1.0f));
            std::cerr << "DEBUG first ent clip=(" << clip.x << "," << clip.y << "," << clip.z << "," << clip.w << ") ndc=(" << (clip.w!=0?clip.x/clip.w:0) << "," << (clip.w!=0?clip.y/clip.w:0) << "," << (clip.w!=0?clip.z/clip.w:0) << ")\n";
        }
        std::cerr << "DEBUG framebuffer " << output.framebuffer->width() << "x" << output.framebuffer->height() << " depth_clear=" << 1.0f << "\n";
    }

    // 6. Dump the framebuffer as a PPM image
    std::string out_path = "output.ppm";
    bool wrote = false;
    if (output.framebuffer) {
        std::ofstream ppm(out_path, std::ios::binary);
        if (ppm.is_open()) {
            ppm << "P6\n" << output.framebuffer->width() << " "
                << output.framebuffer->height() << "\n255\n";
            for (int y = 0; y < output.framebuffer->height(); ++y) {
                for (int x = 0; x < output.framebuffer->width(); ++x) {
                    RGBA c = output.framebuffer->getPixel(x, y);
                    ppm.put(static_cast<char>(c.r));
                    ppm.put(static_cast<char>(c.g));
                    ppm.put(static_cast<char>(c.b));
                }
            }
            wrote = true;
        }
    }

    std::cout << "Draw calls: " << output.stats.draw_calls << std::endl;
    std::cout << "Triangles submitted: " << output.stats.triangles_submitted << std::endl;
    std::cout << "Triangles rasterized: " << output.stats.triangles_rasterized << std::endl;
    std::cout << "Pixels written: " << output.stats.pixels_written << std::endl;
    std::cout << "Frame time: " << output.stats.frame_time_ms << " ms" << std::endl;
    std::cout << (wrote ? ("Wrote " + out_path) : "PPM write FAILED") << std::endl;

    bool ok = wrote && output.errors.empty() &&
              output.stats.triangles_rasterized > 0 &&
              output.stats.pixels_written > 0;
    std::cout << "Headless demo " << (ok ? "OK" : "FAILED") << std::endl;
    return ok ? 0 : 2;
#endif
}
