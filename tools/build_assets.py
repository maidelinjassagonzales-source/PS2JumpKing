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
    """Matriz [pantalla][y][x] de colores RGBA, con el reparto por columnas del original.
    El vacio es negro transparente: los codigos se comparan con alfa 255, como Color en XNA."""
    img = texture_rgba(os.path.join(src, 'level.xnb'))
    rows = img.shape[0] // TILE_H                  # pantallas por columna
    cols = img.shape[1] // TILE_W
    screens = []
    for s in range(rows * cols):
        cx, cy = s // rows, s % rows
        screens.append(img[cy * TILE_H:(cy + 1) * TILE_H, cx * TILE_W:(cx + 1) * TILE_W])
    return screens


SOLID_FOR_EDGE = {(0, 0, 0), (255, 0, 0), (0, 255, 255), (255, 255, 0), (255, 106, 0)}   # IsSolidBlock
FAKE = (128, 128, 128)


def write_level(src, out):
    """level.bin v2 ('JKL2'): por pantalla una cabecera (viento, teletransporte) y 60x45 celdas.
    Celda = tipo | (pendiente << 4); pendiente 0..3 = TopLeft, TopRight, BottomLeft, BottomRight, 4 = ninguna."""
    screens = level_cells(src)
    n = len(screens)

    def rgba(s, x, y):
        # LevelTexture.GetCoord: y fuera de 0..44 pasa a la pantalla de arriba/abajo
        while y < 0:
            y += TILE_H; s += 1
        while y >= TILE_H:
            y -= TILE_H; s -= 1
        if s < 0 or s >= n:
            return None
        return tuple(int(v) for v in screens[s][y, x])

    def is_edge(s, x, y):                                  # LevelManager.IsBlockEdge
        if x < 0 or x >= TILE_W:
            return True
        c = rgba(s, x, y)
        if c is None or c[3] != 255:
            return False
        return c[:3] in SOLID_FOR_EDGE or c[:3] == FAKE

    def slope_type(s, x, y):                               # BaseBlockFactory.GetSlopeType
        if is_edge(s, x + 1, y) and is_edge(s, x, y + 1): return 0
        if is_edge(s, x - 1, y) and is_edge(s, x, y + 1): return 1
        if is_edge(s, x + 1, y) and is_edge(s, x, y - 1): return 2
        if is_edge(s, x - 1, y) and is_edge(s, x, y - 1): return 3
        return 4

    with open(os.path.join(out, 'level.bin'), 'wb') as f:
        f.write(b'JKL2' + struct.pack('<I', n))
        for si, cells in enumerate(screens):
            grid = np.zeros((TILE_H, TILE_W), np.uint8)
            opaque = cells[:, :, 3] == 255
            for code, kind in BLOCK_CODES.items():
                grid[np.all(cells[:, :, :3] == code, axis=2) & opaque] = kind
            wind_on, wind_int, wind_dir, tele = 0, 0.0, -1, [-1, -1]
            for y in range(TILE_H):
                for x in range(TILE_W):
                    r, g, b, a = (int(v) for v in cells[y, x])
                    if a != 255:
                        continue
                    if grid[y, x] == 2:
                        grid[y, x] |= slope_type(si, x, y) << 4
                    if grid[y, x]:
                        continue
                    if (r, g, b) == (0, 255, 0):           # BLOCKCODE_WIND
                        wind_on = 1
                    if g == 0 and b == 255:                # enlace de teletransporte (indice 1-based en R)
                        tele[1 if x >= 30 else 0] = r
                    if g == 255 and b == 0:                # viento con intensidad/direccion
                        if 191 < r < 208: wind_int = float(r - 191)
                        if 207 < r < 224: wind_int, wind_dir = float(r - 207), 1
                        if 223 < r < 240: wind_int, wind_dir = float(r - 223), 0
                        wind_on = 1
            # cabecera de 16 bytes: viento on, direccion (-1/0/1), -, intensidad, teletransporte x2, -
            f.write(struct.pack('<Bbhfhhi', wind_on, wind_dir, 0, wind_int, tele[0], tele[1], 0))
            f.write(grid.tobytes())
    return n


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
