#pragma once

#include "IRenderResourceProvider.h"
#include <unordered_map>

namespace mgd {

class MockResourceProvider : public IRenderResourceProvider {
    std::unordered_map<MeshID, MeshData> meshes;
    std::unordered_map<TextureID, TextureData> textures;
    std::unordered_map<MaterialID, MaterialRecord> materials;

    MeshData createDefaultCube() const;
    MeshData createDefaultPlane() const;

public:
    MockResourceProvider();

    void addMesh(MeshID id, const MeshData& mesh);
    void addTexture(TextureID id, const TextureData& tex);
    void addMaterial(MaterialID id, const MaterialRecord& mat);

    const MeshData* getMesh(MeshID id) override;
    const TextureData* getTexture(TextureID id) override;
    const MaterialRecord* getMaterial(MaterialID id) override;
};

} // namespace mgd
