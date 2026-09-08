# Emulador MGD — progresso (só Odyssey)

Regra: % sempre. Cálculo honesto por área.

## Placar (~12%)
- CPU inteira 64-bit: ~85% (ALU, shifts, bitfield, mult/div, branches, pilha)
- CPU inteira 32-bit: ~80% (ALU, shifts, bitfield, mult/div)
- CPU FP escalar (D+S): ~70% (aritmética, conversão, cmp, select)
- NEON vetorial: ~15% (FADD/SUB/MUL/DIV/MAX/MIN/CMP/MLA int+FP .2D/.4S, ORR, BSL, LDR/STR Q)
- MMU: ~20% (regiões + R/W/X + alias, sem paginação)
- SVC/HOS núcleo: ~12% (heap, query, map, perm, threads, sleep, eventos, mutex)
- Serviços: stubs honestos (sm, nvdrv, vi, audren, fsp, hid, time, apm, psm, lbl, set, fatal, pm)
- Loaders: NRO, NSO+LZ4, PFS0, RomFS, sonda NCA (sem descripto)
- Cripto: AES-ECB/CTR/CMAC + SHA-256 (com NIST)
- GPU execução: ~3% (fila + fence + dreno nulo)
- Mundo/painter: mapa mental + DNA + LOD + preset barato + PPM

## Para rodar o Odyssey falta (ordem)
1. NEON inteiro completo (8H/4S/16B em tudo) + resto FP
2. Exceções/IRQ + mais sysregs
3. NCA descripto (com chaves do usuário) + ExeFS
4. IPC over SVC (passar buffers de verdade)
5. GPU Maxwell executando (o gigante)
6. Applet manager + boot flow real

## Regras do projeto
- Peça nova só entra com teste verde no CI.
- Vale o medido, não o projetado.
- Encaixou MGD, otimiza; não encaixou, pula.
- Sem copiar asset: só IDs + posição.
