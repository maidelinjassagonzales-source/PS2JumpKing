#include <math.h>
#include "camera.h"
#include "values.h"

static int sign_f(float v) { return (v > 0.0f) - (v < 0.0f); }

int jk_camera_update(int current, int total, int center_y, float vy, int force)
{
    int target = -(int)floorf((float)center_y / (float)JK_SCREEN_H);
    if (current == target)
        return current;
    if (!force) {
        if (vy == 0.0f)
            return current;
        int delta = target - current;
        int d = (delta > 0) - (delta < 0);
        /* solo cambia en la direccion del movimiento, y subiendo despacio no cambia */
        if (d != sign_f(0.0f - vy) || (vy < 0.0f && fabsf(vy) < 3.0f))
            return current;
    }
    if (target > total - 1)
        target = total - 1;
    return target;
}
