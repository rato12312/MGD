#include "SkyrimPCAdapter.h"
#include "../analyzers/MeshAnalyzer.h"
#include "../analyzers/TextureAnalyzer.h"
#include "../analyzers/MetadataAnalyzer.h"
#include "../analyzers/BSAAnalyzer.h"
#include "../analyzers/ESPAnalyzer.h"

namespace mgd {

GameIdentity SkyrimPCAdapter::getIdentity() const {
    GameIdentity id;
    id.game_name = "The Elder Scrolls V: Skyrim Legendary Edition";
    id.series = "The Elder Scrolls";
    id.platform = "PC";
    id.source_format = "ESP/BSA/RAW";
    id.game_version = "1.9.32.0.8";
    id.content_version = "LE-DLC3";
    id.scanner_version = "0.2.0";
    id.mgd_format_version = "1.0";
    return id;
}

std::vector<std::string> SkyrimPCAdapter::getKnownExtensions() const {
    // Legendary Edition: base + Dawnguard.esm + Hearthfires.esm + Dragonborn.esm
    // BSA: Skyrim - Textures0..9.bsa, Update.bsa, Dawnguard.bsa etc.
    // + .esl para CC (obra-prima extensível), .pex scripts, .nif meshes
    return {".nif", ".dds", ".esp", ".esm", ".esl", ".bsa", ".pex", ".txt", ".json", ".xml", ".ini"};
}

bool SkyrimPCAdapter::canHandle(const GameIdentity& identity) const {
    return identity.platform == "PC" &&
           (identity.series == "The Elder Scrolls" || identity.game_name.find("Skyrim") != std::string::npos);
}

std::unique_ptr<IAssetAnalyzer> SkyrimPCAdapter::getAnalyzer(const FileInfo& file) {
    if (file.extension == ".nif") {
        return std::make_unique<MeshAnalyzer>();
    }
    if (file.extension == ".dds") {
        return std::make_unique<TextureAnalyzer>();
    }
    if (file.extension == ".bsa") {
        return std::make_unique<BSAAnalyzer>();
    }
    if (file.extension == ".esp" || file.extension == ".esm" || file.extension == ".esl") {
        return std::make_unique<ESPAnalyzer>();
    }
    if (file.extension == ".pex") {
        // Papyrus compilado — trata como SCRIPT, mas Metadata já cobre; usa BSAAnalyzer-like
        return std::make_unique<MetadataAnalyzer>();
    }
    return std::make_unique<MetadataAnalyzer>();
}

} // namespace mgd
