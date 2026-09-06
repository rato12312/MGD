# Emulador MGD (só nosso, do início)

Motor próprio reaproveitando tudo que o MGD já tem. Nada de reescrever
o que existe: o MGD vira o corpo, o novo é só o que falta.

## O que já mora aqui (via `../core`)
- `scanner` — lê o jogo uma vez, cache por IDs
- `mental_map` + `cache` — o jogo nos arquivos, em chucks
- `query/dna` — polígono vira DNA compacto, pixel reaproveitado
- `camera` — consciência, guiada por colisão
- `painter` — olhos, completa o frame
- `bridge` — handoff (captura alimenta a ponte)

## O que nasce aqui
- `cpu/` — MiniArm crescendo (instrução por instrução, cada uma com teste)
- `ram/` — RAM do guest + mapa mental no disco
- `handoff/` — implementação da captura (hoje só contrato no core)

## Regras
1. Peça nova só entra com teste verde.
2. Nunca copiar asset: só IDs + posição.
3. Vale o que está medido, não o que foi projetado.
4. Licença: só código nosso ou MIT-compatível aqui dentro.
   Peça GPL do Eden fica em `ports/` como referência, NÃO aqui.
