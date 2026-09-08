# Cadeia Mario -> Loader (como o jogo entra no emulador)

```
NSP (jogo)
 └─ PFS0  (Pfs0Reader: open/list/read) .............. tools/mgd_nsp.cpp usa
     └─ *.nca
         ├─ sonda (NcaProbe: magic NCA3 + tipo) ...... decide o que é
         ├─ header XTS (Aes::xtsDecrypt + chave do USUÁRIO)
         ├─ seções (NcaSections: FsEntry 0x240 + FsHeader)
         │   └─ dados CTR (Aes::cryptCtrFull + chave do USUÁRIO)
         │       ├─ ExeFS = PFS0 (Pfs0Reader + readMain/readNpdm)
         │       │   ├─ main (NSO: texto do jogo)
         │       │   └─ main.npdm (metadados: o que o jogo pede)
         │       └─ RomFS (RomFsReader + índice)
         │           └─ dados (modelos, fases, shaders)
         └─ NSO: parse (NsoLoader) + LZ4 + relocs (Reloc.h)
             └─ mapa na RAM (loadNsoInto) -> entry
                 └─ Emulator::bootNso -> heap (SVC#1) -> threads
                     -> serviços (sm/nv/vi/aud/fs/hid...)
                     -> frames (mundo pinta, Mali rascunha, painter completa)
```

## O que é chave do usuário (nunca embarcada)
- Header key (XTS dos 0xC00 primeiros bytes do NCA).
- Titlekeys / section keys (CTR das seções).
- Sem elas: sonda identifica, nada descriptografa. `KeyManager` recebe e usa.

## O que o MGD faz no caminho (onde coube)
- Índice RomFS (consulta O(1) em vez de varrer).
- Shader cache pré-compilado (engasgo zero) — `core/cache`.
- Teto de RAM + streaming por região (não segura o mundo).
- LOD pela câmera + painter com upscale (Mali barata).
- Chaves de camada (mede o ganho de cada uma).

## O que o MGD não faz (não coube, esquece)
- Encolher a RAM do guest (física).
- Trocar driver sem root.
- Desenhar sem pixel (tela preta).
