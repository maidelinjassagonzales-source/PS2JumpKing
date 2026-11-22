/* Logica del rey encima del cuerpo: andar, cargar salto, saltar, caida fallida ("splat").
 * Reproduce el resultado del arbol de comportamiento del original con una maquina de estados. */
#ifndef JK_KING_MODULE_H
#define JK_KING_MODULE_H
#include "body.h"

typedef struct { int left, right, jump; } jk_input;

typedef enum { JK_RUNNING, JK_SUCCESS, JK_FAILURE, JK_NONE } jk_result;

typedef struct {
    jk_result last;     /* resultado del ultimo tick en que corrio */
    int       last_tick;
} jk_node;

typedef struct {
    jk_body body;
    int     facing;     /* -1 izquierda, 1 derecha */
    int     screen;     /* pantalla de la camara */

    /* entrada */
    int     can_jump, prev_jump;

    /* carga de salto */
    jk_node jump_node;
    float   charge;
    int     dpad_hist[4], dpad_count;   /* [0] = mas reciente */

    /* caida fallida */
    jk_node fail_node;
    float   splat_timer;
    int     splat_done, splat_eval;
    int     splatted;   /* para el dibujo */

    /* margen de 1/30 s para seguir cargando en el aire */
    jk_node grace_node;
    float   grace_timer;

    int     tick;
} jk_king;

void jk_king_init(jk_king *k, float x, float y);
void jk_king_update(jk_king *k, const jk_level *lv, jk_input in);

#endif
