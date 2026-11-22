#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "props.h"
#include "assets.h"
#include "jk/values.h"

#define MAX_TYPES 32
#define MAX_PROPS 400
#define MAX_BIRDS 8
#define MAX_BPOS 64
#define SLOTS 8
#define SLOT_BYTES (160 * 1024)

typedef struct { float fps; int cx, cy, rnd, nframes; float frames[32]; } ptype;
typedef struct { int screen1, type, x, y, flip; float timer; int frame; } prop;
typedef struct { int screen1, x, y, treasure, look_right, fly_right; float trigger; } bpos;
typedef struct {
    int tex, is_fly, sfx, npos;
    bpos pos[MAX_BPOS];
    int home, done;                 /* RavenState: posicion actual, ya no vuelve */
    int fleeing; float px, py, vx, vy; int tick, frame; float idle_t;
    int slot;
} bird;

static ptype types[MAX_TYPES];
static prop props[MAX_PROPS];
static bird birds[MAX_BIRDS];
static int ntypes, nprops, nbirds, loaded = -2, pending_sfx = -1, gen;
static GSTEXTURE slot_tex[SLOTS];
static int slot_of_type[MAX_TYPES], bird_slot[MAX_BIRDS];

static int rdi16(const unsigned char *p) { return (short)(p[0] | p[1] << 8); }
static int rdu16(const unsigned char *p) { return p[0] | p[1] << 8; }
static float rdf32(const unsigned char *p) { float f; memcpy(&f, p, 4); return f; }

int props_init(GSGLOBAL *gs)
{
    int n;
    unsigned char *b = assets_load("props.bin", &n);
    if (b && memcmp(b, "JKPR", 4) == 0) {
        ntypes = rdu16(b + 4); int np = rdu16(b + 6);
        const unsigned char *p = b + 8;
        for (int i = 0; i < ntypes && i < MAX_TYPES; i++) {
            ptype *t = &types[i];
            t->fps = rdf32(p); t->cx = p[4]; t->cy = p[5]; t->rnd = p[6]; t->nframes = p[7]; p += 8;
            for (int k = 0; k < t->nframes; k++, p += 4) t->frames[k] = rdf32(p);
        }
        for (int i = 0; i < np && nprops < MAX_PROPS; i++, p += 9) {
            prop *pr = &props[nprops++];
            pr->screen1 = rdu16(p); pr->type = rdu16(p + 2); pr->x = rdi16(p + 4); pr->y = rdi16(p + 6); pr->flip = p[8];
            pr->timer = types[pr->type].rnd ? (float)(rand() % 10000) / 100.0f : 0.0f;
        }
    }
    b = assets_load("birds.bin", &n);
    if (b && memcmp(b, "JKBD", 4) == 0) {
        int count = rdu16(b + 4);
        const unsigned char *p = b + 6;
        for (int i = 0; i < count && nbirds < MAX_BIRDS; i++) {
            bird *bd = &birds[nbirds++];
            memset(bd, 0, sizeof *bd);
            bd->tex = p[0]; bd->is_fly = p[1]; bd->sfx = (signed char)p[2]; bd->npos = p[3]; p += 4;
            for (int k = 0; k < bd->npos; k++, p += 13)
                if (k < MAX_BPOS) {
                    bpos *q = &bd->pos[k];
                    q->screen1 = rdu16(p); q->x = rdi16(p + 2); q->y = rdi16(p + 4);
                    q->treasure = p[6]; q->look_right = p[7]; q->fly_right = p[8]; q->trigger = rdf32(p + 9);
                }
            if (bd->npos > MAX_BPOS) bd->npos = MAX_BPOS;
        }
    }
    for (int s = 0; s < SLOTS; s++) memset(&slot_tex[s], 0, sizeof slot_tex[s]);
    return 0;
}

static void load_screen(GSGLOBAL *gs, int screen)
{
    int used = 0;
    for (int t = 0; t < MAX_TYPES; t++) slot_of_type[t] = -1;
    for (int i = 0; i < nprops && used < SLOTS; i++) {
        prop *p = &props[i];
        if (p->screen1 - 1 != screen || slot_of_type[p->type] >= 0) continue;
        char name[32]; snprintf(name, sizeof name, "props/p%d.t8", p->type);
        if (assets_t8_pool(gs, name, &slot_tex[used]) == 0) slot_of_type[p->type] = used++;
    }
    for (int i = 0; i < nbirds; i++) {
        bird_slot[i] = -1;
        bird *b = &birds[i];
        if (b->done || b->home >= b->npos || b->pos[b->home].screen1 - 1 != screen || used >= SLOTS) continue;
        char name[32]; snprintf(name, sizeof name, "props/bird%d.t8", b->tex);
        if (assets_t8_pool(gs, name, &slot_tex[used]) == 0) bird_slot[i] = used++;
    }
    loaded = screen;
    gen = assets_pool_generation();
}

static float bird_real_y(const bpos *q) { return (float)(q->y - (q->screen1 - 1) * JK_SCREEN_H); }

/* RavenComp.GetFlyDirection: hacia abajo si la siguiente posicion esta en una pantalla mas baja */
static int fly_dir_vertical(const bird *b)
{
    if (b->home >= b->npos - 1) return -1;
    return b->pos[b->home].screen1 > b->pos[b->home + 1].screen1 ? 1 : -1;
}

void props_update(GSGLOBAL *gs, const jk_king *k, float dt)
{
    if (k->screen != loaded || gen != assets_pool_generation()) load_screen(gs, k->screen);
    for (int i = 0; i < nprops; i++) {
        prop *p = &props[i];
        if (p->screen1 - 1 != k->screen) continue;
        const ptype *t = &types[p->type];
        int cells = t->cx * t->cy;
        int nf = t->nframes ? t->nframes : cells;
        p->timer += dt;
        for (int guard = 0; guard < 64; guard++) {
            float d = t->nframes ? t->frames[p->frame % nf] : 1.0f / t->fps;
            if (p->timer <= d || d <= 0) break;
            p->timer -= d;
            p->frame = (p->frame + 1) % nf;
        }
    }
    jk_rect hb = jk_body_hitbox(&k->body);
    float kcx = hb.x + hb.w / 2, kcy = hb.y + hb.h / 2;
    for (int i = 0; i < nbirds; i++) {
        bird *b = &birds[i];
        if (b->done || b->home >= b->npos) continue;
        const bpos *q = &b->pos[b->home];
        if (q->screen1 - 1 != k->screen) { b->fleeing = 0; continue; }
        if (!b->fleeing) {
            b->px = q->x; b->py = bird_real_y(q);
            b->idle_t -= dt;
            if (b->idle_t <= 0) { b->frame = rand() % 3; b->idle_t = 0.4f + (rand() % 100) / 100.0f; }
            float dx = kcx - b->px, dy = kcy - b->py;
            if (dx * dx + dy * dy < q->trigger * q->trigger) {   /* IsPlayerClose */
                b->fleeing = 1; b->tick = 0; b->frame = 0;
                int up = fly_dir_vertical(b) < 0, fx = q->fly_right ? 1 : -1;
                b->vx = 0; b->vy = 2.0f * fly_dir_vertical(b);
                if (!up) { b->vx = fx * 3.5f; b->vy = 6.0f; }
                pending_sfx = b->sfx;
            }
            continue;
        }
        /* RavenFlee: cada 2 ticks acelera; mueve v/2 por frame hasta salir de la pantalla */
        int fx = q->fly_right ? 1 : -1;
        if (++b->tick >= 2) {
            b->tick = 0;
            if (fly_dir_vertical(b) < 0) { b->vx += fx * 0.5f; b->vy *= 1.15f; }
            else b->vx += fx * 0.3f;
            b->frame = (b->frame + 1) % 3;
        }
        b->px += b->vx / 2.0f; b->py += b->vy / 2.0f;
        float sy = b->py + k->screen * JK_SCREEN_H;
        if (b->px < -48 || b->px > JK_SCREEN_W + 48 || sy < -48 || sy > JK_SCREEN_H + 48) {
            b->fleeing = 0;                                      /* OnFlee: siguiente posicion */
            if (++b->home >= b->npos) { b->home = b->npos - 1; if (b->is_fly) b->done = 1; }
            if (b->pos[b->home].screen1 - 1 != k->screen) bird_slot[i] = -1;   /* su textura llega con esa pantalla */
        }
    }
}

int props_take_bird_sfx(void) { int s = pending_sfx; pending_sfx = -1; return s; }

void props_draw(GSGLOBAL *gs, int screen, float sx, float sy)
{
    for (int i = 0; i < nprops; i++) {
        const prop *p = &props[i];
        if (p->screen1 - 1 != screen || slot_of_type[p->type] < 0) continue;
        GSTEXTURE *t = &slot_tex[slot_of_type[p->type]];
        const ptype *ty = &types[p->type];
        int cw = t->Width / ty->cx, ch = t->Height / ty->cy, cells = ty->cx * ty->cy;
        int idx = p->frame % cells;
        float u0 = (idx % ty->cx) * cw, v0 = (idx / ty->cx) * ch, u1 = u0 + cw;
        if (p->flip) { float tmp = u0; u0 = u1; u1 = tmp; }
        float x = p->x, y = p->y;                                   /* LoopingProp: esquina superior */
        gsKit_prim_sprite_texture(gs, t, x * sx, y * sy, u0, v0, (x + cw) * sx, (y + ch) * sy, u1, v0 + ch, 2, 0x80808080);
    }
    for (int i = 0; i < nbirds; i++) {
        const bird *b = &birds[i];
        if (bird_slot[i] < 0 || b->done || b->home >= b->npos || b->pos[b->home].screen1 - 1 != screen) continue;
        GSTEXTURE *t = &slot_tex[bird_slot[i]];
        const bpos *q = &b->pos[b->home];
        float u0, v0, w, h;
        if (b->fleeing) { w = 48; h = 32; u0 = (b->frame % 3) * 48; v0 = 64; }               /* Fly */
        else { static const int idle[3] = { 0, 2, 3 }; w = 32; h = 32; u0 = idle[b->frame % 3] * 32; v0 = 0; }
        int right = b->fleeing ? q->fly_right : q->look_right;
        float u1 = u0 + w;
        if (!right) { float tmp = u0; u0 = u1; u1 = tmp; }
        float x = b->px, y = b->py + screen * JK_SCREEN_H;
        gsKit_prim_sprite_texture(gs, t, x * sx, y * sy, u0, v0, (x + w) * sx, (y + h) * sy, u1, v0 + h, 3, 0x80808080);
    }
}

int props_save(unsigned char *out, int max)
{
    int p = 0;
    if (max < 1 + nbirds * 2) return 0;
    out[p++] = (unsigned char)nbirds;
    for (int i = 0; i < nbirds; i++) { out[p++] = (unsigned char)birds[i].home; out[p++] = (unsigned char)birds[i].done; }
    return p;
}

void props_load(const unsigned char *in, int size)
{
    if (size < 1) return;
    int n = in[0];
    for (int i = 0; i < n && i < nbirds && 2 + i * 2 < size + 1; i++) {
        birds[i].home = in[1 + i * 2]; birds[i].done = in[2 + i * 2]; birds[i].fleeing = 0;
    }
    assets_pool_reset();
}

void props_reset(void)
{
    for (int i = 0; i < nbirds; i++) { birds[i].home = 0; birds[i].done = 0; birds[i].fleeing = 0; }
    assets_pool_reset();
}
