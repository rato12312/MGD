# Peças necessárias do Eden para o port do Odyssey

Levantado dos logs de boot no A15 + `src/` do Eden. Regra: só entra o que o
Odyssey provar que usa.

## Loader (`src/core/loader/`)
- `nca.cpp/h`, `nso.cpp/h` (rtld, main, subsdk0, sdk carregados no boot)
- `nsp.cpp/h`, `xci.cpp/h` (formato do dump)
- `patch_manager` (`core/file_sys/`, patches ExeFS aplicados no boot)

## Serviços (`src/core/hle/service/`)
- `filesystem` + `system_archive` (saves, fontes, timezone, SystemVersion)
- `set` (relógio steady/network/user, fuso)
- `glue/time` (timezone binary)
- `audio` (cubeb, 2 streams 48kHz)
- `vi` (display, stubbed no log — candidato a MGD)
- `am` (window controller)
- `hid` (input toque/controle)

## GPU (`src/video_core/`, caminho Mali-amigável)
- `renderer_vulkan` com 1 viewport, sem clip planes, sem divisor de atributo
- `vk_pipeline_cache` (já com espelho/warmup/persist do MGD)
- Shader via MGD: 2D/LUT + warmup por região + cache perm

## Fora do port (por enquanto)
- Todo o resto de `service/` (amigos, loja, rede, etc.)
- Frontend Qt/SDL além do mínimo Android
- Testes e ferramentas do Eden

## Ordem
1. Loader + FS/SET/Time mínimos até carregar NSOs sem crash
2. Audio + Input mínimos
3. GPU Mali-amigável até primeiro frame
4. MGD assume apresentação (DNA, cache, incremental)
5. Medir no A15, travar 30

## Fronteira de execução (não extraível por peça)

O coração (`System::Run`, `Load`, `ExecuteProgram` em `src/core/core.h`,
com `cpu_manager`, `core_timing`, kernel e JIT) depende do Eden inteiro:
não dá para levar só ele ao port sem levar o emulador junto.
Estratégia mantida: modificar o Eden na base (já com MGD dentro) para o
jogo executar, e o port dedicado cresce em volta com loader, serviços
mínimos e apresentação MGD. Longo prazo, sem atalho.
