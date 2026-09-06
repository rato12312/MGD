#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include "../src/loader/NsoHeader.h"

static std::vector<uint8_t> makeNso() {
    std::vector<uint8_t> b(port::NsoHeader::SIZE, 0);
    b[0] = 'N'; b[1] = 'S'; b[2] = 'O'; b[3] = '0';
    auto put32 = [&](size_t off, uint32_t v) { std::memcpy(b.data() + off, &v, 4); };
    put32(4, 0);            // version
    put32(0x10, 0x100); put32(0x14, 0x1000); put32(0x18, 0x2000); // .text
    put32(0x3C, 0x500);     // bss
    b[0x40] = 0xAB;         // build id byte 0
    return b;
}

int main() {
    auto bytes = makeNso();
    auto h = port::NsoHeader::parse(bytes);
    assert(h.has_value() && h->valid);
    assert(h->text.file_offset == 0x100);
    assert(h->text.memory_offset == 0x1000);
    assert(h->text.decompressed_size == 0x2000);
    assert(h->bss_size == 0x500);
    assert(h->build_id[0] == 0xAB);
    std::vector<uint8_t> bad(16, 0);
    assert(!port::NsoHeader::parse(bad).has_value());
    std::vector<uint8_t> wrong = bytes;
    wrong[0] = 'X';
    assert(!port::NsoHeader::parse(wrong).has_value());
    std::printf("nso header tests passed!\n");
    return 0;
}
