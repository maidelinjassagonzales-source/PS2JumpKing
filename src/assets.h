/* Carga de los datos convertidos por tools/build_assets.py desde host: (PCSX2) o la carpeta del ELF. */
#ifndef ASSETS_H
#define ASSETS_H
#include <gsKit.h>

void *assets_load(const char *name, int *size);              /* NULL si no existe */
int   assets_t8(GSGLOBAL *gs, const char *name, GSTEXTURE *t, int keep_vram);  /* 0 = ok */

/* VRAM por pantalla: lo que sobra tras lo permanente (framebuffer, capas, rey, menus) es un pool
 * que se vacia al cambiar de pantalla y se llena con NPC, paredes ocultas, props y pajaros. */
void  assets_pool_init(GSGLOBAL *gs);
void  assets_pool_reset(void);
int   assets_pool_generation(void);
int   assets_t8_pool(GSGLOBAL *gs, const char *name, GSTEXTURE *t);   /* 0 = ok */

#endif
