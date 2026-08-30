#include "CameraController.h"
#include "Camera.h"
#include <cmath>
#include <algorithm>

namespace mgd {

static constexpr float DEG_TO_RAD = 3.14159265358979323846f / 180.0f;
static constexpr float PITCH_LIMIT = 89.0f * DEG_TO_RAD;

void CameraController::update(Camera& camera, float dt) {
    camera.rotateYaw(input.yaw_delta * sensitivity);
    camera.rotatePitch(input.pitch_delta * sensitivity);
    camera.rotateRoll(input.roll_delta * sensitivity);

    float move_speed = input.speed * speed * dt;

    // Obra-prima: câmera guiada por colisão — antes de mover, deduz via Raycast
    // se há parede na frente. Se houver, reduz o movimento (slide) para não
    // atravessar. Usa arquivos do jogo (CollisionSystem) como fonte.
    auto tryMove = [&](Vec3 dir, float amount) {
        if (!collision || std::abs(amount) < 1e-5f) {
            if (dir.x == 0 && dir.y == 0 && std::abs(dir.z) > 0.9f) {
                // forward/backward sem colisão
            }
            return false;
        }
        Vec3 start = camera.getState().position;
        Vec3 end = start + dir * amount;
        Ray ray(start, dir);
        RaycastSystem rc;
        RayHit hit = rc.raycast(ray, std::abs(amount) + 0.2f, *collision);
        if (hit.hit && hit.distance < std::abs(amount) + 0.1f) {
            // Tem parede na frente — desliza, não atravessa (obra-prima)
            float safe = std::max(0.0f, hit.distance - 0.05f);
            if (safe > 0.01f) {
                dir = dir * (safe / std::abs(amount));
                // Caller vai aplicar o movimento reduzido
            } else {
                return true; // bloqueado
            }
        }
        return false;
    };

    if (input.forward != 0.0f) {
        Vec3 fwd = camera.getForward();
        Vec3 dir = fwd * (input.forward > 0 ? 1.0f : -1.0f);
        float amount = input.forward * move_speed;
        bool blocked = false;
        if (collision) {
            Ray ray(camera.getState().position, dir);
            RaycastSystem rc;
            RayHit hit = rc.raycast(ray, std::abs(amount) + 0.2f, *collision);
            if (hit.hit && hit.distance < std::abs(amount) + 0.1f) {
                float safe = std::max(0.0f, hit.distance - 0.05f);
                if (safe > 0.01f) amount = (amount > 0 ? safe : -safe);
                else blocked = true;
            }
        }
        if (!blocked) {
            if (input.forward > 0) camera.moveForward(amount);
            else camera.moveBackward(-amount);
        }
    }
    if (input.strafe != 0.0f) {
        Vec3 right = camera.getRight();
        Vec3 dir = right * (input.strafe > 0 ? 1.0f : -1.0f);
        float amount = input.strafe * move_speed;
        bool blocked = false;
        if (collision) {
            Ray ray(camera.getState().position, dir);
            RaycastSystem rc;
            RayHit hit = rc.raycast(ray, std::abs(amount) + 0.2f, *collision);
            if (hit.hit && hit.distance < std::abs(amount) + 0.1f) {
                float safe = std::max(0.0f, hit.distance - 0.05f);
                if (safe > 0.01f) amount = (amount > 0 ? safe : -safe);
                else blocked = true;
            }
        }
        if (!blocked) camera.moveRight(amount);
    }
    if (input.vertical != 0.0f) {
        // Vertical usa world up, mas também respeita teto/chão via colisão
        float amount = input.vertical * move_speed;
        bool blocked = false;
        if (collision) {
            Vec3 dir = Vec3(0,1,0) * (amount > 0 ? 1.0f : -1.0f);
            Ray ray(camera.getState().position, dir);
            RaycastSystem rc;
            RayHit hit = rc.raycast(ray, std::abs(amount) + 0.2f, *collision);
            if (hit.hit && hit.distance < std::abs(amount) + 0.1f) {
                float safe = std::max(0.0f, hit.distance - 0.05f);
                if (safe > 0.01f) amount = (amount > 0 ? safe : -safe);
                else blocked = true;
            }
        }
        if (!blocked) camera.moveUp(amount);
    }
}

void CameraController::setInput(const CameraInput& in) {
    input = in;
}

void CameraController::setSensitivity(float s) {
    sensitivity = s;
}

void CameraController::setSpeed(float s) {
    speed = s;
}

} // namespace mgd
