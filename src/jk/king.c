#include <math.h>
#include "king.h"
#include "values.h"
#include "camera.h"

/* Semantica de nodo del original: "nuevo" si no estaba corriendo, "reanudar" si corria pero
 * se salto al menos un tick. */
static void node_enter(jk_node *n, int tick, int *is_new, int *is_resume)
{
    *is_new = n->last != JK_RUNNING;
    *is_resume = n->last == JK_RUNNING && n->last_tick != tick - 1;
}
static jk_result node_leave(jk_node *n, int tick, jk_result r)
{
    n->last = r;
    n->last_tick = tick;
    return r;
}

static int dpad_x(jk_input in) { return (in.right ? 1 : 0) - (in.left ? 1 : 0); }

static void set_facing(jk_king *k, int dir) { if (dir != 0) k->facing = dir > 0 ? 1 : -1; }

/* ---- carga y salto ---- */

static void charge_reset(jk_king *k)
{
    k->charge = 0.0f;
    k->dpad_count = 0;
}

static void push_dpad(jk_king *k, int x)
{
    if (k->dpad_count < 4) k->dpad_count++;
    for (int i = k->dpad_count - 1; i > 0; i--)
        k->dpad_hist[i] = k->dpad_hist[i - 1];
    k->dpad_hist[0] = x;
}

static int try_consume_jump(jk_king *k)
{
    int c = k->can_jump;
    k->can_jump = 0;
    return c;
}

static void do_jump(jk_king *k, jk_input in, float intensity)
{
    jk_body *b = &k->body;
    int x = dpad_x(in);
    /* si se solto la direccion justo antes, vale la de los ultimos 4 frames */
    for (int i = 0; x == 0 && i < k->dpad_count; i++)
        x = k->dpad_hist[i];

    b->vy = JK.jump * intensity;
    b->vx += (float)x * JK.air_speed;
    if (b->vx < -JK.air_speed) b->vx = -JK.air_speed;
    if (b->vx >  JK.air_speed) b->vx =  JK.air_speed;
    set_facing(k, x);
    charge_reset(k);
    k->events |= JK_EV_JUMP;
}

static jk_result jump_run(jk_king *k, jk_input in)
{
    jk_body *b = &k->body;
    int is_new, is_resume;
    node_enter(&k->jump_node, k->tick, &is_new, &is_resume);
    if (is_new || is_resume)
        charge_reset(k);

    push_dpad(k, dpad_x(in));
    if (k->charge == 0.0f && !try_consume_jump(k))
        return node_leave(&k->jump_node, k->tick, JK_FAILURE);
    if (k->charge == 0.0f && in.jump)
        b->vx = 0.0f;
    if (b->on_ground)
        b->vx = 0.0f;

    k->charge += JK_DT * 1.0f;          /* multiplicador de agua: pendiente */
    if (k->charge >= JK.charge_time || !in.jump) {
        float t = k->charge / JK.charge_time;
        do_jump(k, in, t < 1.0f ? t : 1.0f);
        return node_leave(&k->jump_node, k->tick, JK_SUCCESS);
    }
    return node_leave(&k->jump_node, k->tick, JK_RUNNING);
}

/* ---- andar ---- */

static jk_result walk_run(jk_king *k, jk_input in)
{
    int x = dpad_x(in);
    k->body.vx = (float)x * JK.walk_speed;
    set_facing(k, x);
    return x == 0 ? JK_FAILURE : JK_SUCCESS;
}

/* ---- caida fallida: aterrizar a velocidad maxima deja al rey tirado ---- */

static void splat_reset(jk_king *k)
{
    k->splat_timer = 0.0f;
    k->splat_done = 0;
    k->splat_eval = 1;
}

static jk_result fail_run(jk_king *k, jk_input in)
{
    jk_body *b = &k->body;
    int is_new, is_resume;
    node_enter(&k->fail_node, k->tick, &is_new, &is_resume);
    if (is_new)
        splat_reset(k);
    if (is_resume) {
        b->vx = 0.0f;
        k->splat_timer = 0.0f;
    }

    if (k->splat_eval) {
        k->splat_eval = 0;
        if (b->last_vy != JK.max_fall) {
            k->splatted = 0;
            return node_leave(&k->fail_node, k->tick, JK_FAILURE);
        }
        b->vx = 0.0f;
        k->splatted = 1;
        k->events |= JK_EV_SPLAT;
    }
    b->vx = 0.0f;
    if (!k->splat_done && b->on_ground) {
        k->splat_timer += JK_DT;
        if (k->splat_timer >= JK.splat_time) {
            while (k->splat_timer >= JK.splat_time) k->splat_timer -= JK.splat_time;
            k->splat_done = 1;
        }
    }
    if (k->splat_done && (in.left || in.right || in.jump)) {
        splat_reset(k);
        k->splatted = 0;
        return node_leave(&k->fail_node, k->tick, JK_SUCCESS);
    }
    return node_leave(&k->fail_node, k->tick, JK_RUNNING);
}

/* ---- margen en el aire ---- */

static jk_result grace_run(jk_king *k)
{
    int is_new, is_resume;
    node_enter(&k->grace_node, k->tick, &is_new, &is_resume);
    if (is_new)
        k->grace_timer = 0.0f;
    k->grace_timer += JK_DT;
    if (k->grace_timer >= JK_LENIENCY_TIME) {
        while (k->grace_timer >= JK_LENIENCY_TIME) k->grace_timer -= JK_LENIENCY_TIME;
        return node_leave(&k->grace_node, k->tick, JK_SUCCESS);
    }
    return node_leave(&k->grace_node, k->tick, JK_RUNNING);
}

void jk_king_init(jk_king *k, float x, float y)
{
    *k = (jk_king){0};
    jk_body_init(&k->body, x, y);
    k->facing = 1;
    k->can_jump = 1;
    k->jump_node.last = k->fail_node.last = k->grace_node.last = JK_NONE;
}

void jk_king_update(jk_king *k, const jk_level *lv, jk_input in)
{
    /* 1. fisica */
    int was_ground = k->was_ground;
    k->events = 0;
    jk_body_step(&k->body, lv, k->screen);
    if (k->body.flags & JK_FLAG_BUMP) k->events |= JK_EV_BUMP;

    /* 2. entrada: pulsar salto habilita un salto, soltarlo lo cancela */
    if (in.jump && !k->prev_jump) k->can_jump = 1;
    if (!in.jump) k->can_jump = 0;
    k->prev_jump = in.jump;

    /* 3. decisiones */
    k->tick++;
    if (k->body.on_ground) {
        /* IsOnGround: sonido de aterrizar al pasar a suelo (el splat tiene el suyo) */
        if (!was_ground && k->body.last_vy != JK.max_fall) k->events |= JK_EV_LAND;
        if (fail_run(k, in) == JK_FAILURE)
            if (jump_run(k, in) == JK_FAILURE)
                walk_run(k, in);
    } else if (k->jump_node.last == JK_RUNNING) {
        grace_run(k);
        jump_run(k, in);
    }

    k->was_ground = k->body.on_ground;

    /* 4. camara (va despues del arbol, como en el original) */
    jk_rect hb = jk_body_hitbox(&k->body);
    k->screen = jk_camera_update(k->screen, lv->count, hb.y + hb.h / 2, k->body.vy, 0);
}
