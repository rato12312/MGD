#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace port {

// Capacidades da GPU tiradas do log real do A15 (Mali-G57 MC2, Vulkan 1.3.303).
// Para cada recurso que a Mali NÃO tem, o port define um fallback em MGD
// em vez de falhar: o "impossível" vira caminho alternativo.
enum class GpuFallback : uint8_t {
    None = 0,        // recurso existe, segue normal
    SingleViewport,  // multiViewport/maxViewports: trava em 1 + scissor
    NoClipPlanes,    // shaderClip/CullDistance: descarta por CPU (frustum MGD)
    CpuDivisor,      // vertex_attribute_divisor: expande instâncias na CPU
    CpuIndirect,     // multiDrawIndirect: emite draws diretos
    WireframeOff,    // fillModeNonSolid: força sólido
    NoAtomics,       // vertexPipelineStoresAndAtomics: resolve na CPU
};

struct GpuRequirement {
    std::string name;       // ex.: "multiViewport"
    bool available_on_mali_g57 = false;
    GpuFallback fallback = GpuFallback::None;
};

class GpuCaps {
public:
    GpuCaps() {
        // Levantado do log do A15 (GetSuitability). Tudo que falta tem fallback.
        add("VK_EXT_vertex_attribute_divisor", false, GpuFallback::CpuDivisor);
        add("fillModeNonSolid", false, GpuFallback::WireframeOff);
        add("multiDrawIndirect", false, GpuFallback::CpuIndirect);
        add("multiViewport", false, GpuFallback::SingleViewport);
        add("shaderClipDistance", false, GpuFallback::NoClipPlanes);
        add("shaderCullDistance", false, GpuFallback::NoClipPlanes);
        add("vertexPipelineStoresAndAtomics", false, GpuFallback::NoAtomics);
        add("maxViewports16", false, GpuFallback::SingleViewport);
        add("maxClipDistances8", false, GpuFallback::NoClipPlanes);
        // O que a G57 tem (do log): segue normal.
        add("geometryShaders", true, GpuFallback::None);
        add("anisotropicFiltering", true, GpuFallback::None);
        add("spirv14", true, GpuFallback::None);
    }

    // Retorna o fallback para um recurso, ou None se existe.
    GpuFallback fallbackFor(const std::string& name) const {
        for (const auto& r : reqs_) {
            if (r.name == name) return r.available_on_mali_g57 ? GpuFallback::None : r.fallback;
        }
        return GpuFallback::None; // desconhecido = tenta normal
    }

    // true se o recurso precisa de caminho alternativo na Mali.
    bool needsFallback(const std::string& name) const {
        return fallbackFor(name) != GpuFallback::None;
    }

    size_t missingCount() const {
        size_t n = 0;
        for (const auto& r : reqs_) {
            if (!r.available_on_mali_g57) ++n;
        }
        return n;
    }

    const std::vector<GpuRequirement>& all() const { return reqs_; }

private:
    void add(const std::string& name, bool avail, GpuFallback fb) {
        reqs_.push_back(GpuRequirement{name, avail, fb});
    }

    std::vector<GpuRequirement> reqs_;
};

} // namespace port
