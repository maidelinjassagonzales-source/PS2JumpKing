"""Convierte el audio de TU copia de Jump King para el port (22050 Hz, 16 bits).
Uso: python -I tools/build_audio.py "<carpeta del juego>" assets_ps2   (salida fuera del repo)
- audio/amb/NNN.pcm : pistas de ambiente/musica, estereo
- audio/sfx_*.pcm   : efectos del rey, mono
- audio/ambience.bin: que pistas suenan en cada pantalla (audio/background/data/values.xml)"""
import os, struct, sys
import xml.etree.ElementTree as ET
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import xnb

RATE = 22050


def to_pcm(path, channels):
    kind, s = xnb.read(path)
    assert kind == 'sound', path
    if s['tag'] == 1 and s['bits'] == 16:
        a = np.frombuffer(s['data'], '<i2').reshape(-1, s['channels']).astype(np.int32)
    elif s['tag'] == 3 and s['bits'] == 32:          # float IEEE
        a = (np.frombuffer(s['data'], '<f4').reshape(-1, s['channels']) * 32767.0).astype(np.int32)
    else:
        raise ValueError(f"{path}: formato de audio {s['tag']}/{s['bits']} no soportado")
    if s['rate'] == 44100:
        a = a[: len(a) // 2 * 2].reshape(-1, 2, a.shape[1]).mean(axis=1)
    elif s['rate'] != RATE:
        idx = np.arange(0, len(a), s['rate'] / RATE).astype(int)
        a = a[idx]
    if channels == 1 and a.shape[1] == 2:
        a = a.mean(axis=1, keepdims=True)
    if channels == 2 and a.shape[1] == 1:
        a = np.repeat(a, 2, axis=1)
    return np.clip(a, -32768, 32767).astype('<i2')


def write_pcm(path, a):
    """'JKPC' u32 rate, u16 canales, u16 0, u32 frames, datos intercalados."""
    with open(path, 'wb') as f:
        f.write(b'JKPC' + struct.pack('<IHHI', RATE, a.shape[1], 0, len(a)))
        f.write(a.tobytes())


def main():
    game, out = sys.argv[1], sys.argv[2]
    src = os.path.join(game, 'Content', 'audio')
    os.makedirs(os.path.join(out, 'audio', 'amb'), exist_ok=True)

    bg = os.path.join(src, 'background')
    names = sorted(f[:-4] for f in os.listdir(bg) if f.endswith('.xnb'))
    ids = {n: i for i, n in enumerate(names)}
    for i, n in enumerate(names):
        write_pcm(os.path.join(out, 'audio', 'amb', f'{i:03d}.pcm'), to_pcm(os.path.join(bg, n + '.xnb'), 2))
    print(f'pistas de ambiente: {len(names)}')

    root = ET.parse(os.path.join(bg, 'data', 'values.xml')).getroot()
    info = {}
    for ai in root.iter('AmbienceInfo'):
        info[ai.findtext('name')] = (ai.findtext('restart') == 'true',
                                     float(ai.findtext('fade_in_length') or 0),
                                     float(ai.findtext('fade_out_length') or 0))
    screens = []
    for sec in root.find('sections').iter('AmbienceSave'):
        tracks = [(ids[a.findtext('name')], float(a.findtext('volume') or 1))
                  for a in sec.iter('Ambience') if a.findtext('name') in ids]
        screens += [tracks] * int(sec.findtext('screens') or 1)

    with open(os.path.join(out, 'audio', 'ambience.bin'), 'wb') as f:
        f.write(b'JKAM' + struct.pack('<HH', len(names), len(screens)))
        for n in names:                                   # por pista: restart, fade in, fade out
            r, fi, fo = info.get(n, (False, 0.0, 0.0))
            f.write(struct.pack('<Iff', int(r), fi, fo))
        for t in screens:                                 # por pantalla: hasta 4 (pista, volumen)
            t = t[:4]
            f.write(struct.pack('<B', len(t)))
            for tid, vol in t + [(0, 0.0)] * (4 - len(t)):
                f.write(struct.pack('<Hf', tid, vol))
    print(f'pantallas con ambiente: {len(screens)}')

    for name in ('king_jump', 'king_land', 'king_bump', 'king_splat'):
        write_pcm(os.path.join(out, 'audio', f'sfx_{name[5:]}.pcm'),
                  to_pcm(os.path.join(src, 'jump_king', name + '.xnb'), 1))
    print('efectos: jump, land, bump, splat')


if __name__ == '__main__':
    main()
