#include "SkyrimPCAdapter.h"
#include "../analyzers/MeshAnalyzer.h"
#include "../analyzers/TextureAnalyzer.h"
#include "../analyzers/MetadataAnalyzer.h"

namespace mgd {

GameIdentity SkyrimPCAdapter::getIdentity() const {
    GameIdentity id;
    id.game_name = "The Elder Scrolls V: Skyrim";
    id.series = "The Elder Scrolls";
    id.platform = "PC";
    id.source_format = "ESP/BSA/RAW";
    id.game_version = "UNKNOWN";
    id.content_version = "UNKNOWN";
    id.scanner_version = "0.1.0";
    id.mgd_format_version = "1.0";
    return id;
}

std::vector<std::string> SkyrimPCAdapter::getKnownExtensions() const {
    return {".nif", ".dds", ".esp", ".esm", ".bsa", ".pex", ".txt", ".json", ".xml"};
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
    return std::make_unique<MetadataAnalyzer>();
}

} // namespace mgd
