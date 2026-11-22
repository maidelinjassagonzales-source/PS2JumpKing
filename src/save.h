/* Guardado en memory card (puerto 1): mc0:/JKPS2SAVE/save.bin.
 * Contenido propio del port: posicion del rey, pantalla, estadisticas, estado de los NPC y opciones. */
#ifndef SAVE_H
#define SAVE_H

#define SAVE_BLOB_MAX 8192

int  save_init(void);                                 /* carga MCMAN/MCSERV; 0 = ok */
int  save_exists(void);                               /* hay partida guardada valida */
int  save_write(const void *data, int size);          /* 0 = ok */
int  save_read(void *data, int max);                  /* bytes leidos o -1 */
void save_delete(void);                               /* nueva partida: borra la anterior */
#endif
