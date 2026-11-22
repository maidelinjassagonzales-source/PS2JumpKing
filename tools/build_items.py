"""Objetos del mundo (worlditems.xml) y nombres de inventario desde TU copia. Salida solo en assets_ps2/.
Uso: python -I tools/build_items.py "<carpeta del juego>" assets_ps2 <idioma.resx>"""
import os, struct, sys
import xml.etree.ElementTree as ET
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from build_assets import write_t8, texture_rgba

# Orden del enum Items del juego
ITEMS = ['Crown', 'Shoes', 'GoldRing', 'CrownNBP', 'Ruby', 'SnakeRing', 'GiantBoots', 'Silver', 'Cap', 'GnomeHat',
         'Tunic', 'YellowShoes', 'CrownOwl', 'CapeOwl', 'BabeGhostItem', 'GhostFragment', 'Shroom', 'BugNote']
# Sprite en el mundo (WorldItemFactory) y texto del inventario (MenuFactory: skin_names / coin_names)
SPRITE = {'GoldRing': 'gold_ring', 'Ruby': 'ruby', 'Silver': 'silver_coin', 'GiantBoots': 'shoes_iron',
          'GnomeHat': 'gnome_hat', 'Tunic': 'tunic', 'YellowShoes': 'yellow_shoes', 'Cap': 'cap',
          'GhostFragment': 'ghost_fragment', 'Shroom': 'shroom', 'BugNote': 'bug_note'}
NAME_KEY = {'GiantBoots': 'MENUFACTORY_INVENTORY_GIANTBOOTS_DESC', 'SnakeRing': 'MENUFACTORY_INVENTORY_SNAKERING_DESC',
            'Crown': 'MENUFACTORY_INVENTORY_CROWN_DESC', 'CrownNBP': 'MENUFACTORY_INVENTORY_CROWNNBP_DESC',
            'CrownOwl': 'MENUFACTORY_CROWN_OWL', 'CapeOwl': 'MENUFACTORY_KINGS_CAPE',
            'Cap': 'MENUFACTORY_INVENTORY_CAP_DESC', 'GnomeHat': 'MENUFACTORY_GNOME_HAT',
            'Shoes': 'MENUFACTORY_INVENTORY_SHOES_DESC', 'YellowShoes': 'MENUFACTORY_YELLOW_SHOES',
            'Tunic': 'MENUFACTORY_TUNIC', 'GoldRing': 'MENUFACTORY_INVENTORY_GOLDRING_DESC',
            'Silver': 'MENUFACTORY_INVENTORY_SILVER_DESC', 'Ruby': 'MENUFACTORY_INVENTORY_RUBY_DESC',
            'GhostFragment': 'MENUFACTORY_GHOST_FRAGMENT', 'Shroom': 'MENUFACTORY_SHROOM',
            'BugNote': 'MENUFACTORY_INVENTORY_BUG_NOTE_DESC'}


def main():
    game, out, resx = sys.argv[1], sys.argv[2], sys.argv[3]
    src = os.path.join(game, 'Content', 'props', 'worlditems')
    lang = {d.get('name'): d.findtext('value') or '' for d in ET.parse(resx).getroot().iter('data')}
    os.makedirs(os.path.join(out, 'items'), exist_ok=True)
    for name, tex in SPRITE.items():
        write_t8(texture_rgba(os.path.join(src, tex + '.xnb')), os.path.join(out, 'items', f'i{ITEMS.index(name)}.t8'))
    placed = []
    for st in ET.parse(os.path.join(src, 'worlditems.xml')).getroot().iter('WorldItemState'):
        it = st.findtext('item')
        if it in SPRITE:
            placed.append((ITEMS.index(it), int(st.findtext('screen') or 1),
                           int(st.findtext('_x') or 0), int(st.findtext('_y') or 0)))
    with open(os.path.join(out, 'items.bin'), 'wb') as f:
        f.write(b'JKIT' + struct.pack('<BH', len(ITEMS), len(placed)))
        for it in ITEMS:
            t = lang.get(NAME_KEY.get(it, ''), it).split('\n')[0].encode('latin-1', 'replace')[:60]
            f.write(struct.pack('<BB', 1 if it in SPRITE else 0, len(t)) + t)
        for it, sc, x, y in placed:
            f.write(struct.pack('<BHhh', it, sc, x, y))
    print(f'objetos en el mundo: {len(placed)} | sprites: {len(SPRITE)} | nombres: {len(ITEMS)}')


if __name__ == '__main__':
    main()
