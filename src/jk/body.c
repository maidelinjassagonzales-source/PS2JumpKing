#include "body.h"
#include "values.h"

void jk_body_init(jk_body *b, float x, float y)
{
    *b = (jk_body){0};
    b->x = x; b->y = y;
    b->w = JK_KING_W; b->h = JK_KING_H;
    b->on_ground = 1;
}

/* La caja se toma truncando la posicion hacia cero (conversion float->int). */
jk_rect jk_body_hitbox(const jk_body *b)
{
    jk_rect r = { (int)b->x, (int)b->y, b->w, b->h };
    return r;
}

static int hits(const jk_body *b, const jk_level *lv, int screen)
{
    jk_rect r = jk_body_hitbox(b);
    return jk_level_collides(lv, screen, &r);
}

static void clamp_to_screen(jk_body *b, const jk_level *lv, int screen)
{
    if (screen < 0 || screen >= lv->count || lv->screens[screen].can_teleport)
        return;
    float before = b->x;
    float lo = (0.0f - (float)b->w) / 2.0f;
    float hi = (float)(JK_SCREEN_W - b->w / 2);
    if (b->x < lo) b->x = lo;
    if (b->x > hi) b->x = hi;
    if (b->x != before)
        b->flags |= JK_FLAG_CAPPED_X;
}

/* Choque horizontal: alinear a pixel y retroceder de a 1 px hasta salir; rebote contra la pared. */
static void resolve_x(jk_body *b, const jk_level *lv, int screen)
{
    if (!hits(b, lv, screen))
        return;
    b->x = (float)(int)b->x;
    int dir = b->vx > 0.0f ? 1 : -1;
    do b->x -= (float)dir; while (hits(b, lv, screen));

    b->vx *= 0.0f - JK.bounce;
    b->knocked = 1;
    if (!b->on_ground || b->vy <= 0.0f)
        b->flags |= JK_FLAG_BUMP;
}

/* Choque vertical: aterrizar (abajo) o golpear el techo (arriba). */
static void resolve_y(jk_body *b, const jk_level *lv, int screen)
{
    int dir = b->vy > 0.0f ? 1 : -1;
    if (!hits(b, lv, screen)) {
        b->on_ground = 0;
        return;
    }
    b->flags |= JK_FLAG_TOUCH_Y;
    b->y = (float)(int)b->y;
    do b->y -= (float)dir; while (hits(b, lv, screen));

    if (dir > 0) {
        b->on_ground = 1;
        b->knocked = 0;
    } else {
        b->flags |= JK_FLAG_BUMP;
        b->vx *= JK.bounce;
    }
    b->vy = 0.0f;
}

void jk_body_step(jk_body *b, const jk_level *lv, int screen)
{
    b->flags = 0;
    b->last_vx = b->vx;
    b->last_vy = b->vy;

    /* Si ya empieza el frame metido en un bloque, se mueve sin resolver nada ni aplicar gravedad. */
    if (hits(b, lv, screen)) {
        b->x += b->vx;
        b->y += b->vy;
        return;
    }

    b->x += b->vx;
    clamp_to_screen(b, lv, screen);
    resolve_x(b, lv, screen);

    /* Pegado al suelo: en el suelo y con vy > 0 se baja 1 px antes de mover, asi la caja vuelve
     * a tocar el suelo y on_ground no parpadea. En el original vive en el comportamiento de la
     * arena y corre siempre que el rey no este sobre arena. */
    if (b->on_ground && b->vy > 0.0f)
        b->y += 1.0f;
    b->y += b->vy;
    resolve_y(b, lv, screen);

    b->vy += JK.gravity;
    if (b->vy > JK.max_fall)
        b->vy = JK.max_fall;
}
