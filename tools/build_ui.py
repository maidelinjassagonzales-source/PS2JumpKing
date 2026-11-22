"""Menus (titulo, pausa, extras, opciones): logo, marco, cursor, iconos, fuentes, textos, creditos y
musica del menu, desde TU copia. Salida solo en assets_ps2/ (fuera del repo).
Uso: python -I tools/build_ui.py "<carpeta del juego>" assets_ps2 <idioma.resx> [_ref/decomp]"""
import os, re, struct, sys
import xml.etree.ElementTree as ET
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from build_assets import write_t8, texture_rgba
from build_audio import to_pcm, write_pcm
from build_npcs import write_font

# Claves de texto que usa el port, en este orden (el ELF las lee por indice: ver T_* en src/ui.c).
KEYS = [
    'LANGUAGEMANAGER_PRESS_BUTTON_TO_START',
    'GAMETITLESCREEN_CONTINUE',
    'GAMETITLESCREEN_NEW_GAME',
    'PAUSEMANAGER_RESUME',
    'PAUSEMANAGER_SAVEEXIT',
    'MENUFACTORY_SAVEGAME_AREYOUSURE',
    'MENUFACTORY_YES',
    'MENUFACTORY_NO',
    'GAMETITLESCREEN_COPYRIGHT_NEXILE',
    'GAMETITLESCREEN_ALL_PROGESS_SAVED',
    'PAUSEMANAGER_WORKSHOP',
    'GAMETITLESCREEN_EXTRAS',
    'GAMETITLESCREEN_OPTIONS',
    'PAUSEMANAGER_INVENTORY',
    'MENUFACTORY_CREDITS',
    'MENUFACTORY_CREDIT_ATTRIBUTION',
    'MENUFACTORY_TOTAL_STATS',
    'MENUFACTORY_CREDIT_FONT_ATTRIBUTION',
    'ATTRIBUTION_JKPLUS',
    'SESSIONINFO_ATTEMPTS',
    'TIMEINFO_TIME',
    'JUMPINFO_JUMPS',
    'FALLINFO_FALLS',
    'MENUFACTORY_GRAPHICS',
    'MENUFACTORY_CONTROLS',
    'MENUFACTORY_AUDIO',
    'MENUFACTORY_INGAME',
    'MENUFACTORY_OPTIONS_TIMER',
    'MENUFACTORY_QUICKLOAD',
    'MENUFACTORY_DISPLAY_TIMER',
    'MENUFACTORY_TIMER_DEFAULT',
    'MENUFACTORY_TIMER_PRECISE',
    'SOUND_TOGGLE_MUSIC',
    'SOUND_TOGGLE_SFX',
    'SOUND_TOGGLE_AMBIENCE',
    'MENUFACTORY_INPUT_SCAN_FOR_DEVICES',
    'MENUFACTORY_INPUT_BIND_PRIMARY',
    'MENUFACTORY_INPUT_BIND_SECONDARY',
    'PAUSEMANAGER_OPTIONS',
    'LANGUAGEMANAGER_GAME_INTRO_DESC',
]


def pack(t):
    b = t.encode('latin-1', 'replace')[:250]
    return struct.pack('<B', len(b)) + b


def credits(decomp, lang):
    """Nombres y cargos de LanguageManager (name_credits + FULL_CREDITS), como CreateExtras."""
    src = open(os.path.join(decomp, 'JumpKing', 'LanguageManager.cs'), encoding='utf-8').read()
    consts = dict(re.findall(r'public static string (_\w+) = "([^"]*)";', src))
    order = re.search(r'name_credits = new string\[\d+\] \{([^}]*)\}', src).group(1)
    names = [c.strip() for c in order.split(',')]
    full = re.search(r'FULL_CREDITS => new Credit\[\d+\]\s*\{(.*?)\};', src, re.S).group(1)
    roles = []
    for key, people in re.findall(r'new Credit\(language\.(\w+), ([^)]*)\)', full):
        roles.append((lang.get(key, key), [p.strip() for p in people.split(',')]))
    out = []
    for n in names:
        r = ', '.join(h for h, ppl in roles if n in ppl)
        out.append((consts.get(n, n), r))
    return out, consts.get('_FONT_ATTRIBUTION', '')


def main():
    game, out, resx = sys.argv[1], sys.argv[2], sys.argv[3]
    decomp = sys.argv[4] if len(sys.argv) > 4 else os.path.join('_ref', 'decomp')
    src = os.path.join(game, 'Content')
    lang = {d.get('name'): d.findtext('value') or '' for d in ET.parse(resx).getroot().iter('data')}
    cred, font_attr = credits(decomp, lang)

    with open(os.path.join(out, 'ui_text.bin'), 'wb') as f:
        f.write(b'JKTX' + struct.pack('<H', len(KEYS)))
        for k in KEYS:
            t = lang.get(k, k)
            if k == 'LANGUAGEMANAGER_PRESS_BUTTON_TO_START':
                t = t.replace('{0}', 'X')                     # boton de salto del mando: X
            if k == 'MENUFACTORY_CREDIT_FONT_ATTRIBUTION':
                t = t.replace('{0}', font_attr)
            f.write(pack(t))
    with open(os.path.join(out, 'credits.bin'), 'wb') as f:
        f.write(b'JKCR' + struct.pack('<H', len(cred)))
        for name, role in cred:
            f.write(pack(name) + pack(role))

    for name, path in (('title.t8', 'title_logo.xnb'), ('frame.t8', 'gui/frame.xnb'), ('cursor.t8', 'gui/cursor.xnb'),
                       ('back.t8', 'gui/back.xnb'), ('checkbox.t8', 'gui/checkbox.xnb'), ('check.t8', 'gui/check.xnb'),
                       ('slider.t8', 'gui/slider.xnb'), ('volume.t8', 'gui/volume.xnb'), ('arrows.t8', 'gui/arrows.xnb'), ('nexile.t8', 'JK_Nexile_Logo.xnb')):
        img = texture_rgba(os.path.join(src, path))
        write_t8(img, os.path.join(out, name))
        print(name, img.shape[1], 'x', img.shape[0])

    mus = os.path.join(src, 'audio', 'music', 'menu loop')
    os.makedirs(os.path.join(out, 'audio'), exist_ok=True)
    for n in ('menu_intro', 'menu_loop'):
        write_pcm(os.path.join(out, 'audio', n + '.pcm'), to_pcm(os.path.join(mus, n + '.xnb'), 2))
    write_pcm(os.path.join(out, 'audio', 'opening.pcm'),                       # IntroState: musica de apertura
              to_pcm(os.path.join(src, 'audio', 'music', 'opening theme.xnb'), 2))
    write_font(src, out, 'sf_litter_lover2_bold', 'menufont')     # MenuFont
    write_font(src, out, 'sf_small', 'smallfont')                  # MenuFontSmall
    for n, rel in (('title_hit', 'gui_sfx/title_hit'), ('press_start', 'press_start'),
                   ('select', 'gui_sfx/selectC'), ('menu_open', 'gui_sfx/menu_open'), ('menu_fail', 'gui_sfx/menu_fail')):
        write_pcm(os.path.join(out, 'audio', 'sfx_' + n + '.pcm'), to_pcm(os.path.join(src, 'audio', rel + '.xnb'), 1))
    print(f'textos: {len(KEYS)} | creditos: {len(cred)} | fuentes menu y pequena | iconos | musica y sonidos del menu')


if __name__ == '__main__':
    main()
