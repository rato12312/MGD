#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mgd {

struct FileInfo {
    std::string path;
    std::string filename;
    std::string extension;
    uint64_t file_size = 0;
    uint64_t last_modified = 0;
};

class FileDiscovery {
public:
    std::vector<FileInfo> discover(const std::string& root_path) const;
    std::vector<FileInfo> discoverWithExtensions(const std::string& root_path, const std::vector<std::string>& extensions) const;
};

} // namespace mgd
