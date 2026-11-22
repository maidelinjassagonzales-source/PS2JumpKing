"""Fondos con scroll (ScrollingBackground) desde TU copia: capas por pantalla y sus texturas.
Uso: python -I tools/build_scroll.py "<carpeta del juego>" assets_ps2   (salida fuera del repo)"""
import os, re, struct, sys
import xml.etree.ElementTree as ET
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from build_assets import write_t8, texture_rgba


def main():
    game, out = sys.argv[1], sys.argv[2]
    base = os.path.join(game, 'Content', 'screens', 'scrolling')
    os.makedirs(os.path.join(out, 'scroll'), exist_ok=True)
    tex, screens, big = {}, [], 0
    for fn in os.listdir(base):
        m = re.match(r'scroll(\d+)\.xml$', fn)
        if not m:
            continue
        r = ET.parse(os.path.join(base, fn)).getroot()
        layers = []
        for ly in r.iter('Layer'):
            t = ly.findtext('texture')
            p = os.path.join(base, 'textures', t + '.xnb')
            if not os.path.exists(p):
                continue
            if t not in tex:
                img = texture_rgba(p)
                tex[t] = len(tex)
                big = max(big, img.shape[0] * img.shape[1])
                write_t8(img, os.path.join(out, 'scroll', f's{tex[t]}.t8'))
            mode = {'Background': 1, 'Foreground': 2}.get(ly.findtext('layer_mode'), 0)
            an = ly.find('animation')
            fps = float(an.findtext('fps') or 10) if an is not None else 0.0
            cx = int(an.findtext('sheet_cells/X') or 1) if an is not None else 1
            cy = int(an.findtext('sheet_cells/Y') or 1) if an is not None else 1
            frames = [float(f.text) for f in an.iter('float')][:16] if an is not None else []
            layers.append((tex[t], int(ly.findtext('position') or 0), float(ly.findtext('scroll_multiplier') or 0),
                           mode, fps, cx, cy, frames))
        screens.append((int(m.group(1)), float(r.findtext('scroll_speed') or 0), layers))
    with open(os.path.join(out, 'scroll.bin'), 'wb') as f:
        f.write(b'JKSC' + struct.pack('<H', len(screens)))
        for sc, speed, layers in screens:
            f.write(struct.pack('<HfB', sc, speed, len(layers)))
            for tid, pos, mul, mode, fps, cx, cy, frames in layers:
                f.write(struct.pack('<HhfBfBBB', tid, pos, mul, mode, fps, cx, cy, len(frames)))
                f.write(b''.join(struct.pack('<f', x) for x in frames))
    print(f'fondos con scroll: {len(screens)} pantallas, {sum(len(s[2]) for s in screens)} capas, '
          f'{len(tex)} texturas (la mayor {big} px)')


if __name__ == '__main__':
    main()
