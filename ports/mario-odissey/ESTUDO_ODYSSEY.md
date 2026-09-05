# Estudo — Super Mario Odyssey no A15 (Mali-G57)

Aparelho: Galaxy A15 (SM-A155M, MT6789, Mali-G57 MC2, Vulkan 1.3.303, driver 54.1.0, 4 GB RAM).
Jogo: SUPER MARIO ODYSSEY v1.0.0 (0100000000010000), firmware NX 22.5.0-1.0.

## O que o jogo toca (pelos logs do Eden)

- Loader: NCA/NSO (`rtld`, `main`, `subsdk0`, `sdk`), patches ExeFS aplicados
- FS: saves, fontes (todas sintetizadas), timezone, `SystemVersion`
- SET: relógio (steady/network/user), fuso, vibração
- Time, Audio (cubeb, 2 streams 48kHz), Input (UDP)
- VI display (stubbed), AM window controller
- GPU/Vulkan com overrides do jogo para Mali (`LoadOverrides ... GPU vendor 5 (Mali)`)

## Onde quebra no Eden

1. Mali declarada "unsuitable": sem `VK_EXT_vertex_attribute_divisor`,
   `fillModeNonSolid`, `multiDrawIndirect`, `multiViewport`,
   `shaderClipDistance/CullDistance`, `vertexPipelineStoresAndAtomics`;
   `maxViewports` 1 (precisa 16), `maxClipDistances` 0 (precisa 8).
2. `Total Pipeline Count: 0` — nada em cache, tudo compila na hora.
3. Spam de `GetView` sem mutable format (caro na Mali).
4. Crash final: segfault em `SvcSendSyncRequest`, processo morto.

## Peças necessárias do port (rascunho)

- [ ] Loader mínimo (NCA/NSO + patches)
- [ ] FS mínimo (save, fontes, timezone, versão)
- [ ] SET/Time mínimos
- [ ] Audio mínimo (cubeb stereo)
- [ ] Input (toque + controle)
- [ ] GPU: caminho Mali-amigável (1 viewport, sem clip planes, sem divisor)
- [ ] Shader: 2D via LUT + warmup + cache perm (MGD)
- [ ] Apresentação via MGD (incremental + LOD)

## Regra do port

Só entra peça que o Odyssey provar que usa (pelo log). O resto fica de fora.

## Achado de pesquisa (emulação do Odyssey no Android)

- Odyssey roda 60 na maioria das áreas em Snapdragon flagship, com quedas em cutscene; dock mode ajuda em glitch 3D.
- **Drivers Mali podem ser carregados no Eden** (relato com screenshots a 30fps): procurar pacote de drivers Mali compatível com o build e carregar nas configurações de GPU do Eden.
- Sem driver alternativo na Mali-G57, o caminho é o modo Mali-amigável (1 viewport, sem clip planes, resolução baixa) + caches.
- Referência de comparação: S23 roda Odyssey ~45fps no Yuzu Android — régua de flagship vs A15.
