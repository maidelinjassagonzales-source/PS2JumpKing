"""NPC (viejo, mercader, etc.) y fuente de dialogo desde TU copia de Jump King.
Uso: python -I tools/build_npcs.py "<carpeta del juego>" assets_ps2   (salida fuera del repo)
Datos: props/textures/old_man/{lines,merchant}/*.xml (OldManSettings) y font/sf_pixolde.xnb."""
import os, struct, sys
import xml.etree.ElementTree as ET
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import xnb
from build_assets import write_t8, texture_rgba


def rgb565(c):
    return np.array([((c >> 11) & 31) * 255 // 31, ((c >> 5) & 63) * 255 // 63, (c & 31) * 255 // 31], np.int32)


def dxt3_to_rgba(data, w, h):
    out = np.zeros((h, w, 4), np.uint8)
    pos = 0
    for by in range(0, h, 4):
        for bx in range(0, w, 4):
            alpha = int.from_bytes(data[pos:pos + 8], 'little'); pos += 8
            c0, c1, bits = struct.unpack_from('<HHI', data, pos); pos += 8
            a, b = rgb565(c0), rgb565(c1)
            pal = [a, b, (2 * a + b) // 3, (a + 2 * b) // 3]
            for i in range(16):
                px, py = bx + i % 4, by + i // 4
                out[py, px, :3] = pal[(bits >> (2 * i)) & 3]
                out[py, px, 3] = ((alpha >> (4 * i)) & 15) * 17
    return out


LANG = {}          # clave -> texto, de los recursos de idioma de la instalacion


def load_language(resx_path):
    """Lee el .resx que ILSpy exporta de <idioma>/LanguageJK.resources.dll (tools/_bin, _ref/)."""
    if resx_path and os.path.exists(resx_path):
        for d in ET.parse(resx_path).getroot().iter('data'):
            LANG[d.get('name')] = d.findtext('value') or ''


def ascii_text(s):
    """Resuelve la clave como TargetLine (quita saltos y busca en el idioma) y aplica FormatString."""
    key = (s or '').replace('\n', '')
    t = LANG.get(key, key)
    for a, b in (('’', "'"), ('‘', "'"), (' .', '.'), ('“', '"'), ('”', '"')):
        t = t.replace(a, b)
    return t.encode('latin-1', 'replace').decode('latin-1')


def write_font(src, out, font='sf_pixolde', name='font'):
    k, f = xnb.read(os.path.join(src, 'font', font + '.xnb'))
    t = f['texture']
    rgba = dxt3_to_rgba(t['rgba'], t['w'], t['h']) if t['fmt'] == 5 else \
        np.frombuffer(t['rgba'], np.uint8).reshape(t['h'], t['w'], 4)
    write_t8(rgba, os.path.join(out, name + '.t8'))
    table = {}
    for ch, g, c, kn in zip(f['chars'], f['glyphs'], f['crops'], f['kern']):
        if len(ch) == 1 and 32 <= ord(ch) < 256:
            table[ord(ch)] = (g, c, kn)
    with open(os.path.join(out, name + '.bin'), 'wb') as fo:
        fo.write(b'JKFN' + struct.pack('<hf', f['line'], f['spacing']))
        for code in range(32, 256):                       # 224 entradas fijas (Latin-1)
            g, c, kn = table.get(code, ((0, 0, 0, 0), (0, 0, 0, 0), (0.0, 0.0, 0.0)))
            fo.write(struct.pack('<4h4h3f', *g, *c, *kn))


def frames(node):
    res = []
    if node is not None:
        for fr in node.iter('AnimationFrame'):
            res.append((int(fr.findtext('sprite_index') or 0), float(fr.findtext('duration') or 0.1)))
    return res[:16]


def quote_lines(q):
    return [ascii_text(s.text) for s in q.iter('string')][:16] if q is not None else []


def pack_str(s):
    b = s.encode('latin-1', 'replace')[:250]
    return struct.pack('<B', len(b)) + b


def write_npcs(src, out):
    base = os.path.join(src, 'props', 'textures', 'old_man')
    roots = []
    for sub in ('lines', 'merchant'):
        d = os.path.join(base, sub)
        for fn in sorted(os.listdir(d)):
            if fn.endswith('.xml'):
                r = ET.parse(os.path.join(d, fn)).getroot()
                if r.tag == 'MerchantSettings':
                    r = r.find('settings')
                if r is not None and r.findtext('name'):
                    roots.append(r)
    blobs = []
    for i, r in enumerate(roots):
        name = r.findtext('name')
        tex = os.path.join(base, name + '.xnb')
        if not os.path.exists(tex):
            continue
        write_t8(texture_rgba(tex), os.path.join(out, f'npc{len(blobs)}.t8'))
        cx, cy = int(r.findtext('sprite_cells/X')), int(r.findtext('sprite_cells/Y'))
        tb = r.find('trigger_box')
        sr = tb.find('screen_rect') if tb is not None else None
        srect = tuple(int(sr.findtext(k) or 0) for k in ('X', 'Y', 'Width', 'Height')) if sr is not None else (0, 0, 0, 0)
        b = struct.pack('<BBhhH', cx, cy, int(r.findtext('position/X')), int(r.findtext('position/Y')),
                        int(r.findtext('home_screen') or 1))
        b += struct.pack('<hhff4h', int(tb.findtext('width') or 0), int(tb.findtext('height') or 0),
                         float(tb.findtext('center_x') or 0.5), float(tb.findtext('center_y') or 0.5), *srect)
        b += struct.pack('<ffH', float(r.findtext('talk_delay') or 0), float(r.findtext('talking_speed') or 0),
                         int(r.findtext('bubble_format/width') or 160))
        for anim in ('busy_animation', 'talk_animation'):
            fr = frames(r.find(anim))
            b += struct.pack('<B', len(fr)) + b''.join(struct.pack('<Bf', ix, du) for ix, du in fr)
        quotes = []
        if r.find('intro_quote') is not None and quote_lines(r.find('intro_quote')):
            quotes.append((0, 0, quote_lines(r.find('intro_quote'))))
        for sc in r.iter('OldManScreen'):
            for q in sc.iter('OldManQuote'):
                quotes.append((1, int(sc.findtext('screen') or 0), quote_lines(q)))
        loop = r.find('looping_lines')
        if loop is not None:
            for q in loop.iter('OldManQuote'):
                quotes.append((2, 0, quote_lines(q)))
        quotes = [q for q in quotes if q[2]][:96]
        b += struct.pack('<B', len(quotes))
        for kind, scr, lines in quotes:
            b += struct.pack('<BHB', kind, scr, len(lines)) + b''.join(pack_str(l) for l in lines)
        blobs.append(b)
    with open(os.path.join(out, 'npc.bin'), 'wb') as f:
        f.write(b'JKNP' + struct.pack('<H', len(blobs)))
        for b in blobs:
            f.write(struct.pack('<I', len(b)) + b)
    return len(blobs)


def write_signs(src, out):
    """Carteles y libros (RattmanEntity) de props/messages: zona, punto del texto y lineas."""
    base = os.path.join(src, 'props', 'messages')
    signs = []
    for fn in sorted(os.listdir(base)):
        if not fn.endswith('.xml'):
            continue
        r = ET.parse(os.path.join(base, fn)).getroot()
        hb = r.find('hitbox')
        w, h = int(hb.findtext('Width') or 0), int(hb.findtext('Height') or 0)
        x = int(hb.findtext('X') or 0) - int(w * float(r.findtext('hitbox_center/X') or 0))
        y = int(hb.findtext('Y') or 0) - int(h * float(r.findtext('hitbox_center/Y') or 0))
        lines = quote_lines(r.find('text'))
        if not lines:
            continue
        signs.append((int(r.findtext('screen') or 1), x, y, w, h,
                      int(r.findtext('text_anchor/X') or 0), int(r.findtext('text_anchor/Y') or 0),
                      int(r.findtext('bubble_format/width') or 160), lines))
    with open(os.path.join(out, 'signs.bin'), 'wb') as f:
        f.write(b'JKSG' + struct.pack('<H', len(signs)))
        for sc, x, y, w, h, ax, ay, bw, lines in signs:
            f.write(struct.pack('<HhhhhhhHB', sc, x, y, w, h, ax, ay, bw, len(lines)))
            f.write(b''.join(pack_str(l) for l in lines))
    return len(signs)


def main():
    game, out = sys.argv[1], sys.argv[2]
    load_language(sys.argv[3] if len(sys.argv) > 3 else None)
    print(f'textos de idioma: {len(LANG)}')
    src = os.path.join(game, 'Content')
    write_font(src, out)
    print('fuente: font.t8 + font.bin')
    print(f'npc: {write_npcs(src, out)}')
    print(f'carteles/libros: {write_signs(src, out)}')


if __name__ == '__main__':
    main()
