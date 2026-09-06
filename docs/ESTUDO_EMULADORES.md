# Estudo de emuladores (Eden + outros) — o que roubar

## Eden (nossa base ferro-velho)
- CPU: Dynarmic JIT (x86) + **NCE no Android ARM64** (roda o guest direto,
  sem traduzir). No A15 (ARM64), NCE é o caminho — não interpretador.
- Com NCE, o gargalo vira a GPU. Na Mali fraca: resolução baixa + async.
- Docs oficiais: Mali tem performance ruim; Android 16 melhora muito;
  cooler recomendado; 8 GB RAM é o mínimo em todo lugar.
- Conclusão: nosso preset (Mali barata) ataca exatamente o gargalo certo.

## Ryujinx (accuracy-first, .NET, sem Android)
- LLE, preciso, pesado: 8 GB mínimo, 16-32 GB recomendado, VRAM alta.
- Ótimo no desktop, péssimo em aparelho fraco. Lição: NÃO seguir esse
  caminho no A15. Accuracy é luxo de PC.
- Roubar: modularidade do código e LayeredFS (mods sem tocar no jogo).

## Yuzu-linha (speed-first, hacks agressivos)
- HLE + hacks + shader cache agressivo + multicore cedo = roda em
  hardware fraco. É por isso que a base Eden (fork Yuzu) é a certa
  para o A15, e o modo Edge (velocidade antes de precisão) é Skyline
  com outro nome.

## Skyline (mobile-first, morto)
- Nascido pra ARM, renderer pra GPU tiled, velocidade primeiro.
- Código puxado do ar: só ideias (Edge mode, batching, sem churn).

## Dynarmic (0BSD — pode vendorar!)
- Licença permissiva: dá pra trazer o JIT pra dentro do emulador-mgd
  no futuro, sem problema de GPL.
- Roteiro da CPU: interpretador (hoje) → blocos com cache → JIT/NCE.

## Veredito para o A15 4 GB
- Todo emulador pede 8 GB mínimo. 4 GB está abaixo do piso de TODOS.
- Odyssey bootar no Edge e morrer na fase = OOM, confirmado pela cena.
- Estratégia: Edge + preset + shader cache pronto + jogar leve hoje.
