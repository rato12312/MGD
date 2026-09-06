#include <cassert>
#include <cstdio>
#include <vector>
#include "../src/loader/Yaz0.h"

int main() {
    // "AB" + match(dist=2, count=4) = "ABABAB"
    std::vector<uint8_t> bytes(16, 0);
    bytes[0] = 'Y'; bytes[1] = 'a'; bytes[2] = 'z'; bytes[3] = '0';
    bytes[7] = 6;
    bytes.push_back(0xC0); // literal, literal, match, ...
    bytes.push_back('A');
    bytes.push_back('B');
    bytes.push_back(0x20); // count = 2+2 = 4
    bytes.push_back(0x01); // dist = 2
    auto out = port::Yaz0::decompress(bytes);
    assert(out.has_value());
    assert(out->size() == 6);
    assert((*out)[0] == 'A' && (*out)[1] == 'B');
    assert((*out)[2] == 'A' && (*out)[3] == 'B');
    assert((*out)[4] == 'A' && (*out)[5] == 'B');

    // match longo (nibble 0 -> terceiro byte): "X" x10 via cópias
    std::vector<uint8_t> bytes2(16, 0);
    bytes2[0] = 'Y'; bytes2[1] = 'a'; bytes2[2] = 'z'; bytes2[3] = '0';
    bytes2[7] = 10;
    bytes2.push_back(0x80); // 1 literal + resto match
    bytes2.push_back('X');
    bytes2.push_back(0x00); // count longo
    bytes2.push_back(0x00); // dist = 1
    bytes2.push_back(0xF7); // count = 0xF7 + 0x12 = 264 -> corta em 9 restantes
    auto out2 = port::Yaz0::decompress(bytes2);
    assert(out2.has_value());
    assert(out2->size() == 10);
    for (auto c : *out2) assert(c == 'X');

    // inválidos
    std::vector<uint8_t> bad(8, 0);
    assert(!port::Yaz0::decompress(bad).has_value());
    std::vector<uint8_t> wrong = bytes;
    wrong[0] = 'Q';
    assert(!port::Yaz0::decompress(wrong).has_value());

    std::printf("yaz0 tests passed!\n");
    return 0;
}
