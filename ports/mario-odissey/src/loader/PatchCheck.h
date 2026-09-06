#pragma once

#include "NsoHeader.h"
#include "PatchManifest.h"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace port {

// Liga loader + manifesto: dado um NSO (bytes do cabeçalho) e seu nome,
// extrai o Build ID e confere no manifesto (equivale ao HasNSOPatch do log).
// Desconhecido = nullopt (ignora com aviso, não quebra o boot).
inline std::optional<NsoPatchEntry> matchNsoPatch(const PatchManifest& manifest,
                                                 const std::string& name,
                                                 const std::vector<uint8_t>& nsoBytes) {
    (void)name; // reservado para log/override por nome no futuro
    auto h = NsoHeader::parse(nsoBytes);
    if (!h) return std::nullopt;
    return manifest.find(h->build_id, sizeof(h->build_id));
}

} // namespace port
