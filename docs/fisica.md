# Física del rey (resumen con valores medidos)

Valores calculados en `jk_values_init` con las mismas fórmulas que el original y comprobados
por `make test` (PC):

| Magnitud | Valor |
|---|---|
| Gravedad | 153/595 ≈ 0.2571429 px/frame² |
| Salto completo | −34 × gravedad ≈ −8.742858 px/frame |
| Subida del salto completo | 153 px |
| Tiempo en el aire (salto completo, suelo a suelo) | 69 frames |
| Carga completa | 35 frames con botón (0.6 s; el último incremento lo dispara) |
| Andar | 1.5 px/frame |
| Velocidad horizontal en el aire | 3.5 px/frame |
| Rebote contra pared | vx × −0.5 |
| Golpe en el techo | vx × 0.5, vy = 0 |
| Caída máxima | 10 px/frame; aterrizar a exactamente esa velocidad = splat |
| Splat | 30 ticks (0.5 s) en el suelo antes de aceptar entrada |

Paso físico por frame (`src/jk/body.c`): guardar velocidad → si empieza solapado, mover y salir
→ X += vx → limitar a pantalla → resolver choque X → pegado al suelo (+1 px) → Y += vy →
resolver choque Y → gravedad con tope.

Luego, en `src/jk/king.c`: latch del botón de salto → decisiones (splat > carga/salto > andar
en el suelo; en el aire solo sigue una carga ya empezada) → cámara.
