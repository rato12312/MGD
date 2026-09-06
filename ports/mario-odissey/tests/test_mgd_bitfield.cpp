#include <cassert>
#include <cstdint>
#include <cstdio>
#include "mgd/common/BitField.h"

// Mesma prova do teste do Eden, agora na versão MGD:
// ADD X2, X0, #5 = 0x91001402 -> Rd=2, Rn=0, imm12=5.
// B com offset negativo: 0x17FFFFFD -> off26 = -3.
union MgdAddImm {
    uint32_t hex;
    port::mgd::Field<0, 5, uint32_t> rd;
    port::mgd::Field<5, 5, uint32_t> rn;
    port::mgd::Field<10, 12, uint32_t> imm;
};

union MgdBranch {
    uint32_t hex;
    port::mgd::Field<0, 26, int32_t> off;
};

int main() {
    MgdAddImm insn;
    insn.hex = 0x91001402u;
    assert(static_cast<uint32_t>(insn.rd) == 2);
    assert(static_cast<uint32_t>(insn.rn) == 0);
    assert(static_cast<uint32_t>(insn.imm) == 5);

    MgdBranch b;
    b.hex = 0x17FFFFFDu; // B -12 bytes -> off26 = -3
    assert(static_cast<int32_t>(b.off) == -3);

    // escrita reflete no cru
    MgdAddImm w;
    w.hex = 0;
    w.rd = 7;
    assert((w.hex & 0x1F) == 7);
    std::printf("mgd bitfield tests passed!\n");
    return 0;
}
