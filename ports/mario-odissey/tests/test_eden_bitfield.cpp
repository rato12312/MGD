#include <cassert>
#include <cstdint>
#include <cstdio>
#include "common/bit_field.h"

// Prova que o BitField vendorado do Eden decodifica instrução ARM64:
// ADD X2, X0, #5 = 0x91001402 -> Rd=2, Rn=0, imm12=5.
union AddImm {
    uint32_t hex;
    BitField<0, 5, uint32_t> rd;
    BitField<5, 5, uint32_t> rn;
    BitField<10, 12, uint32_t> imm;
};

int main() {
    AddImm insn;
    insn.hex = 0x91001402u;
    assert(static_cast<uint32_t>(insn.rd) == 2);
    assert(static_cast<uint32_t>(insn.rn) == 0);
    assert(static_cast<uint32_t>(insn.imm) == 5);
    // FormatValue monta de volta
    uint32_t rebuilt = AddImm::rd::FormatValue(2) | AddImm::rn::FormatValue(0) |
                       AddImm::imm::FormatValue(5);
    assert((rebuilt & 0x001FFFFFu) == (0x91001402u & 0x001FFFFFu));
    std::printf("eden bitfield tests passed!\n");
    return 0;
}
