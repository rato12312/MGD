#pragma once

#include <any>
#include <cstddef>
#include <functional>
#include <unordered_map>
#include <vector>

namespace mgd {

// Sem RTTI (Eden compila com -fno-rtti): a chave por tipo usa o endereço
// de um static por instanciação de template — único por EventT, sem typeid.
class EventManager {
public:
    EventManager() = default;

    template<typename EventT>
    using Callback = std::function<void(const EventT&)>;

    template<typename EventT>
    void subscribe(Callback<EventT> cb) {
        listeners_[typeKey<EventT>()].push_back([cb](const std::any& event) {
            cb(std::any_cast<const EventT&>(event));
        });
    }

    template<typename EventT>
    void emit(const EventT& event) {
        auto it = listeners_.find(typeKey<EventT>());
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
    template<typename EventT>
    static const void* typeKey() {
        static char tag{};
        return &tag;
    }

    std::unordered_map<const void*, std::vector<std::function<void(const std::any&)>>> listeners_;
};

} // namespace mgd
