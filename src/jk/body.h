/* Cuerpo fisico del rey: paso por frame con el mismo orden de operaciones que el original. */
#ifndef JK_BODY_H
#define JK_BODY_H
#include "level.h"

enum {
    JK_FLAG_CAPPED_X = 1 << 0,
    JK_FLAG_BUMP     = 1 << 1,   /* sonaria el golpe contra pared/techo */
    JK_FLAG_TOUCH_Y  = 1 << 2,
};

typedef struct {
    float x, y;          /* esquina superior izquierda */
    float vx, vy;
    float last_vx, last_vy;  /* velocidad al inicio del frame, antes de resolver choques */
    int   w, h;
    int   on_ground;
    int   knocked;       /* rebotado contra una pared: solo cambia el sprite */
    int   flags;         /* JK_FLAG_* del ultimo frame */
} jk_body;

void    jk_body_init(jk_body *b, float x, float y);
jk_rect jk_body_hitbox(const jk_body *b);
void    jk_body_step(jk_body *b, const jk_level *lv, int screen);

#endif
