#include "MeshAnalyzer.h"
#include <fstream>
#include <array>

namespace mgd {

bool MeshAnalyzer::canHandle(const FileInfo& file) const {
    return file.extension == ".nif";
}

RawAnalysisResult MeshAnalyzer::analyze(const FileInfo& file) {
    RawAnalysisResult result;
    result.file = file;
    result.detected_type = AssetType::MESH;

    std::ifstream ifs(file.path, std::ios::binary);
    if (!ifs.is_open()) {
        result.error = "Failed to open file: " + file.path;
        return result;
    }

    std::array<uint8_t, 32> header{};
    ifs.read(reinterpret_cast<char*>(header.data()), 32);
    size_t bytes_read = static_cast<size_t>(ifs.gcount());

    if (bytes_read < 8) {
        result.error = "File too small to be a valid NIF";
        return result;
    }

    bool is_nif = false;

    if (header[0] == 'N' && header[1] == 'I' && header[2] == 'F' && header[3] == 'F') {
        is_nif = true;
    } else if (header[0] == 'B' && header[1] == 'S' && header[2] == 'N' && header[3] == 'I') {
        is_nif = true;
    }

    if (!is_nif) {
        result.error = "No NIF magic bytes found";
        return result;
    }

    result.metadata["magic"] = std::string(header.begin(), header.begin() + 4);
    result.metadata["file_size"] = std::to_string(file.file_size);
    result.metadata["format"] = "NIF";

    // TODO: Full NIF header parsing - version string, block counts, block types,
    //       triangle/vertex counts, NiNode tree traversal, AABB computation.
    //       Current implementation only validates magic bytes.

    result.spatial_bounds = AABB::invalid();
    result.success = true;
    return result;
}

} // namespace mgd
