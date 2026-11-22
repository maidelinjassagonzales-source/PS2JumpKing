#include <stdio.h>
#include <string.h>
#include "walls.h"
#include "assets.h"
#include "jk/values.h"

#define MAX_WALLS 128
#define MAX_BOXES 8
#define SLOTS 3

typedef struct {
    int screen1, tex, x, y, nbox;
    jk_rect box[MAX_BOXES];
    float alpha;               /* 1 = visible, 0 = desvanecida */
    int slot;
} wall;

static wall walls[MAX_WALLS];
static int nwalls, loaded = -2;
static GSTEXTURE slot_tex[SLOTS];

static int rd_i32(const unsigned char *p) { return (int)(p[0] | p[1] << 8 | p[2] << 16 | (unsigned)p[3] << 24); }
static int rd_i16(const unsigned char *p) { return (short)(p[0] | p[1] << 8); }

int walls_init(GSGLOBAL *gs)
{
    int n;
    unsigned char *b = assets_load("walls.bin", &n);
    if (!b || memcmp(b, "JKHW", 4) != 0) return -1;
    int count = b[4] | b[5] << 8;
    const unsigned char *p = b + 6;
    for (int i = 0; i < count && nwalls < MAX_WALLS; i++) {
        wall *w = &walls[nwalls++];
        w->screen1 = p[0] | p[1] << 8;
        w->tex = p[2] | p[3] << 8;
        w->x = rd_i32(p + 4);
        w->y = rd_i32(p + 8);
        int nb = p[12];
        p += 13;
        w->nbox = nb > MAX_BOXES ? MAX_BOXES : nb;
        for (int k = 0; k < nb; k++, p += 12)
            if (k < MAX_BOXES) {
                w->box[k].x = rd_i32(p); w->box[k].y = rd_i32(p + 4);
                w->box[k].w = rd_i16(p + 8); w->box[k].h = rd_i16(p + 10);
            }
        w->alpha = 1.0f;
        w->slot = -1;
    }
    for (int s = 0; s < SLOTS; s++) {
        memset(&slot_tex[s], 0, sizeof slot_tex[s]);
        slot_tex[s].Vram = gsKit_vram_alloc(gs, gsKit_texture_size(480, 360, GS_PSM_T8), GSKIT_ALLOC_USERBUFFER);
        slot_tex[s].VramClut = gsKit_vram_alloc(gs, gsKit_texture_size(16, 16, GS_PSM_CT32), GSKIT_ALLOC_USERBUFFER);
    }
    return 0;
}

static void load_screen(GSGLOBAL *gs, int screen)
{
    int used = 0;
    for (int i = 0; i < nwalls; i++) {
        walls[i].slot = -1;
        if (walls[i].screen1 - 1 != screen || used >= SLOTS) continue;
        char name[32];
        snprintf(name, sizeof name, "walls/w%d.t8", walls[i].tex);
        if (assets_t8(gs, name, &slot_tex[used], 1) == 0 && slot_tex[used].Width <= 480 && slot_tex[used].Height <= 360)
            walls[i].slot = used++;
    }
    loaded = screen;
}

void walls_update(GSGLOBAL *gs, const jk_king *k, float dt)
{
    if (k->screen != loaded) load_screen(gs, k->screen);
    jk_rect hb = jk_body_hitbox(&k->body);
    for (int i = 0; i < nwalls; i++) {
        wall *w = &walls[i];
        if (w->screen1 - 1 != k->screen) continue;
        int touching = 0;
        for (int b = 0; b < w->nbox; b++)
            if (jk_rect_overlap(&w->box[b], &hb)) touching = 1;
        w->alpha += (touching ? -1.0f : 1.0f) * dt / 0.2f;     /* Fade(0.2 s) */
        if (w->alpha < 0) w->alpha = 0;
        if (w->alpha > 1) w->alpha = 1;
    }
}

void walls_draw(GSGLOBAL *gs, int screen, float sx, float sy)
{
    for (int i = 0; i < nwalls; i++) {
        const wall *w = &walls[i];
        if (w->screen1 - 1 != screen || w->slot < 0 || w->alpha <= 0.0f) continue;
        GSTEXTURE *t = &slot_tex[w->slot];
        float x = (float)w->x, y = (float)(w->y + screen * JK_SCREEN_H);
        int a = (int)(w->alpha * 128.0f);
        gsKit_prim_sprite_texture(gs, t, x * sx, y * sy, 0, 0, (x + t->Width) * sx, (y + t->Height) * sy,
                                  t->Width, t->Height, 3, GS_SETREG_RGBAQ(0x80, 0x80, 0x80, a, 0));
    }
}
