#include "MockResourceProvider.h"

namespace mgd {

MeshData MockResourceProvider::createDefaultCube() const {
    MeshData mesh;

    mesh.vertices = {
        // Front face (z = +0.5), CCW winding when viewed from front
        {{-0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {0.0f, 0.0f}, {200, 200, 200, 255}},
        {{ 0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {1.0f, 0.0f}, {200, 200, 200, 255}},
        {{ 0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {1.0f, 1.0f}, {200, 200, 200, 255}},
        {{-0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {0.0f, 1.0f}, {200, 200, 200, 255}},

        // Back face (z = -0.5), CCW from back
        {{ 0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {0.0f, 0.0f}, {180, 180, 180, 255}},
        {{-0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {1.0f, 0.0f}, {180, 180, 180, 255}},
        {{-0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {1.0f, 1.0f}, {180, 180, 180, 255}},
        {{ 0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {0.0f, 1.0f}, {180, 180, 180, 255}},

        // Top face (y = +0.5), CCW from top
        {{-0.5f,  0.5f,  0.5f}, { 0.0f,  1.0f,  0.0f}, {0.0f, 0.0f}, {220, 220, 220, 255}},
        {{ 0.5f,  0.5f,  0.5f}, { 0.0f,  1.0f,  0.0f}, {1.0f, 0.0f}, {220, 220, 220, 255}},
        {{ 0.5f,  0.5f, -0.5f}, { 0.0f,  1.0f,  0.0f}, {1.0f, 1.0f}, {220, 220, 220, 255}},
        {{-0.5f,  0.5f, -0.5f}, { 0.0f,  1.0f,  0.0f}, {0.0f, 1.0f}, {220, 220, 220, 255}},

        // Bottom face (y = -0.5), CCW from bottom
        {{-0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f}, {0.0f, 0.0f}, {160, 160, 160, 255}},
        {{ 0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f}, {1.0f, 0.0f}, {160, 160, 160, 255}},
        {{ 0.5f, -0.5f,  0.5f}, { 0.0f, -1.0f,  0.0f}, {1.0f, 1.0f}, {160, 160, 160, 255}},
        {{-0.5f, -0.5f,  0.5f}, { 0.0f, -1.0f,  0.0f}, {0.0f, 1.0f}, {160, 160, 160, 255}},

        // Right face (x = +0.5), CCW from right
        {{ 0.5f, -0.5f,  0.5f}, { 1.0f,  0.0f,  0.0f}, {0.0f, 0.0f}, {190, 190, 190, 255}},
        {{ 0.5f, -0.5f, -0.5f}, { 1.0f,  0.0f,  0.0f}, {1.0f, 0.0f}, {190, 190, 190, 255}},
        {{ 0.5f,  0.5f, -0.5f}, { 1.0f,  0.0f,  0.0f}, {1.0f, 1.0f}, {190, 190, 190, 255}},
        {{ 0.5f,  0.5f,  0.5f}, { 1.0f,  0.0f,  0.0f}, {0.0f, 1.0f}, {190, 190, 190, 255}},

        // Left face (x = -0.5), CCW from left
        {{-0.5f, -0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 0.0f}, {170, 170, 170, 255}},
        {{-0.5f, -0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 0.0f}, {170, 170, 170, 255}},
        {{-0.5f,  0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 1.0f}, {170, 170, 170, 255}},
        {{-0.5f,  0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 1.0f}, {170, 170, 170, 255}},
    };

    mesh.indices = {
         0,  1,  2,   0,  2,  3,
         4,  5,  6,   4,  6,  7,
         8,  9, 10,   8, 10, 11,
        12, 13, 14,  12, 14, 15,
        16, 17, 18,  16, 18, 19,
        20, 21, 22,  20, 22, 23,
    };

    mesh.bounds = AABB({-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f});
    return mesh;
}

MeshData MockResourceProvider::createDefaultPlane() const {
    MeshData mesh;

    mesh.vertices = {
        {{-0.5f, 0.0f,  0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, {180, 180, 180, 255}},
        {{ 0.5f, 0.0f,  0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}, {180, 180, 180, 255}},
        {{ 0.5f, 0.0f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}, {180, 180, 180, 255}},
        {{-0.5f, 0.0f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}, {180, 180, 180, 255}},
    };

    mesh.indices = {0, 1, 2, 0, 2, 3};
    mesh.bounds = AABB({-0.5f, 0.0f, -0.5f}, {0.5f, 0.0f, 0.5f});
    return mesh;
}

MockResourceProvider::MockResourceProvider() {
    meshes[1] = createDefaultCube();
    meshes[2] = createDefaultPlane();
}

void MockResourceProvider::addMesh(MeshID id, const MeshData& mesh) {
    meshes[id] = mesh;
}

void MockResourceProvider::addTexture(TextureID id, const TextureData& tex) {
    textures[id] = tex;
}

void MockResourceProvider::addMaterial(MaterialID id, const MaterialRecord& mat) {
    materials[id] = mat;
}

const MeshData* MockResourceProvider::getMesh(MeshID id) {
    auto it = meshes.find(id);
    if (it != meshes.end()) return &it->second;
    if (id == INVALID_MESH_ID) {
        auto def = meshes.find(1);
        if (def != meshes.end()) return &def->second;
    }
    return nullptr;
}

const TextureData* MockResourceProvider::getTexture(TextureID id) {
    auto it = textures.find(id);
    if (it != textures.end()) return &it->second;
    return nullptr;
}

const MaterialRecord* MockResourceProvider::getMaterial(MaterialID id) {
    auto it = materials.find(id);
    if (it != materials.end()) return &it->second;
    return nullptr;
}

} // namespace mgd
