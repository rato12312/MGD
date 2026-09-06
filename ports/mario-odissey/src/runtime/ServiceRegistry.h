#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace port {

// Registro mínimo de serviços: o Odyssey chama serviços do Switch;
// o port responde stubbed ou implementado, e registra o que foi tocado.
// Base para descobrir o conjunto mínimo real (pelos logs + execução).
enum class ServiceMode : uint8_t {
    Unknown = 0,
    Stubbed = 1,     // responde OK sem fazer nada (ex.: VI display)
    Implemented = 2, // implementação mínima real
};

struct ServiceInfo {
    std::string name;          // "fs", "set", "time", "audio", "vi", "am", "hid"
    ServiceMode mode = ServiceMode::Unknown;
    uint64_t call_count = 0;
};

class ServiceRegistry {
public:
    ServiceRegistry() {
        // Conjunto inicial provado pelos logs de boot no A15.
        declare("fs", ServiceMode::Implemented);    // saves, fontes, timezone
        declare("set", ServiceMode::Implemented);    // relógio, fuso
        declare("time", ServiceMode::Implemented);   // timezone binary
        declare("audio", ServiceMode::Implemented);  // cubeb stereo
        declare("vi", ServiceMode::Stubbed);         // display (stub no log)
        declare("am", ServiceMode::Implemented);     // window controller
        declare("hid", ServiceMode::Implemented);    // input
    }

    void declare(const std::string& name, ServiceMode mode) {
        services_[name] = ServiceInfo{name, mode, 0};
    }

    // Chamado a cada uso: conta e diz o modo. Desconhecido = registra como Unknown.
    ServiceMode call(const std::string& name) {
        auto it = services_.find(name);
        if (it == services_.end()) {
            services_[name] = ServiceInfo{name, ServiceMode::Unknown, 1};
            return ServiceMode::Unknown;
        }
        it->second.call_count++;
        return it->second.mode;
    }

    uint64_t calls(const std::string& name) const {
        auto it = services_.find(name);
        return it == services_.end() ? 0 : it->second.call_count;
    }

    std::vector<ServiceInfo> all() const {
        std::vector<ServiceInfo> out;
        for (const auto& kv : services_) out.push_back(kv.second);
        return out;
    }

    size_t size() const { return services_.size(); }

private:
    std::unordered_map<std::string, ServiceInfo> services_;
};

} // namespace port
