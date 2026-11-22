/* Objetos del mundo (BasicWorldItemEntity / WorldItemComp) e inventario (InventoryManager).
 * Datos: assets_ps2/items.bin + items/iN.t8 (generados desde la copia del usuario). */
#ifndef ITEMS_H
#define ITEMS_H
#include <gsKit.h>
#include "jk/king.h"

#define ITEM_KINDS 18

int  items_init(GSGLOBAL *gs);
void items_update(GSGLOBAL *gs, const jk_king *k);
void items_draw(GSGLOBAL *gs, int screen, float sx, float sy);
void items_spawn(int item, int screen1, int x, int y);   /* p. ej. el que suelta el cuervo */
int  items_take_pickup(void);                            /* 1 si se recogio algo este frame */
int  items_count(int item);
const char *items_name(int item);
int  items_save(unsigned char *out, int max);
void items_load(const unsigned char *in, int size);
void items_reset(void);
#endif
