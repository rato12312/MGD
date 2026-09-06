#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "../src/loader/PatchManifest.h"
#include "../src/loader/PatchCheck.h"
#include "../src/loader/PatchApplier.h"

static std::vector<uint8_t> hexBytes(const std::string& hex) {
    std::vector<uint8_t> out;
    auto nib = [](char c) -> uint8_t {
        if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
        if (c >= 'A' && c <= 'F') return static_cast<uint8_t>(c - 'A' + 10);
        return static_cast<uint8_t>(c - 'a' + 10);
    };
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        out.push_back(static_cast<uint8_t>((nib(hex[i]) << 4) | nib(hex[i + 1])));
    }
    return out;
}

int main() {
    port::PatchManifest manifest;
    assert(manifest.size() == 4);

    auto mainId = hexBytes("3CA12DFAAF9C82DA064D1698DF79CDA1");
    auto found = manifest.find(mainId.data(), mainId.size());
    assert(found.has_value() && found->name == "main");

    auto sdkId = hexBytes("AE34E75D02925F4417B24499AD80C39412FC76DB");
    auto foundSdk = manifest.find(sdkId.data(), sdkId.size());
    assert(foundSdk.has_value() && foundSdk->name == "sdk");

    std::vector<uint8_t> unknown(20, 0xFF);
    assert(!manifest.find(unknown.data(), unknown.size()).has_value());

    // Integração loader + manifesto: NSO com Build ID do main casa com "main"
    {
        std::vector<uint8_t> nso(0x100, 0);
        nso[0] = 'N'; nso[1] = 'S'; nso[2] = 'O'; nso[3] = '0';
        auto id = hexBytes("3CA12DFAAF9C82DA064D1698DF79CDA1");
        std::memcpy(nso.data() + 0x40, id.data(), id.size());
        auto m = port::matchNsoPatch(manifest, "main", nso);
        assert(m.has_value() && m->name == "main");
        // NSO desconhecido = nullopt (ignora, não quebra)
        std::vector<uint8_t> other(0x100, 0);
        other[0] = 'N'; other[1] = 'S'; other[2] = 'O'; other[3] = '0';
        other[0x40] = 0xFF;
        assert(!port::matchNsoPatch(manifest, "mystery", other).has_value());
    }

    // Aplicador: 4 NSOs conhecidos do Odyssey aplicam, 1 estranho pula
    {
        port::PatchManifest manifest;
        port::PatchApplier applier(manifest);
        auto nsoWith = [&](const std::string& hexId) {
            std::vector<uint8_t> nso(0x100, 0);
            nso[0] = 'N'; nso[1] = 'S'; nso[2] = 'O'; nso[3] = '0';
            auto id = hexBytes(hexId);
            std::memcpy(nso.data() + 0x40, id.data(), id.size());
            return nso;
        };
        assert(applier.apply(0x0100000000010000ull, "rtld", nsoWith("A75512BE30BB2A8C880177505D7A0B3E24E9D642")));
        assert(applier.apply(0x0100000000010000ull, "main", nsoWith("3CA12DFAAF9C82DA064D1698DF79CDA1")));
        assert(applier.apply(0x0100000000010000ull, "subsdk0", nsoWith("798C30E126697F2222CF843F78F5C1406287619A")));
        assert(applier.apply(0x0100000000010000ull, "sdk", nsoWith("AE34E75D02925F4417B24499AD80C39412FC76DB")));
        std::vector<uint8_t> strange(0x100, 0);
        strange[0] = 'N'; strange[1] = 'S'; strange[2] = 'O'; strange[3] = '0';
        strange[0x40] = 0xFF;
        assert(applier.apply(0x0100000000010000ull, "mystery", strange));
        assert(applier.applied().size() == 4);
        assert(applier.skipped().size() == 1);
    }

    std::printf("patch manifest tests passed!\n");
    return 0;
}
