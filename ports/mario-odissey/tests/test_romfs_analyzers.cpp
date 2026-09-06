#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>
#include "../src/loader/RomfsAnalyzers.h"

static std::vector<uint8_t> makeSized(size_t n, const char* magic4) {
    std::vector<uint8_t> b(n, 0);
    for (int i = 0; i < 4 && magic4[i]; ++i) b[i] = static_cast<uint8_t>(magic4[i]);
    return b;
}

int main() {
    auto sarc = makeSized(0x20, "SARC");
    auto r1 = port::analyzeRomfs(".sarc", sarc);
    assert(r1.has_value() && r1->format == "SARC");

    auto yaz0 = makeSized(0x20, "Yaz0");
    auto r2 = port::analyzeRomfs(".szs", yaz0);
    assert(r2.has_value() && r2->format == "YAZ0");

    std::vector<uint8_t> byml(8, 0);
    byml[0] = 'B'; byml[1] = 'Y'; byml[2] = 3; byml[3] = 0;
    auto r3 = port::analyzeRomfs(".byml", byml);
    assert(r3.has_value() && r3->format == "BYML" && r3->version == 3);

    auto bfres = makeSized(0x20, "FRES");
    auto r4 = port::analyzeRomfs(".bfres", bfres);
    assert(r4.has_value() && r4->format == "BFRES");

    auto bntx = makeSized(0x20, "BNTX");
    auto r5 = port::analyzeRomfs(".bntx", bntx);
    assert(r5.has_value() && r5->format == "BNTX");

    std::vector<uint8_t> junk(0x20, 0xFF);
    assert(!port::analyzeRomfs(".sarc", junk).has_value());
    assert(!port::analyzeRomfs(".xyz", sarc).has_value());
    std::vector<uint8_t> tiny(2, 0);
    assert(!port::analyzeRomfs(".byml", tiny).has_value());

    std::printf("romfs analyzers tests passed!\n");
    return 0;
}
