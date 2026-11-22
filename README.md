# PS2JumpKing

Port para PlayStation 2 hecho por **Aroy 24**.

Port homebrew de Jump King a PlayStation 2 (PS2SDK + gsKit), basado en la ingeniería inversa
del ejecutable original (no en lógica inventada). Ver `docs/procedencia.md`.

**Este repo no contiene assets, niveles, música ni binarios del juego.** Hace falta tu propia
copia de Jump King para generar los datos.

## Compilar

En WSL/Linux con el toolchain de ps2dev y gsKit (`PS2SDK`, `GSKIT` definidos):

    make

Probar en PCSX2: `jugar.bat`.
