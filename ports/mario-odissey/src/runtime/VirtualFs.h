#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace port {

// FS mínimo do port: resolvedor de caminhos virtuais do Switch
// (save:/, rom:/, sdcard:/ ...) para diretórios do host.
// Sem I/O real ainda: só mapeamento + normalização, testável puro.
class VirtualFs {
public:
    // Monta um prefixo virtual (ex.: "save:/") num diretório do host.
    void mount(const std::string& prefix, const std::string& hostDir) {
        mounts_[normPrefix(prefix)] = hostDir;
    }

    void unmount(const std::string& prefix) {
        mounts_.erase(normPrefix(prefix));
    }

    // Resolve "save:/MarioOdyssey/progress" -> "<host>/MarioOdyssey/progress".
    // Retorna nullopt se nenhum prefixo casar.
    std::optional<std::string> resolve(const std::string& virtualPath) const {
        auto [prefix, rest] = split(virtualPath);
        auto it = mounts_.find(prefix);
        if (it == mounts_.end()) return std::nullopt;
        if (rest.empty()) return it->second;
        std::string out = it->second;
        if (!out.empty() && out.back() != '/') out += '/';
        out += rest;
        return out;
    }

    std::vector<std::string> mounts() const {
        std::vector<std::string> out;
        for (const auto& kv : mounts_) out.push_back(kv.first);
        return out;
    }

private:
    static std::string normPrefix(const std::string& p) {
        // "save:/" e "save:" viram "save:"
        if (p.size() >= 2 && p[p.size() - 1] == '/' && p[p.size() - 2] == ':') {
            return p.substr(0, p.size() - 1);
        }
        return p;
    }

    static std::pair<std::string, std::string> split(const std::string& v) {
        auto pos = v.find(':');
        if (pos == std::string::npos) return {"", v};
        std::string prefix = v.substr(0, pos + 1);
        std::string rest = v.substr(pos + 1);
        while (!rest.empty() && rest.front() == '/') rest.erase(rest.begin());
        return {prefix, rest};
    }

    std::unordered_map<std::string, std::string> mounts_;
};

} // namespace port
