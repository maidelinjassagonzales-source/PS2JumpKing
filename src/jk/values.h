/* Constantes de movimiento del rey. Fuente: docs/procedencia.md (PlayerValues, ErikMath). */
#ifndef JK_VALUES_H
#define JK_VALUES_H

#define JK_SCREEN_W      480
#define JK_SCREEN_H      360
#define JK_KING_W        18
#define JK_KING_H        26
/* Paso fijo del juego original: 1/60 s guardado como float. */
#define JK_DT            0.01666666753590107f
#define JK_LENIENCY_TIME (1.0f / 30.0f)

typedef struct {
    float gravity;      /* derivada: altura de salto a 30 fps reescalada a 60 fps */
    float jump;         /* velocidad Y del salto a carga completa (negativa = arriba) */
    float walk_speed;
    float air_speed;
    float bounce;
    float max_fall;
    float max_slide;
    float ice_friction;
    float charge_time;
    float splat_time;
} jk_values;

extern jk_values JK;

void jk_values_init(void);

#endif
