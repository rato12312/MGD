#include "DependencyResolver.h"
#include <algorithm>
#include <set>

namespace mgd {

void DependencyResolver::addDependency(uint32_t from, uint32_t to) {
    dependencies[from].push_back(to);
    dependents[to].push_back(from);
}

std::vector<uint32_t> DependencyResolver::getDependencies(uint32_t id) const {
    auto it = dependencies.find(id);
    if (it != dependencies.end()) {
        return it->second;
    }
    return {};
}

std::vector<uint32_t> DependencyResolver::getDependents(uint32_t id) const {
    auto it = dependents.find(id);
    if (it != dependents.end()) {
        return it->second;
    }
    return {};
}

std::vector<uint32_t> DependencyResolver::resolve() const {
    std::set<uint32_t> all_nodes;
    for (const auto& [from, deps] : dependencies) {
        all_nodes.insert(from);
        for (auto to : deps) {
            all_nodes.insert(to);
        }
    }
    for (const auto& [to, deps] : dependents) {
        all_nodes.insert(to);
        for (auto from : deps) {
            all_nodes.insert(from);
        }
    }

    std::unordered_map<uint32_t, uint32_t> in_degree;
    for (auto node : all_nodes) {
        in_degree[node] = 0;
    }
    for (const auto& [from, deps] : dependencies) {
        for (auto to : deps) {
            in_degree[to]++;
        }
    }

    std::vector<uint32_t> queue;
    for (auto node : all_nodes) {
        if (in_degree[node] == 0) {
            queue.push_back(node);
        }
    }

    std::vector<uint32_t> sorted;
    sorted.reserve(all_nodes.size());

    size_t idx = 0;
    while (idx < queue.size()) {
        uint32_t current = queue[idx++];
        sorted.push_back(current);

        auto it = dependencies.find(current);
        if (it != dependencies.end()) {
            for (auto neighbor : it->second) {
                in_degree[neighbor]--;
                if (in_degree[neighbor] == 0) {
                    queue.push_back(neighbor);
                }
            }
        }
    }

    return sorted;
}

void DependencyResolver::clear() {
    dependencies.clear();
    dependents.clear();
}

} // namespace mgd
