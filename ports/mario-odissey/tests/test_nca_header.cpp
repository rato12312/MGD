#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include "../src/loader/NcaHeader.h"

static std::vector<uint8_t> makeNca() {
    std::vector<uint8_t> b(port::NcaHeader::SIZE, 0);
    b[0x200] = 'N'; b[0x201] = 'C'; b[0x202] = 'A'; b[0x203] = '3';
    b[0x204] = 1; // gamecard
    b[0x205] = 0; // program
    auto put32 = [&](size_t off, uint32_t v) { std::memcpy(b.data() + off, &v, 4); };
    put32(0x240, 0x10); put32(0x244, 0x20); // seção 0
    return b;
}

int main() {
    auto bytes = makeNca();
    auto h = port::NcaHeader::parse(bytes);
    assert(h.has_value() && h->valid);
    assert(h->distribution == 1);
    assert(h->content_type == 0);
    assert(h->sections[0].media_offset == 0x10);
    assert(h->sections[0].media_end_offset == 0x20);
    std::vector<uint8_t> small(16, 0);
    assert(!port::NcaHeader::parse(small).has_value());
    std::vector<uint8_t> wrong = bytes;
    wrong[0x200] = 'X';
    assert(!port::NcaHeader::parse(wrong).has_value());
    std::printf("nca header tests passed!\n");
    return 0;
}
