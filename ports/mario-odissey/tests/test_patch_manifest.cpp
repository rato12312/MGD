#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "../src/loader/PatchManifest.h"

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

    std::printf("patch manifest tests passed!\n");
    return 0;
}
