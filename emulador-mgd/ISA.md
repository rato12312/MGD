# ISA coberta (CPU MGD) — mapa + ordem de decode

Ordem importa: primeiro que casa, executa. Máscaras auditadas
(regra: valor não tem bit fora da máscara).

## Controle
SVC, B, BL, B.cond, CBZ/CBNZ, TBZ/TBNZ, RET, NOP/HINT, DMB/DSB/ISB,
MSR-imm, MRS/MSR TPIDR_EL0/CNTVCT_EL0.

## Inteira 64/32
MOVZ/MOVK/MOVN, ADD/SUB/ADDS/SUBS (imm+reg, 64+32, SP certo),
AND/ORR/EOR/BIC/ORN/EON + ANDS (+32), UBFM/SBFM (64+32),
LSL/LSR/ASR/ROR (imm+reg, 64+32), MADD/MSUB/MUL, SMADDL/UMADDL,
MADDW, SDIV/UDIV (64+32), ADCS/SBCS, CSEL/CSINC/CSINV/CSNEG (64+32),
CCMN/CCMP (64+32), REV*/CLZ/RBIT/EXTR/CRC32, SMULH? não. MULW via MADDW.

## Memória
LDR/STR 64/32/8 + S/U + literal + reg-offset + LDUR/STUR (todos),
LDRSW, LDP/STP X/W/D/S/Q (offset/pre/post), LDAR/STLR/LDAXR/STLXR/SWPAL,
PRFM (aceita), DC ZVA (zera) + CVAC/CIVAC/IVAC/IC-IVAU (aceita).

## FP escalar (D+S) e NEON (.2D + int size-aware)
FADD/SUB/MUL/DIV/MAX/MIN/NMUL, FMADD/MSUB, FSQRT/NEG/ABS,
FCMP/FCSEL, FCVT ambas direções, SCVTF/UCVTF ambas, FCVTZS/ZU (W),
FMOV D + FMOV X(bits), LDR/STR D/S/Q, STP/LDP D/S/Q,
FADD/SUB/MUL/DIV/MAX/MIN/CMP/MLA vector, ADD/SUB/MUL int vector,
ORR/BSL/FCMEQ, DUP? não. FRINT? não. CAS? não. PAC/BTI? não (v8.0).

## Log de desconhecido
Tudo que cai no fim é registrado (opcode + conta) — é a fila do próximo.
