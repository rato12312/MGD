# Emulador MGD — como rodar os testes

Só Odyssey. Sem jogo, sem chave, sem firmware: tudo aqui é sintético.

## Linux / Codespace (g++)
```
git pull
bash build.sh
./build/mgd_tests
```
Verde = tudo certo. Vermelho = cola o primeiro `FAIL:` aqui.

## O que cada teste prova
- `EmulatorMachine`: CPU (int/FP/NEON), MMU, SVC, threads, HOS, loaders, cripto.
- `EmulatorOdyssey`: mundo boota, frame parado reaproveita, teto de RAM segura.
- `fib(10)=55`, `fat(5)=120`: a máquina computa de verdade.

## Rodar um programa
```cpp
mgd::emu::Emulator emu;
emu.loadProgram({0xD28000E0u /*MOVZ X0,#7*/, 0xD4000001u /*SVC#0*/}, 0);
emu.runCpu(16); // => X0=7, stopped
emu.bootWorld(8);
emu.frame("saida.ppm", 8); // bomba + dreno + mundo + PPM
```

## Regras
- Peça nova só entra com teste.
- Vale o medido, não o projetado.
- Dúvida de encoding? Não adivinha: deixa no log de desconhecidos.
