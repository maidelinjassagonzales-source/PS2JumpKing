"""Auditoria: compara lo que el port convierte con lo que trae TU copia del juego.
Solo imprime cuentas (no textos). Uso: python -I tools/audit.py "<carpeta del juego>" assets_ps2 <idioma.resx>"""
import os, re, struct, sys
import xml.etree.ElementTree as ET


def npc_dialogues(content, assets, lang):
    base = os.path.join(content, 'props', 'textures', 'old_man')
    rep = {'quotes_total': 0, 'lines_total': 0, 'lines_untranslated': 0, 'by_kind': {}, 'lost': {}}
    per_npc = []
    for sub in ('lines', 'merchant'):
        for fn in sorted(os.listdir(os.path.join(base, sub))):
            if not fn.endswith('.xml'):
                continue
            r = ET.parse(os.path.join(base, sub, fn)).getroot()
            if r.tag == 'MerchantSettings':
                r = r.find('settings')
            name = r.findtext('name')
            counts = {}
            for tag, kind in (('intro_quote', 'intro'), ('screens', 'pantalla'), ('looping_lines', 'bucle'),
                              ('flag_quotes', 'evento'), ('falls', 'caidas'), ('items', 'objetos')):
                node = r.find(tag)
                if node is None:
                    continue
                qs = list(node.iter('OldManQuote')) if tag != 'intro_quote' else [node]
                for q in qs:
                    lines = [s.text or '' for s in q.iter('string')]
                    if not lines:
                        continue
                    counts[kind] = counts.get(kind, 0) + 1
                    rep['quotes_total'] += 1
                    rep['lines_total'] += len(lines)
                    rep['lines_untranslated'] += sum(1 for l in lines if l.replace('\n', '') not in lang)
                    if len(lines) > 16:
                        rep['lost']['lineas >16 por frase'] = rep['lost'].get('lineas >16 por frase', 0) + len(lines) - 16
            for k, v in counts.items():
                rep['by_kind'][k] = rep['by_kind'].get(k, 0) + v
            total = sum(counts.values())
            if total > 96:
                rep['lost']['frases >96 por NPC'] = rep['lost'].get('frases >96 por NPC', 0) + total - 96
            for k in ('evento', 'caidas', 'objetos'):
                if k in counts:
                    rep['lost'][f'frases de {k} (no portadas)'] = rep['lost'].get(f'frases de {k} (no portadas)', 0) + counts[k]
            per_npc.append((name, total, os.path.exists(os.path.join(base, name + '.xnb'))))
    # lo que hay en npc.bin
    d = open(os.path.join(assets, 'npc.bin'), 'rb').read()
    n = struct.unpack_from('<H', d, 4)[0]
    rep['npc_in_port'] = n
    rep['npc_in_game'] = len(per_npc)
    rep['npc_without_sprite'] = [p[0] for p in per_npc if not p[2]]
    return rep


def signs(content, assets):
    base = os.path.join(content, 'props', 'messages')
    total = sum(1 for f in os.listdir(base) if f.endswith('.xml'))
    d = open(os.path.join(assets, 'signs.bin'), 'rb').read()
    return total, struct.unpack_from('<H', d, 4)[0]


def screens(content, assets):
    d = open(os.path.join(assets, 'level.bin'), 'rb').read()
    n = struct.unpack_from('<I', d, 4)[0]
    with_blocks, missing_mid, missing_fg_used = [], [], []
    for s in range(n):
        g = d[8 + s * 2716 + 16: 8 + s * 2716 + 16 + 2700]
        if any(v & 15 for v in g):
            with_blocks.append(s)
            if not os.path.exists(os.path.join(assets, 'screens', f'm{s}.t8')):
                missing_mid.append(s + 1)
    layers = {k: len([f for f in os.listdir(os.path.join(assets, 'screens')) if f.startswith(k)]) for k in 'bmf'}
    src_layers = {k: len(os.listdir(os.path.join(content, 'screens', d))) for k, d in
                  (('b', 'background'), ('m', 'midground'), ('f', 'foreground'))}
    scrolling = len([f for f in os.listdir(os.path.join(content, 'screens', 'scrolling')) if f.endswith('.xml')])
    masks = len(os.listdir(os.path.join(content, 'screens', 'masks')))
    return n, with_blocks, missing_mid, layers, src_layers, scrolling, masks


def main():
    game, assets, resx = sys.argv[1], sys.argv[2], sys.argv[3]
    content = os.path.join(game, 'Content')
    lang = {e.get('name') for e in ET.parse(resx).getroot().iter('data')}

    r = npc_dialogues(content, assets, lang)
    print('== DIALOGOS DE NPC')
    print(f"NPC en el juego: {r['npc_in_game']} | en el port: {r['npc_in_port']} | sin sprite: {r['npc_without_sprite']}")
    print(f"frases: {r['quotes_total']} ({r['lines_total']} lineas) | sin traduccion: {r['lines_untranslated']}")
    print('frases por tipo:', r['by_kind'])
    print('NO llegan al port:', r['lost'] or 'nada')
    t, p = signs(content, assets)
    print(f'== CARTELES Y LIBROS: juego {t} | port {p}')

    n, wb, mm, lay, src, scr, masks = screens(content, assets)
    print('== PANTALLAS')
    print(f'pantallas posibles: {n} | con bloques: {len(wb)} (de la 1 a la {wb[-1] + 1 if wb else 0})')
    print(f'capas convertidas: {lay} | archivos en el juego: {src}')
    print(f'pantallas con bloques y sin capa media: {mm or "ninguna"}')
    print(f'fondos con scroll (no portados): {scr} | mascaras de capa (no portadas): {masks}')


if __name__ == '__main__':
    main()
