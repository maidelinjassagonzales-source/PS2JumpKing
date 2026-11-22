/* Carga de los datos convertidos por tools/build_assets.py desde host: (PCSX2) o la carpeta del ELF. */
#ifndef ASSETS_H
#define ASSETS_H
#include <gsKit.h>

void *assets_load(const char *name, int *size);              /* NULL si no existe */
int   assets_t8(GSGLOBAL *gs, const char *name, GSTEXTURE *t, int keep_vram);  /* 0 = ok */

#endif
