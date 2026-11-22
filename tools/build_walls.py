"""Paredes ocultas (RaymanWallEntity) desde TU copia: texturas y zonas de contacto por pantalla.
Uso: python -I tools/build_walls.py "<carpeta del juego>" assets_ps2   (salida fuera del repo)"""
import os, re, struct, sys
import xml.etree.ElementTree as ET
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from build_assets import write_t8, texture_rgba

H = 360


def rect(e):
    return [int(e.findtext(k) or 0) for k in ('X', 'Y', 'Width', 'Height')]


def main():
    game, out = sys.argv[1], sys.argv[2]
    base = os.path.join(game, 'Content', 'props', 'hidden_walls')
    os.makedirs(os.path.join(out, 'walls'), exist_ok=True)
    tex_ids, walls = {}, []
    for fn in sorted(os.listdir(base)):
        m = re.match(r'hidden_wall(\d+)\.xml$', fn)
        if not m:
            continue
        screen1 = int(m.group(1))                       # pantalla 1-based (nombre del archivo)
        for d in ET.parse(os.path.join(base, fn)).getroot().iter('RaymanData'):
            name = d.findtext('texture_name')
            path = os.path.join(base, 'textures', name + '.xnb')
            if not os.path.exists(path):
                continue
            if name not in tex_ids:
                img = texture_rgba(path)
                tex_ids[name] = (len(tex_ids), img.shape[1], img.shape[0])
                write_t8(img, os.path.join(out, 'walls', f'w{tex_ids[name][0]}.t8'))
            tid, tw, th = tex_ids[name]
            # RaymanData.CorrectPosition
            dy = -(screen1 - 1) * H
            px, py = int(d.findtext('Position/X') or 0), int(d.findtext('Position/Y') or 0) + dy
            screen_coords = d.findtext('use_screen_coords') == 'true'
            boxes = [rect(r) for r in d.iter('Rectangle')] or [[0, 0, tw, th]]
            fixed = []
            for x, y, w, h in boxes:
                if screen_coords and d.find('hitboxes') is not None:
                    fixed.append((x, y + dy, w, h))
                else:
                    fixed.append((x + px, y + py, w, h))
            walls.append((screen1, tid, px, py, fixed))
    with open(os.path.join(out, 'walls.bin'), 'wb') as f:
        f.write(b'JKHW' + struct.pack('<H', len(walls)))
        for screen1, tid, px, py, boxes in walls:
            f.write(struct.pack('<HHiiB', screen1, tid, px, py, len(boxes)))
            for b in boxes:
                f.write(struct.pack('<iihh', *b))
    print(f'paredes ocultas: {len(walls)} en {len(set(w[0] for w in walls))} pantallas, {len(tex_ids)} texturas')


if __name__ == '__main__':
    main()
