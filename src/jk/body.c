#include <math.h>
#include "body.h"
#include "values.h"
#include "camera.h"

void jk_body_init(jk_body *b, float x, float y)
{
    *b = (jk_body){0};
    b->x = x; b->y = y;
    b->w = JK_KING_W; b->h = JK_KING_H;
    b->on_ground = 1;
}

/* La caja se toma truncando la posicion hacia cero (Vector2.ToPoint). */
jk_rect jk_body_hitbox(const jk_body *b)
{
    jk_rect r = { (int)b->x, (int)b->y, b->w, b->h };
    return r;
}

float jk_body_multiplier(const jk_body *b) { return b->in_water ? 0.5f : 1.0f; }

static int query(const jk_body *b, const jk_level *lv, int screen, jk_cinfo *info)
{
    jk_rect r = jk_body_hitbox(b);
    return jk_level_query(lv, screen, &r, info);
}

static float move_towards(float cur, float target, float step)
{
    float d = target - cur;
    if (d < -step) d = -step;
    if (d > step) d = step;
    return cur + d;
}

/* Proyeccion de v sobre la perpendicular de la normal (VectorMath.MakeComposite(...).i). */
static void slide_along(float *vx, float *vy, float nx, float ny)
{
    float px = ny, py = -nx;
    float d = *vx * px + *vy * py;
    *vx = d * px;
    *vy = d * py;
}

static const jk_screen *scr(const jk_level *lv, int screen)
{
    return (screen >= 0 && screen < lv->count) ? &lv->screens[screen] : 0;
}

/* WindManager.CurrentVelocity */
static float wind_velocity(const jk_screen *s, float t)
{
    if (!s || !s->wind_on) return 0.0f;
    float a = t * 0.48124886f;
    float v = sinf(a);
    v = cosf(a) > 0.0f ? v * 2.0f + 1.0f : v * 2.0f - 1.0f;
    if (v < -1.0f) v = -1.0f;
    if (v > 1.0f) v = 1.0f;
    if (s->wind_int != 0.0f) {
        if (s->wind_dir == 0) return 0.0125f * s->wind_int;
        if (s->wind_dir == 1) return -0.0125f * s->wind_int;
        return v * (0.0125f * s->wind_int);
    }
    return v * 0.1f;
}

/* Choques extra de la arena: fuera de ella, la arena solo frena de lado y al subir. */
static int sand_x(const jk_body *b, const jk_cinfo *c) { return (c->flags & JK_C_SAND) && !b->on_sand; }
static int sand_y(const jk_body *b, const jk_cinfo *c) { return (c->flags & JK_C_SAND) && !b->on_sand && b->vy < 0.0f; }

static void clamp_to_screen(jk_body *b, const jk_screen *s)
{
    if (!s || s->can_teleport) return;
    float before = b->x;
    float lo = (0.0f - (float)b->w) / 2.0f;
    float hi = (float)(JK_SCREEN_W - b->w / 2);
    if (b->x < lo) b->x = lo;
    if (b->x > hi) b->x = hi;
    if (b->x != before) b->flags |= JK_FLAG_CAPPED_X;
}

/* HandlePlayerTeleportBehaviour: salir por un borde te lleva a la pantalla enlazada. */
static void teleport(jk_body *b, const jk_level *lv, int *screen)
{
    const jk_screen *s = scr(lv, *screen);
    if (!s || !s->can_teleport) return;
    jk_rect hb = jk_body_hitbox(b);
    int cx = hb.x + hb.w / 2, cy = hb.y + hb.h / 2;
    if (cx >= 0 && cx <= JK_SCREEN_W) return;
    int left = cx < 0;
    b->x += (float)(JK_SCREEN_W * (left ? 1 : -1));
    int target = -1;
    int two = s->tele[0] != -1 && s->tele[1] != -1;
    if (!two) target = (s->tele[0] != -1 ? s->tele[0] : s->tele[1]) - 1;
    else      target = s->tele[left ? 0 : 1] - 1;
    if (target < 0) return;
    int here = (int)((0.0f - (b->y - 360.0f)) / 360.0f);
    int shift = 360 * (target - here);
    b->y = (float)(int)b->y - (float)shift;
    cy -= shift;
    *screen = jk_camera_update(*screen, lv->count, cy, 0.0f, 1);
    b->flags |= JK_FLAG_TELEPORT;
}

static void resolve_x(jk_body *b, const jk_level *lv, int screen)
{
    jk_cinfo info, next;
    if (!query(b, lv, screen, &info) && !sand_x(b, &info)) return;
    b->x = (float)(int)b->x;
    int dir = b->vx > 0.0f ? 1 : -1;
    next = info;
    do {
        info = next;
        b->x -= (float)dir;
    } while (query(b, lv, screen, &next) || sand_x(b, &next));

    if (info.slope == JK_SLOPE_NONE) {
        b->vx *= 0.0f - JK.bounce;
        b->knocked = 1;
        if (!b->on_ground || b->vy <= 0.0f) b->flags |= JK_FLAG_BUMP;
    } else {
        b->flags |= JK_FLAG_X_SLOPE;
        if (b->on_ground && fabsf(b->vx) <= JK.walk_speed && b->vy > 0.0f)
            b->vx = 0.0f;
        else if (b->last_vx * info.nx + b->last_vy * info.ny < 0.0f)
            slide_along(&b->vx, &b->vy, info.nx, info.ny);
    }
}

static void resolve_y(jk_body *b, const jk_level *lv, int screen)
{
    jk_cinfo info, next;
    int dir = b->vy > 0.0f ? 1 : -1;
    b->ystep = dir;
    b->on_snow = 0;
    if (query(b, lv, screen, &info) || sand_y(b, &info)) {
        b->flags |= JK_FLAG_TOUCH_Y;
        b->on_ice = (info.flags & JK_C_ICE) != 0;
        b->y = (float)(int)b->y;
        next = info;
        do {
            info = next;
            b->y -= (float)dir;
        } while (query(b, lv, screen, &next) || sand_y(b, &next));

        if (dir > 0) {
            int on_slope = info.slope != JK_SLOPE_NONE;
            b->on_ground = 1;
            if (on_slope) {
                b->on_ground = 0;
                if (b->last_vx * info.nx + b->last_vy * info.ny < 0.0f) {
                    slide_along(&b->vx, &b->vy, info.nx, info.ny);
                    b->knocked = 1;
                } else {
                    on_slope = 0;
                }
            }
            if (!on_slope) {
                b->knocked = 0;
                if (info.flags & JK_C_SNOW) b->on_snow = 1;
            }
        } else {
            b->flags |= JK_FLAG_BUMP;
            if (info.slope == JK_SLOPE_NONE) {
                b->vx *= JK.bounce;
            } else if (b->last_vx * info.nx + b->last_vy * info.ny < 0.0f) {
                float vx = b->vx, vy = b->vy;
                slide_along(&vx, &vy, info.nx, info.ny);
                b->vy = vy;
                b->vx *= JK.bounce;
            }
        }
        if (info.slope == JK_SLOPE_NONE || b->vy <= 0.0f || b->on_ground)
            b->vy = 0.0f;
    } else {
        /* Quark: cayendo a tope se alinea a 8 px */
        if ((info.flags & JK_C_QUARK) && b->vy == JK.max_fall) {
            if (b->flags & JK_FLAG_CAPPED_X) b->vx = 0.0f;
            b->y = (float)((int)ceilf(b->y / 8.0f) * 8);
        }
        b->on_ground = 0;
        b->on_ice = 0;
    }
}

void jk_body_step(jk_body *b, const jk_level *lv, int *screen)
{
    jk_cinfo c;
    b->flags = 0;

    /* estado de los bloques al empezar el frame */
    query(b, lv, *screen, &c);
    b->on_sand = (c.flags & JK_C_SAND) != 0;
    b->was_in_water = b->in_water;
    b->in_water = (c.flags & JK_C_WATER) != 0;

    /* viento */
    const jk_screen *s = scr(lv, *screen);
    if (!s || !s->wind_on) b->wind_enabled = 0;
    else if (b->on_ground || b->last_screen > *screen) b->wind_enabled = 1;
    if (!b->on_snow && b->wind_enabled && s && s->wind_on && !(c.flags & JK_C_NOWIND))
        b->vx += wind_velocity(s, b->run_time);
    b->last_screen = *screen;

    b->last_vx = b->vx;
    b->last_vy = b->vy;

    /* empieza metido en un bloque: se mueve sin resolver nada */
    if (query(b, lv, *screen, 0)) {
        b->x += b->vx;
        b->y += b->vy;
        return;
    }

    float m = jk_body_multiplier(b);
    float vx = b->vx;
    if (b->on_sand) vx *= 0.25f;
    vx *= m;
    b->x += vx;
    clamp_to_screen(b, scr(lv, *screen));
    teleport(b, lv, screen);
    resolve_x(b, lv, *screen);

    /* Y: arena frena al subir; fuera de la arena, en el suelo, se baja 1 px (pegado al suelo) */
    float vy = b->vy * ((b->on_sand && b->vy <= 0.0f) ? 0.5f : 1.0f);
    if (!b->on_sand && b->on_ground && b->vy > 0.0f) b->y += 1.0f;
    vy *= m;
    b->y += vy;
    resolve_y(b, lv, *screen);

    b->vy += JK.gravity * m;
    if (b->vy > JK.max_fall) b->vy = JK.max_fall;

    /* comportamientos de bloque al final del frame */
    if (b->on_ice) b->vx = move_towards(b->vx, 0.0f, JK.ice_friction);
    query(b, lv, *screen, &c);
    b->on_sand = (c.flags & JK_C_SAND) != 0;
    if (b->on_sand) {
        if (b->vy > 0.75f) b->vy = 0.75f;
        b->knocked = 0;
    }

    /* GuardtowerSoulBugFix: rozar una pendiente de lado al caer cuenta como rebote */
    if ((b->flags & JK_FLAG_X_SLOPE) && !(b->flags & JK_FLAG_TOUCH_Y) && b->ystep > 0 && !b->knocked) {
        b->vx *= 0.0f - JK.bounce;
        b->knocked = 1;
        b->flags |= JK_FLAG_BUMP;
    }
}
