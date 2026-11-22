"""Menu de titulo y pausa: logo, marco, cursor, textos del menu y musica del menu, desde TU copia.
Uso: python -I tools/build_ui.py "<carpeta del juego>" assets_ps2 <idioma.resx>   (salida fuera del repo)"""
import os, struct, sys
import xml.etree.ElementTree as ET
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from build_assets import write_t8, texture_rgba
from build_audio import to_pcm, write_pcm

# Claves de texto que usa el port (en este orden; el ELF las lee por indice).
KEYS = [
    'LANGUAGEMANAGER_PRESS_BUTTON_TO_START',
    'GAMETITLESCREEN_CONTINUE',
    'GAMETITLESCREEN_NEW_GAME',
    'PAUSEMANAGER_RESUME',
    'PAUSEMANAGER_SAVEEXIT',
    'MENUFACTORY_SAVEGAME_AREYOUSURE',
    'MENUFACTORY_YES',
    'MENUFACTORY_NO',
]


def main():
    game, out, resx = sys.argv[1], sys.argv[2], sys.argv[3]
    src = os.path.join(game, 'Content')
    lang = {d.get('name'): d.findtext('value') or '' for d in ET.parse(resx).getroot().iter('data')}
    with open(os.path.join(out, 'ui_text.bin'), 'wb') as f:
        f.write(b'JKTX' + struct.pack('<H', len(KEYS)))
        for k in KEYS:
            t = lang.get(k, k).replace('{0}', 'X')          # boton de salto del mando: X
            b = t.encode('latin-1', 'replace')[:250]
            f.write(struct.pack('<B', len(b)) + b)
    for name, path in (('title.t8', 'title_logo.xnb'), ('frame.t8', 'gui/frame.xnb'), ('cursor.t8', 'gui/cursor.xnb')):
        img = texture_rgba(os.path.join(src, path))
        write_t8(img, os.path.join(out, name))
        print(name, img.shape[1], 'x', img.shape[0])
    mus = os.path.join(src, 'audio', 'music', 'menu loop')
    os.makedirs(os.path.join(out, 'audio'), exist_ok=True)
    for n in ('menu_intro', 'menu_loop'):
        write_pcm(os.path.join(out, 'audio', n + '.pcm'), to_pcm(os.path.join(mus, n + '.xnb'), 2))
    print('textos:', len(KEYS), '| musica: menu_intro, menu_loop')


if __name__ == '__main__':
    main()
