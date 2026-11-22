"""Final normal (NormalEnding): zona izquierda de la hoja del rey (princesa, pareja, extras del final),
imagen final, musica y sonidos, desde TU copia. Salida solo en assets_ps2/.
Uso: python -I tools/build_ending.py "<carpeta del juego>" assets_ps2"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from build_assets import write_t8, texture_rgba
from build_audio import to_pcm, write_pcm


def main():
    game, out = sys.argv[1], sys.argv[2]
    src = os.path.join(game, 'Content')
    base = texture_rgba(os.path.join(src, 'king', 'base.xnb'))
    # KingSpriteLayers.Babe / BabeCouple / Ending1Misc muestrean la parte izquierda de la hoja
    write_t8(base[0:288, 0:224], os.path.join(out, 'ending_sheet.t8'))
    write_t8(texture_rgba(os.path.join(src, 'ending', 'imagecrown.xnb')), os.path.join(out, 'ending_image.t8'))
    write_pcm(os.path.join(out, 'audio', 'ending.pcm'), to_pcm(os.path.join(src, 'audio', 'music', 'ending.xnb'), 2))
    for name, rel in (('babe_jump', 'babe/babe jump'), ('babe_kiss', 'babe/babe kiss'), ('babe_mou', 'babe/babe mou'),
                      ('babe_scream', 'babe/babe scream'), ('babe_surprised', 'babe/babe surprised2'), ('plink', 'plink')):
        p = os.path.join(src, 'audio', rel + '.xnb')
        if os.path.exists(p):
            write_pcm(os.path.join(out, 'audio', f'sfx_{name}.pcm'), to_pcm(p, 1))
    print('final normal: hoja, imagen, musica y sonidos')


if __name__ == '__main__':
    main()
