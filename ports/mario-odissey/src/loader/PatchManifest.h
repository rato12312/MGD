#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace port {

// Manifesto de patches do port: NSO build IDs conhecidos do
// SUPER MARIO ODYSSEY v1.0.0 (extraídos de boot real no A15).
// O port só aplica patch ao que conhece; desconhecido = ignora com aviso.
struct NsoPatchEntry {
    std::string name;            // "rtld", "main", "subsdk0", "sdk"
    std::array<uint8_t, 32> build_id{};
    size_t id_len = 0;           // bytes significativos (logs mostram 16-20)
};

class PatchManifest {
public:
    PatchManifest() {
        add("rtld", "A75512BE30BB2A8C880177505D7A0B3E24E9D642");
        add("main", "3CA12DFAAF9C82DA064D1698DF79CDA1");
        add("subsdk0", "798C30E126697F2222CF843F78F5C1406287619A");
        add("sdk", "AE34E75D02925F4417B24499AD80C39412FC76DB");
    }

    std::optional<NsoPatchEntry> find(const uint8_t* build_id, size_t len) const {
        for (const auto& e : entries_) {
            if (len < e.id_len) continue;
            bool eq = true;
            for (size_t i = 0; i < e.id_len; ++i) {
                if (e.build_id[i] != build_id[i]) { eq = false; break; }
            }
            if (eq) return e;
        }
        return std::nullopt;
    }

    size_t size() const { return entries_.size(); }

private:
    static uint8_t hexNibble(char c) {
        if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
        if (c >= 'A' && c <= 'F') return static_cast<uint8_t>(c - 'A' + 10);
        if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(c - 'a' + 10);
        return 0;
    }

    void add(const std::string& name, const std::string& hex) {
        NsoPatchEntry e;
        e.name = name;
        e.build_id.fill(0);
        size_t n = 0;
        for (; n < 32 && n * 2 + 1 < hex.size(); ++n) {
            e.build_id[n] = static_cast<uint8_t>((hexNibble(hex[n * 2]) << 4) | hexNibble(hex[n * 2 + 1]));
        }
        e.id_len = n;
        entries_.push_back(e);
    }

    std::vector<NsoPatchEntry> entries_;
};

} // namespace port
