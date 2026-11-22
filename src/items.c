#include <stdio.h>
#include <string.h>
#include "items.h"
#include "assets.h"
#include "jk/values.h"

#define MAX_WORLD 64

typedef struct { int item, screen1, x, y, taken; } witem;

static char names[ITEM_KINDS][64];
static int has_sprite[ITEM_KINDS];
static int inventory[ITEM_KINDS];
static witem defaults[MAX_WORLD], world[MAX_WORLD];
static int ndefaults, nworld, loaded = -2, gen, pickup;
static GSTEXTURE tex[ITEM_KINDS];
static int tex_ok[ITEM_KINDS];

int items_init(GSGLOBAL *gs)
{
    int n;
    unsigned char *b = assets_load("items.bin", &n);
    if (!b || memcmp(b, "JKIT", 4) != 0) return -1;
    int kinds = b[4], count = b[5] | b[6] << 8;
    const unsigned char *p = b + 7;
    for (int i = 0; i < kinds && i < ITEM_KINDS; i++) {
        has_sprite[i] = p[0];
        int l = p[1];
        memcpy(names[i], p + 2, l); names[i][l] = 0;
        p += 2 + l;
    }
    for (int i = 0; i < count && ndefaults < MAX_WORLD; i++, p += 7) {
        witem *w = &defaults[ndefaults++];
        w->item = p[0]; w->screen1 = p[1] | p[2] << 8;
        w->x = (short)(p[3] | p[4] << 8); w->y = (short)(p[5] | p[6] << 8);
        w->taken = 0;
    }
    items_reset();
    (void)gs;
    return 0;
}

void items_reset(void)
{
    memcpy(world, defaults, sizeof world);
    nworld = ndefaults;
    memset(inventory, 0, sizeof inventory);
    loaded = -2;
}

void items_spawn(int item, int screen1, int x, int y)
{
    if (nworld >= MAX_WORLD || item < 0 || item >= ITEM_KINDS) return;
    world[nworld++] = (witem){ item, screen1, x, y, 0 };
    loaded = -2;
}

static void load_screen(GSGLOBAL *gs, int screen)
{
    memset(tex_ok, 0, sizeof tex_ok);
    for (int i = 0; i < nworld; i++) {
        const witem *w = &world[i];
        if (w->taken || w->screen1 - 1 != screen || tex_ok[w->item] || !has_sprite[w->item]) continue;
        char name[32]; snprintf(name, sizeof name, "items/i%d.t8", w->item);
        memset(&tex[w->item], 0, sizeof tex[w->item]);
        tex_ok[w->item] = assets_t8_pool(gs, name, &tex[w->item]) == 0;
    }
    loaded = screen;
    gen = assets_pool_generation();
}

/* WorldItemState.position (sube 360 px por pantalla) y sprite centrado */
static void item_rect(const witem *w, jk_rect *r)
{
    int iw = tex_ok[w->item] ? tex[w->item].Width : 16, ih = tex_ok[w->item] ? tex[w->item].Height : 16;
    int py = w->y - (w->screen1 - 1) * JK_SCREEN_H;
    r->x = w->x - iw / 2; r->y = py - ih / 2; r->w = iw; r->h = ih;
}

void items_update(GSGLOBAL *gs, const jk_king *k)
{
    if (k->screen != loaded || gen != assets_pool_generation()) load_screen(gs, k->screen);
    jk_rect hb = jk_body_hitbox(&k->body);
    for (int i = 0; i < nworld; i++) {
        witem *w = &world[i];
        if (w->taken || w->screen1 - 1 != k->screen) continue;
        jk_rect r; item_rect(w, &r);
        if (jk_rect_overlap(&r, &hb)) {           /* WorldItemComp: tocarlo lo recoge */
            w->taken = 1;
            inventory[w->item]++;
            pickup = 1;
        }
    }
}

int items_take_pickup(void) { int p = pickup; pickup = 0; return p; }

void items_draw(GSGLOBAL *gs, int screen, float sx, float sy)
{
    for (int i = 0; i < nworld; i++) {
        const witem *w = &world[i];
        if (w->taken || w->screen1 - 1 != screen || !tex_ok[w->item]) continue;
        jk_rect r; item_rect(w, &r);
        GSTEXTURE *t = &tex[w->item];
        float x = r.x, y = r.y + screen * JK_SCREEN_H;
        gsKit_prim_sprite_texture(gs, t, x * sx, y * sy, 0, 0, (x + t->Width) * sx, (y + t->Height) * sy,
                                  t->Width, t->Height, 2, 0x80808080);
    }
}

int items_count(int item) { return (item >= 0 && item < ITEM_KINDS) ? inventory[item] : 0; }
const char *items_name(int item) { return (item >= 0 && item < ITEM_KINDS) ? names[item] : ""; }

/* guardado: inventario + objetos del mundo (recogidos o soltados por pajaros) */
int items_save(unsigned char *out, int max)
{
    int need = ITEM_KINDS + 1 + nworld * 8;
    if (max < need) return 0;
    int p = 0;
    for (int i = 0; i < ITEM_KINDS; i++) out[p++] = (unsigned char)inventory[i];
    out[p++] = (unsigned char)nworld;
    for (int i = 0; i < nworld; i++) {
        const witem *w = &world[i];
        out[p++] = (unsigned char)w->item; out[p++] = (unsigned char)w->taken;
        out[p++] = w->screen1 & 255; out[p++] = w->screen1 >> 8;
        out[p++] = w->x & 255; out[p++] = (w->x >> 8) & 255;
        out[p++] = w->y & 255; out[p++] = (w->y >> 8) & 255;
    }
    return p;
}

void items_load(const unsigned char *in, int size)
{
    if (size < ITEM_KINDS + 1) return;
    int p = 0;
    for (int i = 0; i < ITEM_KINDS; i++) inventory[i] = in[p++];
    int n = in[p++];
    nworld = 0;
    for (int i = 0; i < n && nworld < MAX_WORLD && p + 8 <= size; i++, p += 8) {
        witem *w = &world[nworld++];
        w->item = in[p]; w->taken = in[p + 1];
        w->screen1 = in[p + 2] | in[p + 3] << 8;
        w->x = (short)(in[p + 4] | in[p + 5] << 8);
        w->y = (short)(in[p + 6] | in[p + 7] << 8);
    }
    loaded = -2;
}
