# Procedencia del código

Cada pieza del port sale de la descompilación (ILSpy 11.2, `ilspycmd -p`) de `JumpKing.exe`
de la copia legítima de Steam (MonoGame, .NET 4, sin ofuscar). La descompilación vive en
`_ref/decomp/` y **no se sube al repo** (está en `.gitignore`), igual que cualquier asset,
nivel, música o binario del juego.

| Pieza del port | Origen (clase / método en el decomp) | Estado |
|---|---|---|
| Hitbox 18x26 en `src/main.c` | `JumpKing.PlayerValues.PLAYER_WIDTH/HEIGHT` | marcador |

## Mapa de la lógica original (para los próximos pasos)

- Física por frame: `JumpKing.Player.BodyComp.SetupBaseBehaviours` — orden fijo de pasos en
  `JumpKing.BodyCompBehaviours` (viento, X + colisión X, Y + colisión Y, gravedad, bloques).
- Constantes: `JumpKing.PlayerValues` (gravedad y fuerza de salto derivadas por fórmula).
- Carga del salto: `JumpKing.Player.JumpState` (0.6 s, `min(1, t/CHARGE_TIME)`).
- Caída fallida («splat»): `JumpKing.Player.FailState` (0.5 s).
- Cámara por pantallas: `JumpKing.Camera`.
- Niveles: `level.xnb` es una textura; cada color de píxel es un tipo de bloque
  (`JumpKing.Level.LevelManager.LoadBlocksInterval`, `JumpKing.Level.Sampler`).
