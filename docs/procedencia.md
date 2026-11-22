# Procedencia del código

Cada pieza del port sale de la descompilación (ILSpy 11.2, `ilspycmd -p`) de `JumpKing.exe`
de la copia legítima de Steam (MonoGame, .NET 4, sin ofuscar). La descompilación vive en
`_ref/decomp/` y **no se sube al repo** (está en `.gitignore`), igual que cualquier asset,
nivel, música o binario del juego.

| Pieza del port | Origen (clase / método en el decomp) | Estado |
|---|---|---|
| `src/jk/values.c` (gravedad, salto, velocidades, rebote, caída máx., tiempos) | `JumpKing.PlayerValues`, `ErikMaths.ErikMath.SigmaSum1/SigmaSumf` | verificado en PC |
| `src/jk/body.c` `jk_body_step` (orden del paso físico) | `JumpKing.Player.BodyComp.SetupBaseBehaviours` + `UpdateInternal` | verificado en PC |
| salida temprana si empieza solapado | `BodyCompBehaviours.CollisionCheckEarlyExitBehaviour` | portado |
| límite de X en pantalla | `BodyCompBehaviours.CapPositionBehaviour` | portado |
| choque X y rebote | `BodyCompBehaviours.ResolveXCollisionBehaviour` (sin pendientes) | verificado en PC |
| pegado al suelo (+1 px) | `BlockBehaviours.SandBlockBehaviour.ModifyYVelocity` (rama sin arena) | verificado en PC |
| choque Y, aterrizaje, techo | `BodyCompBehaviours.ResolveYCollisionBehaviour` (sin pendientes/hielo/nieve/quark) | verificado en PC |
| gravedad y tope de caída | `BodyCompBehaviours.ApplyGravityBehaviour` | verificado en PC |
| `src/jk/king.c` orden de frame (física → entrada → decisiones → cámara) | `PlayerEntity` ctor (`AddComponents`), `EntityComponent.Entity.UpdateComponents` | portado |
| decisiones del rey (máquina de estados) | `PlayerEntity.MakeBT` + semántica de `BehaviorTree.IBTnode.Run` (nuevo/reanudar), `BTselector`, `BTsequencor`, `BTsimultaneous`, `StaticNode` | verificado en PC |
| carga y salto, buffer de 4 frames de dirección | `JumpKing.Player.JumpState`, `Buffer<T>` | verificado en PC |
| latch del botón de salto | `JumpKing.Player.InputComponent.Update/TryConsumeJump` | portado |
| andar | `JumpKing.Player.Walk` (sin hielo/nieve/arena) | verificado en PC |
| splat (caída a velocidad máxima, 0.5 s) | `JumpKing.Player.FailState`, `JumpKing.Timer` | verificado en PC |
| margen de 1/30 s cargando en el aire | `PlayerEntity.JUMP_LENIENCY_TIME`, `JumpKing.Util.PauseNode` | portado |
| `src/jk/camera.c` cambio de pantalla | `JumpKing.Camera.UpdateCameraWithVelocity`, `Player.CameraFollowComp` | verificado en PC |
| `src/jk/level.c` colisión pantalla actual ±1 | `Level.LevelManager.CheckCollisionInternal`, `LevelScreen.TryCollision`, `BoxBlock.Intersects` | portado (solo cajas) |
| truncado de la caja (float→int) | `MonoGame Vector2.ToPoint` (confirmado descompilando MonoGame.Framework.dll) | portado |

El nivel de `src/jk/level.c` es **de prueba e inventado**, no sale del juego. Los gráficos son rectángulos.

## Pendiente

Pendientes, hielo, nieve, arena, agua, viento, quark, teletransporte entre pantallas, sonidos,
lectura de `level.xnb` desde tu copia, sprites.

## Riesgo conocido: FPU del EE

El FPU del EE redondea hacia cero y no tiene denormales; el original corre en SSE (IEEE, redondeo
al más cercano). Las pruebas (`make test`) corren en PC. Falta confirmar en PCSX2/hardware que
los frames de carga (35) y la subida (153 px) dan igual.

## Mapa de la lógica original (para los próximos pasos)

- Física por frame: `JumpKing.Player.BodyComp.SetupBaseBehaviours` — orden fijo de pasos en
  `JumpKing.BodyCompBehaviours` (viento, X + colisión X, Y + colisión Y, gravedad, bloques).
- Constantes: `JumpKing.PlayerValues` (gravedad y fuerza de salto derivadas por fórmula).
- Carga del salto: `JumpKing.Player.JumpState` (0.6 s, `min(1, t/CHARGE_TIME)`).
- Caída fallida («splat»): `JumpKing.Player.FailState` (0.5 s).
- Cámara por pantallas: `JumpKing.Camera`.
- Niveles: `level.xnb` es una textura; cada color de píxel es un tipo de bloque
  (`JumpKing.Level.LevelManager.LoadBlocksInterval`, `JumpKing.Level.Sampler`).
