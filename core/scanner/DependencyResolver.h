#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace mgd {

class DependencyResolver {
    std::unordered_map<uint32_t, std::vector<uint32_t>> dependencies;
    std::unordered_map<uint32_t, std::vector<uint32_t>> dependents;

public:
    void addDependency(uint32_t from, uint32_t to);
    std::vector<uint32_t> getDependencies(uint32_t id) const;
    std::vector<uint32_t> getDependents(uint32_t id) const;
    std::vector<uint32_t> resolve() const;
    void clear();
};

} // namespace mgd
