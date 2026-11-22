#include "values.h"

jk_values JK;

/* Suma de i*s para i en [first, last], acumulada en float en el mismo orden que el original. */
static float sum_scaled(int last, int first, float s)
{
    float acc = 0.0f;
    for (int i = first; i <= last; i++)
        acc += (float)i * s;
    return acc;
}

/* Cuantos pasos de aceleracion "step" hacen falta para subir target pixeles; devuelve la velocidad inicial. */
static float strength_for_height(int target, float step)
{
    float height = 0.0f, speed = 0.0f;
    int steps = 0;
    while (height < (float)target) {
        speed += step;
        height += speed;
        steps++;
    }
    return (float)steps * step;
}

void jk_values_init(void)
{
    const float fps_scale = 0.5f;               /* valores pensados a 30 fps, juego a 60 */
    const int   jump30    = 17;                 /* velocidad de salto a 30 fps */
    float height30 = (float)(jump30 * (jump30 + 1)) * 0.5f;

    JK.gravity      = 1.0f * height30 / sum_scaled((int)(jump30 / fps_scale), 1, 1.0f);
    JK.jump         = 0.0f - strength_for_height((int)height30, JK.gravity);
    JK.walk_speed   = 3.0f * fps_scale;
    JK.air_speed    = 7.0f * fps_scale;
    JK.bounce       = 0.5f;
    JK.max_fall     = 20.0f * fps_scale;
    JK.max_slide    = 15.0f * fps_scale;
    JK.ice_friction = 0.2f * fps_scale;
    JK.charge_time  = 0.6f;
    JK.splat_time   = 0.5f;
}
