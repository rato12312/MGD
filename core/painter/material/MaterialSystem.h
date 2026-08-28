#pragma once

#include "MaterialRecord.h"
#include <unordered_map>

namespace mgd {

class MaterialSystem {
    std::unordered_map<MaterialID, MaterialRecord> materials;

public:
    void store(const MaterialRecord& mat);
    const MaterialRecord* get(MaterialID id) const;
    MaterialRecord getDefault() const;
    bool has(MaterialID id) const;
    void clear();
};

} // namespace mgd
