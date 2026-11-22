/* Final normal (NormalEnding + EndingKing/EndingBabe/EndingCherub + EndingPictureScreen).
 * Datos: assets_ps2/ending_sheet.t8, ending_image.t8, audio/ending.pcm y sonidos (copia del usuario). */
#ifndef ENDING_H
#define ENDING_H
#include <gsKit.h>
#include "jk/king.h"

int  ending_check(const jk_king *k);                 /* condiciones de victoria del original */
void ending_start(GSGLOBAL *gs, const jk_king *k);
int  ending_update(int any_input);                   /* escena: 1 = termino; imagen: 1 = cerrar */
void ending_show_picture(void);                      /* EndingPictureScreen tras los creditos */
void ending_camera(float *ox, float *oy);            /* desplazamiento de camara (EndingScroll) */
int  ending_in_picture(void);                        /* mostrando la imagen final */
void ending_draw(GSGLOBAL *gs, GSTEXTURE *king_sheet, float sx, float sy);
#endif
