/* Props animados (LoopingProp) y pajaros (RavenEntity) de la pantalla actual.
 * Datos: assets_ps2/props.bin, birds.bin, props/pN.t8, props/birdN.t8, audio/birdN.pcm. */
#ifndef PROPS_H
#define PROPS_H
#include <gsKit.h>
#include "jk/king.h"

int  props_init(GSGLOBAL *gs);
void props_update(GSGLOBAL *gs, const jk_king *k, float dt);
void props_draw(GSGLOBAL *gs, int screen, float sx, float sy);
int  props_take_bird_sfx(void);            /* indice del sonido de vuelo a reproducir, o -1 */
int  props_save(unsigned char *out, int max);
void props_load(const unsigned char *in, int size);
void props_reset(void);
#endif
