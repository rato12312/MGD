#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "../src/loader/Pfs0.h"

// Monta um PFS0 válido à mão: 2 arquivos ("main.npdm", "main").
static std::vector<uint8_t> makePfs0() {
    std::string names("main.npdm\0main\0", 15);
    uint32_t count = 2;
    size_t header = 16 + count * 16;
    std::vector<uint8_t> b(header + names.size(), 0);
    auto put32 = [&](size_t off, uint32_t v) { std::memcpy(b.data() + off, &v, 4); };
    b[0] = 'P'; b[1] = 'F'; b[2] = 'S'; b[3] = '0';
    put32(4, count);
    put32(8, static_cast<uint32_t>(names.size()));
    // entry 0: offset 0, size 100, name_off 0
    put32(16, 0); put32(20, 100); put32(24, 0);
    // entry 1: offset 100, size 200, name_off 10 ("main")
    put32(32, 100); put32(36, 200); put32(40, 10);
    std::memcpy(b.data() + header, names.data(), names.size());
    return b;
}

int main() {
    auto bytes = makePfs0();
    auto p = port::Pfs0::parse(bytes);
    assert(p.has_value() && p->valid);
    assert(p->files.size() == 2);
    assert(p->files[0].name == "main.npdm");
    assert(p->files[0].offset == 0 && p->files[0].size == 100);
    assert(p->files[1].name == "main");
    assert(p->files[1].offset == 100 && p->files[1].size == 200);
    const auto* f = p->find("main");
    assert(f && f->size == 200);
    assert(p->find("nada") == nullptr);

    std::vector<uint8_t> bad(8, 0);
    assert(!port::Pfs0::parse(bad).has_value());
    std::vector<uint8_t> wrong = bytes;
    wrong[0] = 'X';
    assert(!port::Pfs0::parse(wrong).has_value());
    // name_off fora da string table
    std::vector<uint8_t> corrupt = bytes;
    corrupt[40] = 0xFF;
    assert(!port::Pfs0::parse(corrupt).has_value());

    std::printf("pfs0 tests passed!\n");
    return 0;
}
