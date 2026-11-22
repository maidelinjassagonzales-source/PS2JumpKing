/* Fondos con scroll (ScrollingBackground): capas que se desplazan en bucle cada 480 px.
 * Datos: assets_ps2/scroll.bin + scroll/sN.t8 (convertidos desde la copia del usuario). */
#ifndef SCROLL_H
#define SCROLL_H
#include <gsKit.h>

int  scroll_init(void);
void scroll_update(GSGLOBAL *gs, int screen, float dt);
void scroll_draw(GSGLOBAL *gs, int screen, int foreground, float ox, float oy, float sx, float sy);
#endif
