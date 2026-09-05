#pragma once

#include <any>
#include <functional>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace mgd {

class EventManager {
public:
    EventManager() = default;

    template<typename EventT>
    using Callback = std::function<void(const EventT&)>;

    template<typename EventT>
    void subscribe(Callback<EventT> cb) {
        auto key = std::type_index(typeid(EventT));
        listeners_[key].push_back([cb](const std::any& event) {
            cb(std::any_cast<const EventT&>(event));
        });
    }

    template<typename EventT>
    void emit(const EventT& event) {
        auto key = std::type_index(typeid(EventT));
        auto it = listeners_.find(key);
        if (it != listeners_.end()) {
            std::any wrapped = event;
            for (auto& listener : it->second) {
                listener(wrapped);
            }
        }
    }

    void clear() {
        listeners_.clear();
    }

private:
    std::unordered_map<std::type_index, std::vector<std::function<void(const std::any&)>>> listeners_;
};

} // namespace mgd
