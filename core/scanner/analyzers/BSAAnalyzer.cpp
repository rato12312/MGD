#include "BSAAnalyzer.h"
#include <fstream>
#include <array>
#include <cstring>

namespace mgd {

bool BSAAnalyzer::canHandle(const FileInfo& file) const {
    return file.extension == ".bsa";
}

RawAnalysisResult BSAAnalyzer::analyze(const FileInfo& file) {
    RawAnalysisResult result;
    result.file = file;
    result.detected_type = AssetType::ARCHIVE;

    std::ifstream ifs(file.path, std::ios::binary);
    if (!ifs.is_open()) {
        result.error = "Failed to open BSA: " + file.path;
        return result;
    }

    std::array<uint8_t, 36> header{};
    ifs.read(reinterpret_cast<char*>(header.data()), header.size());
    if (ifs.gcount() < 36) {
        result.error = "BSA too small for header";
        return result;
    }

    // Skyrim BSA: magic "BSA\0" (0x00415342 LE) at 0, version 104 (0x68) at 4
    bool is_bsa = (header[0] == 'B' && header[1] == 'S' && header[2] == 'A' && header[3] == '\0');
    if (!is_bsa) {
        result.error = "Invalid BSA magic";
        return result;
    }

    uint32_t version = 0;
    std::memcpy(&version, header.data() + 4, 4);
    uint32_t dir_size = 0, file_count = 0, archive_flags = 0, file_flags = 0;
    std::memcpy(&dir_size, header.data() + 12, 4);
    std::memcpy(&file_count, header.data() + 16, 4);
    std::memcpy(&archive_flags, header.data() + 20, 4);
    std::memcpy(&file_flags, header.data() + 24, 4);

    result.metadata["magic"] = "BSA";
    result.metadata["version"] = std::to_string(version);
    result.metadata["file_count"] = std::to_string(file_count);
    result.metadata["dir_size"] = std::to_string(dir_size);
    result.metadata["archive_flags"] = std::to_string(archive_flags);
    result.metadata["file_flags"] = std::to_string(file_flags);
    result.metadata["file_size"] = std::to_string(file.file_size);
    result.metadata["format"] = "BSA";
    // DLC-aware: Skyrim.esm BSA vs Update.bsa vs Dawnguard.bsa etc.
    // O nome do arquivo já distingue; guardamos para Infector.
    result.metadata["bsa_name"] = file.filename;

    // BSA contém meshes/textures/scripts — dependências são internas,
    // não precisam de dependency_ids externos; mas registramos para cache.
    result.spatial_bounds = AABB::invalid();
    result.success = true;
    return result;
}

} // namespace mgd
