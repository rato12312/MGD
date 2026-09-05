# MGD Port — Super Mario Odyssey

Port dedicado ao Super Mario Odyssey com MGD Engine como núcleo visual.

Filosofia: o jogo simula e mostra as UIs; o MGD faz o 3D. Nada de emulador
completo por baixo — só as peças que o Odyssey usa.

```
Odyssey (estado e simulação)
  ↓
Runtime dedicado (só peças necessárias)
  ↓
MGD (DNA → LOD → shader 2D/LUT → cache → incremental)
  ↓
Tela
```

## Pastas

- `docs/` — estudo do jogo (o que o Odyssey toca, por onde quebra no Mali)
- `src/` — implementações específicas do Odyssey
- `tests/` — provas de cada peça

## Estado atual

Ver `docs/ESTUDO_ODYSSEY.md`.
