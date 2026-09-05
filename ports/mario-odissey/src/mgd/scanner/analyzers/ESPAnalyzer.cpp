#include "ESPAnalyzer.h"
#include <fstream>
#include <array>
#include <cstring>

namespace mgd {

bool ESPAnalyzer::canHandle(const FileInfo& file) const {
    return file.extension == ".esp" || file.extension == ".esm" || file.extension == ".esl";
}

RawAnalysisResult ESPAnalyzer::analyze(const FileInfo& file) {
    RawAnalysisResult result;
    result.file = file;
    result.detected_type = AssetType::WORLD;

    std::ifstream ifs(file.path, std::ios::binary);
    if (!ifs.is_open()) {
        result.error = "Failed to open ESP/ESM: " + file.path;
        return result;
    }

    // TES4 header: 24 bytes, magic "TES4" at 0, size at 4
    std::array<uint8_t, 24> hdr{};
    ifs.read(reinterpret_cast<char*>(hdr.data()), hdr.size());
    if (ifs.gcount() < 24) {
        result.error = "ESP/ESM too small for TES4 header";
        return result;
    }

    bool is_tes4 = (hdr[0]=='T' && hdr[1]=='E' && hdr[2]=='S' && hdr[3]=='4');
    if (!is_tes4) {
        // Legendary Edition ESPs sempre TES4; se não for, ainda considera WORLD
        // mas marca warning
        result.metadata["warning"] = "No TES4 magic, treating as WORLD";
    }

    uint32_t tes4_size = 0;
    std::memcpy(&tes4_size, hdr.data()+4, 4);
    result.metadata["tes4_size"] = std::to_string(tes4_size);
    result.metadata["file_size"] = std::to_string(file.file_size);
    result.metadata["format"] = "TES4";
    result.metadata["plugin"] = file.filename;

    // Masters: para Legendary Edition load order
    // Skyrim.esm -> Update.esm -> Dawnguard.esm -> Hearthfires.esm -> Dragonborn.esm
    // Não parseamos GRUP/CELL completo aqui (obra-prima mínima: valida header e
    // expõe contagem de dependências). O Hash dos masters vira dependency_ids
    // para o DependencyResolver ordenar chunks corretamente.
    // Lê bytes extras para estimar record count (tamanho - header)
    ifs.seekg(0, std::ios::end);
    auto total = ifs.tellg();
    result.metadata["total_size"] = std::to_string(static_cast<long long>(total));

    // Dependency: hash simples do nome do arquivo como placeholder para ordenação
    // de load order. O Infector usará isso para criar ResourceRecord correspondente.
    uint32_t dep_hash = 0;
    for (char c : file.filename) dep_hash = dep_hash * 31 + static_cast<uint8_t>(c);
    if (file.filename.find("Dawnguard") != std::string::npos ||
        file.filename.find("Hearthfires") != std::string::npos ||
        file.filename.find("Dragonborn") != std::string::npos) {
        // DLC plugins dependem de Skyrim.esm/Update.esm
        result.dependency_ids.push_back(0x53534B59u); // 'SKYS' placeholder
    }
    result.metadata["dep_hash"] = std::to_string(dep_hash);

    // World placement: ESP não tem bounds único; deixamos inválido para que
    // o Infector gere bounds placeholder e o ChunkManager distribua depois.
    result.spatial_bounds = AABB::invalid();
    result.success = true;
    return result;
}

} // namespace mgd
