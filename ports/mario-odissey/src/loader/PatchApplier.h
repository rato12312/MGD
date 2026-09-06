#pragma once

#include "PatchCheck.h"
#include <cstdint>
#include <string>
#include <vector>

namespace port {

// Aplicador mínimo de patches ExeFS por título.
// Para cada NSO (nome + bytes do cabeçalho): casa no manifesto e registra
// como aplicado; desconhecido é ignorado com aviso (não quebra o boot).
// Equivale ao "LayeredExeFS patches applied successfully" do log.
struct AppliedPatch {
    std::string nso_name;
    std::string patch_name;
};

class PatchApplier {
public:
    explicit PatchApplier(const PatchManifest& manifest) : manifest_(manifest) {}

    // Retorna true se algum patch casou (ou se não havia o que casar).
    // Desconhecidos vão para skipped().
    bool apply(uint64_t title_id, const std::string& nso_name,
               const std::vector<uint8_t>& nsoBytes) {
        (void)title_id; // reservado para overrides por título no futuro
        auto m = matchNsoPatch(manifest_, nso_name, nsoBytes);
        if (!m) {
            skipped_.push_back(nso_name);
            return true; // ignorar não é erro
        }
        applied_.push_back(AppliedPatch{nso_name, m->name});
        return true;
    }

    const std::vector<AppliedPatch>& applied() const { return applied_; }
    const std::vector<std::string>& skipped() const { return skipped_; }
    void clear() { applied_.clear(); skipped_.clear(); }

private:
    const PatchManifest& manifest_;
    std::vector<AppliedPatch> applied_;
    std::vector<std::string> skipped_;
};

} // namespace port
