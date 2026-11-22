"""Convierte los datos de TU copia de Jump King al formato del port de PS2.
Uso: python -I tools/build_assets.py "<carpeta del juego>" assets_ps2
La salida queda solo en tu disco (assets_ps2/ esta en .gitignore); el repo no lleva datos del juego."""
import os, struct, sys
import numpy as np
from PIL import Image
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import xnb

TILE_W, TILE_H = 60, 45            # celdas de 8 px por pantalla (LevelTexture.TILE)

# Codigos de color del mapa (BaseBlockFactory / LevelManager). Valor = tipo de bloque del port.
BLOCK_CODES = {
    (0, 0, 0): 1,          # solido
    (255, 0, 0): 2,        # pendiente
    (0, 255, 255): 3,      # hielo
    (255, 255, 0): 4,      # nieve
    (255, 106, 0): 5,      # arena
    (255, 255, 255): 6,    # sin viento
    (0, 170, 170): 7,      # agua
    (182, 255, 0): 8,      # quark
}


def texture_rgba(path):
    kind, t = xnb.read(path)
    assert kind == 'texture', path
    return np.frombuffer(t['rgba'], np.uint8).reshape(t['h'], t['w'], 4)


def level_cells(src):
    """Matriz [pantalla][y][x] de colores RGB, con el reparto por columnas del original."""
    img = texture_rgba(os.path.join(src, 'level.xnb'))
    rows = img.shape[0] // TILE_H                  # pantallas por columna
    cols = img.shape[1] // TILE_W
    screens = []
    for s in range(rows * cols):
        cx, cy = s // rows, s % rows
        screens.append(img[cy * TILE_H:(cy + 1) * TILE_H, cx * TILE_W:(cx + 1) * TILE_W, :3])
    return screens


def write_level(src, out):
    screens = level_cells(src)
    with open(os.path.join(out, 'level.bin'), 'wb') as f:
        f.write(b'JKLV' + struct.pack('<I', len(screens)))
        for cells in screens:
            grid = np.zeros((TILE_H, TILE_W), np.uint8)
            for code, kind in BLOCK_CODES.items():
                grid[np.all(cells == code, axis=2)] = kind
            f.write(grid.tobytes())
    return len(screens)


def swizzle_clut(pal):
    """Orden CSM1 del GS para paletas de 256: en cada bloque de 32, intercambiar 8..15 con 16..23."""
    out = pal.copy()
    for b in range(0, 256, 32):
        out[b + 8:b + 16], out[b + 16:b + 24] = pal[b + 16:b + 24], pal[b + 8:b + 16]
    return out


def write_t8(rgba, path):
    """Textura indexada de 8 bits: 'JKT8', u16 w, u16 h, 256 colores RGBA (alfa PS2 0..128), indices."""
    h, w = rgba.shape[:2]
    img = Image.fromarray(np.ascontiguousarray(rgba), 'RGBA')
    q = img.quantize(colors=255, method=Image.Quantize.FASTOCTREE, dither=Image.Dither.NONE)
    idx = np.array(q, np.uint8)
    pal = np.zeros((256, 4), np.uint8)
    p = np.array(q.getpalette('RGBA') or [], np.uint8).reshape(-1, 4)[:256]
    pal[:len(p)] = p
    pal[:, 3] = (pal[:, 3].astype(np.uint16) + 1) // 2          # 255 -> 128
    with open(path, 'wb') as f:
        f.write(b'JKT8' + struct.pack('<HH', w, h))
        f.write(swizzle_clut(pal).tobytes())
        f.write(idx.tobytes())


def write_screens(src, out, count):
    layers = (('background', 'bg', 'b'), ('midground', '', 'm'), ('foreground', 'fg', 'f'))
    os.makedirs(os.path.join(out, 'screens'), exist_ok=True)
    made = 0
    for s in range(count):
        for folder, prefix, tag in layers:
            p = os.path.join(src, 'screens', folder, f'{prefix}{s + 1}.xnb')
            if os.path.exists(p):
                write_t8(texture_rgba(p), os.path.join(out, 'screens', f'{tag}{s}.t8'))
                made += 1
    return made


def write_king(src, out):
    """Hoja del rey normal: celdas de 48x48 en una rejilla de 4x4 desde (224,0) (KingSpriteLayers.Regular)."""
    img = texture_rgba(os.path.join(src, 'king', 'base.xnb'))
    write_t8(img[0:192, 224:224 + 192], os.path.join(out, 'king.t8'))


def main():
    game, out = sys.argv[1], sys.argv[2]
    src = os.path.join(game, 'Content')
    os.makedirs(out, exist_ok=True)
    n = write_level(src, out)
    print(f'nivel: {n} pantallas')
    print(f'capas de pantalla: {write_screens(src, out, n)}')
    write_king(src, out)
    print('rey: king.t8')


if __name__ == '__main__':
    main()
