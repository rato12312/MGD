#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>
#include "../src/loader/NpdmHeader.h"

int main() {
    std::vector<uint8_t> good(port::NpdmHeader::MIN_SIZE, 0);
    good[0] = 'M'; good[1] = 'E'; good[2] = 'T'; good[3] = 'A';
    auto h = port::NpdmHeader::parse(good);
    assert(h.has_value() && h->valid);
    std::vector<uint8_t> small(16, 0);
    assert(!port::NpdmHeader::parse(small).has_value());
    std::vector<uint8_t> wrong = good;
    wrong[0] = 'X';
    assert(!port::NpdmHeader::parse(wrong).has_value());
    std::printf("npdm header tests passed!\n");
    return 0;
}
