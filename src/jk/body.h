/* Cuerpo fisico del rey: paso por frame con el mismo orden de operaciones que el original
 * (BodyComp.SetupBaseBehaviours) y los comportamientos de bloque (hielo, arena, agua, nieve). */
#ifndef JK_BODY_H
#define JK_BODY_H
#include "level.h"

enum {
    JK_FLAG_CAPPED_X  = 1 << 0,
    JK_FLAG_BUMP      = 1 << 1,   /* golpe contra pared/techo (sonido) */
    JK_FLAG_TOUCH_Y   = 1 << 2,
    JK_FLAG_X_SLOPE   = 1 << 3,
    JK_FLAG_TELEPORT  = 1 << 4,
};

typedef struct {
    float x, y;              /* esquina superior izquierda */
    float vx, vy;
    float last_vx, last_vy;  /* velocidad al inicio del frame, antes de resolver choques */
    int   w, h;
    int   on_ground;
    int   knocked;           /* rebotado contra pared o pendiente */
    int   flags;             /* JK_FLAG_* del ultimo frame */
    int   ystep;             /* direccion de la ultima resolucion en Y (+1 baja, -1 sube) */
    /* "IsPlayerOnBlock" de cada comportamiento de bloque */
    int   on_ice, on_sand, on_snow, in_water, was_in_water;
    /* viento */
    int   wind_enabled, last_screen;
    float run_time;          /* segundos de partida: el viento oscila con el tiempo */
} jk_body;

void    jk_body_init(jk_body *b, float x, float y);
jk_rect jk_body_hitbox(const jk_body *b);
float   jk_body_multiplier(const jk_body *b);   /* 0.5 en agua (BodyComp.GetMultipliers) */
/* screen se actualiza si el rey se teletransporta. */
void    jk_body_step(jk_body *b, const jk_level *lv, int *screen);

#endif
