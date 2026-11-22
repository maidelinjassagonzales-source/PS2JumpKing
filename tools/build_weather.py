"""Clima (WeatherManager/WeatherInstance + MaskShader) desde TU copia. Salida solo en assets_ps2/.
Sin mascara: frames de 120x120 que el PS2 repite en mosaico. Con mascara (screens/masks): se precalcula
el mosaico a 480x360 con el alfa de la mascara, porque el PS2 no tiene shaders.
Uso: python -I tools/build_weather.py "<carpeta del juego>" assets_ps2"""
import os, struct, sys
import xml.etree.ElementTree as ET
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from build_assets import write_t8, texture_rgba


def main():
    game, out = sys.argv[1], sys.argv[2]
    src = os.path.join(game, 'Content')
    pdir = os.path.join(src, 'particles')
    mdir = os.path.join(src, 'screens', 'masks')
    os.makedirs(os.path.join(out, 'weather'), exist_ok=True)
    weathers = []
    for w in ET.parse(os.path.join(pdir, 'weather.xml')).getroot().iter('Weather'):
        name = w.findtext('name')
        wdir = os.path.join(pdir, name)
        if not os.path.isdir(wdir):
            continue
        frames = [texture_rgba(os.path.join(wdir, f)) for f in sorted(os.listdir(wdir)) if f.endswith('.xnb')][:8]
        wid = len(weathers)
        for i, fr in enumerate(frames):
            write_t8(fr, os.path.join(out, 'weather', f'w{wid}_{i}.t8'))
        screens = [int(s.text) for s in w.iter('int')]
        masked = []
        for sc in screens:
            mp = os.path.join(mdir, f'{name}mask{sc}.xnb')
            if not os.path.exists(mp):
                continue
            mask = texture_rgba(mp)[..., 3].astype(np.uint16)
            for i, fr in enumerate(frames):
                h, wdt = fr.shape[:2]
                tile = np.tile(fr, (360 // h + 1, 480 // wdt + 1, 1))[:360, :480].copy()
                tile[..., 3] = (tile[..., 3].astype(np.uint16) * mask // 255).astype(np.uint8)
                write_t8(tile, os.path.join(out, 'weather', f'm{wid}_{sc}_{i}.t8'))
            masked.append(sc)
        weathers.append((int(w.findtext('fps') or 12), w.findtext('foreground') == 'true', len(frames),
                         frames[0].shape[1] if frames else 0, frames[0].shape[0] if frames else 0, screens, masked))
    with open(os.path.join(out, 'weather.bin'), 'wb') as f:
        f.write(b'JKWE' + struct.pack('<B', len(weathers)))
        for fps, fg, nf, tw, th, screens, masked in weathers:
            f.write(struct.pack('<BBBHHBB', fps, int(fg), nf, tw, th, len(screens), len(masked)))
            f.write(b''.join(struct.pack('<H', s) for s in screens) + b''.join(struct.pack('<H', s) for s in masked))
    print(f'clima: {len(weathers)} efectos, {sum(len(w[5]) for w in weathers)} pantallas, '
          f'{sum(len(w[6]) for w in weathers)} con mascara')


if __name__ == '__main__':
    main()
