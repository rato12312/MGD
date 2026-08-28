#include <iostream>
#include <memory>
#include <cmath>
#include <cstring>

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

        auto all = mental_map_.getActiveEntities();
        for (EntityID eid : all) {
            auto ent = mental_map_.getEntity(eid);
            if (!ent) continue;
            const MentalEntity& entity = ent->get();
            CollisionShapeData shape;
            shape.id = static_cast<CollisionID>(entity.id);
            shape.type = ShapeType::AABB;
            shape.center = entity.bounds.aabb.center();
            shape.half_extents = entity.bounds.aabb.extents();
            collision_.addShape(shape.id, shape);
        }
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
    std::cout << "MGD Engine requires SDL2 to run the prototype application." << std::endl;
    std::cout << "Build with SDL2 to enable the full application." << std::endl;
    return 0;
#endif
}
