# PS2JumpKing

Port nativo de **Jump King** para PlayStation 2 (PS2SDK + gsKit), hecho por **Aroy 24**.
*Native PlayStation 2 port of **Jump King** (PS2SDK + gsKit), by **Aroy 24**.*

> **Necesitás tu propia copia de Jump King (Steam).** Este repo solo contiene el código del port y
> las herramientas de conversión: **ningún** gráfico, sonido, música, texto ni nivel del juego.
> Todo eso se genera en tu PC desde tu instalación.
>
> **You need your own copy of Jump King (Steam).** This repo only contains the port's code and the
> conversion tools: **no** graphics, sounds, music, text or levels from the game. Those are
> generated on your PC from your own installation.

Proyecto de fans, no afiliado a Nexile. Todo el mérito del juego es suyo: si te gusta, comprá el original.
*Fan project, not affiliated with Nexile. All credit for the game goes to them — buy the original.*

---

## Español

### Qué hace falta
- Jump King instalado (Steam).
- Linux o WSL con el toolchain de [ps2dev](https://github.com/ps2dev/ps2dev) (PS2SDK + gsKit) para compilar.
- Python 3 con `Pillow` y `numpy`.
- ILSpy de línea de comandos (`dotnet tool install -g ilspycmd`), para leer los textos y créditos de tu copia.
- PCSX2 (o una PS2 que pueda cargar ELF).

### Pasos
1. Compilar el port:

       make

2. Generar los datos desde tu copia (cambiá la ruta por la de tu instalación; `JKLANG` es el idioma: `es-ES`, `en-US`, ...):

       make assets GAME="/mnt/c/Program Files (x86)/Steam/steamapps/common/Jump King" JKLANG=es-ES

   Crea la carpeta `assets_ps2/` (y `_ref/`, que es privada). **No las compartas.**
3. En PCSX2 activá **Host Filesystem** y arrancá `jumpking_ps2.elf` (o usá `jugar.bat`), con `assets_ps2/` al lado del ELF.

Controles: cruceta o stick izquierdo para andar, ✕ para cargar y soltar el salto, Start para pausa.

---

## English

### Requirements
- Jump King installed (Steam).
- Linux or WSL with the [ps2dev](https://github.com/ps2dev/ps2dev) toolchain (PS2SDK + gsKit) to build.
- Python 3 with `Pillow` and `numpy`.
- Command-line ILSpy (`dotnet tool install -g ilspycmd`), to read the text and credits from your copy.
- PCSX2 (or a PS2 that can load ELFs).

### Steps
1. Build the port:

       make

2. Generate the data from your copy (use your own install path; `JKLANG` is the language: `en-US`, `es-ES`, ...):

       make assets GAME="/mnt/c/Program Files (x86)/Steam/steamapps/common/Jump King" JKLANG=en-US

   This creates `assets_ps2/` (and the private `_ref/`). **Don't share them.**
3. In PCSX2 enable **Host Filesystem** and boot `jumpking_ps2.elf` (or use `jugar.bat`), with `assets_ps2/` next to the ELF.

Controls: d-pad or left stick to walk, ✕ to charge and release a jump, Start to pause.

---

## Estado / Status

Física completa, 169 pantallas, NPC y diálogos, pájaros, objetos y prendas, música, ambiente y clima,
menús, guardado en memory card, intro y final normal con créditos. Faltan los modos New Babe+ y
Ghost of the Babe. Probado en PCSX2; falta probar en hardware real.

*Full physics, all 169 screens, NPCs and dialogue, birds, items and outfits, music, ambience and
weather, menus, memory card saves, intro and the normal ending with credits. Missing: New Babe+ and
Ghost of the Babe. Tested in PCSX2, not yet on real hardware.*

De dónde sale cada pieza del port: [`docs/procedencia.md`](docs/procedencia.md).
