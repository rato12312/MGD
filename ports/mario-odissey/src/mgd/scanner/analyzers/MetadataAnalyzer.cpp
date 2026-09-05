#include "MetadataAnalyzer.h"
#include <algorithm>
#include <fstream>
#include <sstream>

namespace mgd {

bool MetadataAnalyzer::canHandle(const FileInfo& file) const {
    return file.extension == ".txt" ||
           file.extension == ".json" ||
           file.extension == ".xml" ||
           file.extension == ".ini";
}

RawAnalysisResult MetadataAnalyzer::analyze(const FileInfo& file) {
    RawAnalysisResult result;
    result.file = file;
    result.detected_type = AssetType::METADATA;

    std::ifstream ifs(file.path);
    if (!ifs.is_open()) {
        result.error = "Failed to open file: " + file.path;
        return result;
    }

    std::ostringstream ss;
    ss << ifs.rdbuf();
    std::string content = ss.str();

    result.metadata["extension"] = file.extension;
    result.metadata["file_size"] = std::to_string(file.file_size);
    result.metadata["line_count"] = std::to_string(
        std::count(content.begin(), content.end(), '\n') + (content.empty() || content.back() != '\n' ? 1 : 0));

    if (file.extension == ".json") {
        result.metadata["format"] = "JSON";
    } else if (file.extension == ".xml") {
        result.metadata["format"] = "XML";
    } else if (file.extension == ".ini") {
        result.metadata["format"] = "INI";
    } else {
        result.metadata["format"] = "TEXT";
    }

    result.spatial_bounds = AABB::invalid();
    result.success = true;
    return result;
}

} // namespace mgd
