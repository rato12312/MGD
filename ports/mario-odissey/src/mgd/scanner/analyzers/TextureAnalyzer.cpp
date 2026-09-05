#include "TextureAnalyzer.h"
#include <fstream>
#include <cstring>
#include <array>

namespace mgd {

namespace {

struct DDSHeader {
    uint32_t magic = 0;
    uint32_t size = 0;
    uint32_t flags = 0;
    uint32_t height = 0;
    uint32_t width = 0;
    uint32_t pitch_or_linear_size = 0;
    uint32_t depth = 0;
    uint32_t mip_map_count = 0;
};

} // anonymous namespace

bool TextureAnalyzer::canHandle(const FileInfo& file) const {
    return file.extension == ".dds";
}

RawAnalysisResult TextureAnalyzer::analyze(const FileInfo& file) {
    RawAnalysisResult result;
    result.file = file;
    result.detected_type = AssetType::TEXTURE;

    std::ifstream ifs(file.path, std::ios::binary);
    if (!ifs.is_open()) {
        result.error = "Failed to open file: " + file.path;
        return result;
    }

    DDSHeader header{};
    ifs.read(reinterpret_cast<char*>(&header), sizeof(DDSHeader));

    if (ifs.gcount() < static_cast<std::streamsize>(sizeof(DDSHeader))) {
        result.error = "File too small for DDS header";
        return result;
    }

    constexpr uint32_t DDS_MAGIC = 0x20534444;
    if (header.magic != DDS_MAGIC) {
        result.error = "Invalid DDS magic number";
        return result;
    }

    if (header.size != 124) {
        result.error = "Unexpected DDS header size: " + std::to_string(header.size);
        return result;
    }

    result.metadata["format"] = "DDS";
    result.metadata["width"] = std::to_string(header.width);
    result.metadata["height"] = std::to_string(header.height);
    result.metadata["mip_count"] = std::to_string(header.mip_map_count);
    result.metadata["file_size"] = std::to_string(file.file_size);

    result.spatial_bounds = AABB(Vec3(0, 0, 0), Vec3(static_cast<float>(header.width),
                                                       static_cast<float>(header.height), 0));
    result.success = true;
    return result;
}

} // namespace mgd
