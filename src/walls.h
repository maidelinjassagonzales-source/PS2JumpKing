/* Paredes ocultas (RaymanWallEntity): textura que tapa una zona y se desvanece en 0.2 s mientras
 * el rey toca alguna de sus zonas de contacto. Datos: assets_ps2/walls.bin + walls/wN.t8. */
#ifndef WALLS_H
#define WALLS_H
#include <gsKit.h>
#include "jk/king.h"

int  walls_init(GSGLOBAL *gs);
void walls_update(GSGLOBAL *gs, const jk_king *k, float dt);
void walls_draw(GSGLOBAL *gs, int screen, float sx, float sy);
#endif
