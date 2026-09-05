#include "FileDiscovery.h"
#include <algorithm>
#include <cctype>
#include <filesystem>

namespace fs = std::filesystem;

namespace mgd {

namespace {

FileInfo makeFileInfo(const fs::directory_entry& entry) {
    FileInfo info;
    info.path = entry.path().string();
    info.filename = entry.path().filename().string();
    info.extension = entry.path().extension().string();

    std::error_code ec;
    auto last_write = fs::last_write_time(entry, ec);
    if (!ec) {
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            last_write - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
        info.last_modified = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(sctp.time_since_epoch()).count());
    }

    auto file_size = fs::file_size(entry, ec);
    info.file_size = ec ? 0 : static_cast<uint64_t>(file_size);

    return info;
}

} // anonymous namespace

std::vector<FileInfo> FileDiscovery::discover(const std::string& root_path) const {
    std::vector<FileInfo> results;

    std::error_code ec;
    if (!fs::is_directory(root_path, ec) || ec) {
        return results;
    }

    for (const auto& entry : fs::recursive_directory_iterator(root_path,
            fs::directory_options::skip_permission_denied, ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        if (!entry.is_regular_file()) {
            continue;
        }
        results.push_back(makeFileInfo(entry));
    }

    return results;
}

std::vector<FileInfo> FileDiscovery::discoverWithExtensions(const std::string& root_path,
                                                             const std::vector<std::string>& extensions) const {
    std::vector<FileInfo> results;

    std::error_code ec;
    if (!fs::is_directory(root_path, ec) || ec) {
        return results;
    }

    for (const auto& entry : fs::recursive_directory_iterator(root_path,
            fs::directory_options::skip_permission_denied, ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        if (!entry.is_regular_file()) {
            continue;
        }

        std::string ext = entry.path().extension().string();
        for (auto& c : ext) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }

        bool match = false;
        for (const auto& e : extensions) {
            std::string target = e;
            for (auto& c : target) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            if (ext == target) {
                match = true;
                break;
            }
        }

        if (match) {
            results.push_back(makeFileInfo(entry));
        }
    }

    return results;
}

} // namespace mgd
