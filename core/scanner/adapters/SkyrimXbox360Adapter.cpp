#include "SkyrimXbox360Adapter.h"
#include "../analyzers/MeshAnalyzer.h"
#include "../analyzers/TextureAnalyzer.h"
#include "../analyzers/MetadataAnalyzer.h"

namespace mgd {

GameIdentity SkyrimXbox360Adapter::getIdentity() const {
    GameIdentity id;
    id.game_name = "The Elder Scrolls V: Skyrim";
    id.series = "The Elder Scrolls";
    id.platform = "Xbox 360";
    id.source_format = "Xbox 360 BE";
    id.game_version = "1.9.32.0.8";
    id.content_version = "UNKNOWN";
    id.scanner_version = "0.1.0";
    id.mgd_format_version = "1.0";
    return id;
}

std::vector<std::string> SkyrimXbox360Adapter::getKnownExtensions() const {
    return {".nif", ".dds", ".esp", ".esm", ".bsa", ".pex"};
}

bool SkyrimXbox360Adapter::canHandle(const GameIdentity& identity) const {
    return identity.platform == "Xbox 360" &&
           (identity.series == "The Elder Scrolls" || identity.game_name.find("Skyrim") != std::string::npos);
}

std::unique_ptr<IAssetAnalyzer> SkyrimXbox360Adapter::getAnalyzer(const FileInfo& file) {
    if (file.extension == ".nif") {
        return std::make_unique<MeshAnalyzer>();
    }
    if (file.extension == ".dds") {
        return std::make_unique<TextureAnalyzer>();
    }
    return std::make_unique<MetadataAnalyzer>();
}

} // namespace mgd
