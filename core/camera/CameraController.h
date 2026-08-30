#pragma once

#include "ICameraController.h"
#include "../collision/CollisionSystem.h"
#include "../collision/queries/Raycast.h"

namespace mgd {

class CameraController : public ICameraController {
    CameraInput input;
    float sensitivity = 0.003f;
    float speed = 5.0f;
    CollisionSystem* collision = nullptr;

public:
    void update(Camera& camera, float dt) override;
    void setInput(const CameraInput& in);
    void setSensitivity(float s);
    void setSpeed(float s);

    // Obra-prima: câmera guiada por colisão — deduzindo com os arquivos do jogo
    // via RaycastSystem. A consciência (camera) sabe o que está na frente.
    void setCollisionSystem(CollisionSystem* cs) { collision = cs; }
    CollisionSystem* getCollisionSystem() const { return collision; }
};

} // namespace mgd
