"""Props animados (LoopingProp) y pajaros (RavenEntity) desde TU copia. Salida solo en assets_ps2/.
Uso: python -I tools/build_props.py "<carpeta del juego>" assets_ps2"""
import os, re, struct, sys
import xml.etree.ElementTree as ET
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from build_assets import write_t8, texture_rgba
from build_audio import to_pcm, write_pcm


def main():
    game, out = sys.argv[1], sys.argv[2]
    src = os.path.join(game, 'Content')
    props_dir = os.path.join(src, 'props')
    os.makedirs(os.path.join(out, 'props'), exist_ok=True)

    # PropSettings: fps, celdas de la hoja y duraciones opcionales por frame
    settings = {}
    for ps in ET.parse(os.path.join(props_dir, 'textures', 'prop_settings.xml')).getroot().iter('PropSetting'):
        frames = [float(f.text) for f in ps.iter('float')] if ps.find('frames') is not None else []
        settings[ps.findtext('name')] = (float(ps.findtext('fps') or 10), int(ps.findtext('sheet_cells/X') or 1),
                                         int(ps.findtext('sheet_cells/Y') or 1), ps.findtext('random_offset') == 'true',
                                         frames)
    types, placed = {}, []
    for fn in os.listdir(props_dir):
        m = re.match(r'prop(\d+)\.xml$', fn)
        if not m:
            continue
        screen1 = int(m.group(1))
        for pd in ET.parse(os.path.join(props_dir, fn)).getroot().iter('PropData'):
            t = pd.findtext('type')
            tex = os.path.join(props_dir, 'textures', t + '.xnb')
            if t not in settings or not os.path.exists(tex):
                continue
            if t not in types:
                img = texture_rgba(tex)
                types[t] = len(types)
                write_t8(img, os.path.join(out, 'props', f'p{types[t]}.t8'))
            placed.append((screen1, types[t], int(pd.findtext('Position/X') or 0), int(pd.findtext('Position/Y') or 0),
                           pd.findtext('flipped') == 'true'))
    with open(os.path.join(out, 'props.bin'), 'wb') as f:
        names = sorted(types, key=types.get)
        f.write(b'JKPR' + struct.pack('<HH', len(names), len(placed)))
        for n in names:
            fps, cx, cy, rnd, frames = settings[n]
            frames = frames[:32]
            f.write(struct.pack('<fBBBB', fps, cx, cy, int(rnd), len(frames)) + b''.join(struct.pack('<f', x) for x in frames))
        for screen1, tid, x, y, fl in placed:
            f.write(struct.pack('<HHhhB', screen1, tid, x, y, int(fl)))
    print(f'props: {len(placed)} colocados, {len(types)} tipos')

    # Pajaros: .ravset (RavenSettings) con sus posiciones por pantalla
    rdir = os.path.join(props_dir, 'textures', 'raven')
    birds, tex_ids = [], {}
    for fn in sorted(os.listdir(rdir)):
        if not fn.endswith('.ravset'):
            continue
        r = ET.parse(os.path.join(rdir, fn)).getroot()
        tex = r.findtext('texture')
        # partida normal: el cuervo blanco es de Nueva buenorra+ y el tsuchinoko del modo fantasma
        if tex in ('white_raven', 'tsuchinoko'):
            continue
        if tex not in tex_ids:
            tex_ids[tex] = len(tex_ids)
            write_t8(texture_rgba(os.path.join(rdir, tex + '.xnb')), os.path.join(out, 'props', f'bird{tex_ids[tex]}.t8'))
        sfx = r.findtext('fly_sfx') or 'bird_fly'
        pos = []
        for rp in r.iter('RavenPosition'):
            td = rp.findtext('trigger_distance')
            pos.append((int(rp.findtext('screen') or 1), int(rp.findtext('position/X') or 0),
                        int(rp.findtext('position/Y') or 0), rp.findtext('treasure') == 'true',
                        rp.findtext('look_direction') != 'Left', rp.findtext('fly_direction') != 'Left',
                        float(td) if td else 40.0))
        item = r.findtext('item') or 'NULL'
        birds.append((tex_ids[tex], tex == 'fly', sfx, pos, item))
    sfx_ids = {}
    for _, _, sfx, _, _ in birds:
        if sfx not in sfx_ids:
            p = os.path.join(src, 'audio', 'event_sfx', sfx + '.xnb')
            if os.path.exists(p):
                sfx_ids[sfx] = len(sfx_ids)
                write_pcm(os.path.join(out, 'audio', f'bird{sfx_ids[sfx]}.pcm'), to_pcm(p, 1))
    with open(os.path.join(out, 'birds.bin'), 'wb') as f:
        f.write(b'JKBD' + struct.pack('<H', len(birds)))
        from build_items import ITEMS
        for tid, is_fly, sfx, pos, item in birds:
            iid = ITEMS.index(item) if item in ITEMS else 255
            f.write(struct.pack('<BBbBB', tid, int(is_fly), sfx_ids.get(sfx, -1), len(pos), iid))
            for sc, x, y, tr, look_r, fly_r, td in pos:
                f.write(struct.pack('<HhhBBBf', sc, x, y, int(tr), int(look_r), int(fly_r), td))
    print(f'pajaros: {len(birds)} ({sum(len(b[3]) for b in birds)} posiciones), sonidos: {len(sfx_ids)}')


if __name__ == '__main__':
    main()
