#include <stdio.h>
#include <string.h>
#include "scroll.h"
#include "assets.h"

#define MAX_SCREENS 96
#define MAX_LAYERS 6

typedef struct {
    int tex, pos, mode, cx, cy, nframes;   /* mode: 0 = las dos pasadas, 1 = fondo, 2 = frente */
    float mul, fps, frames[16];
    float timer, ftimer; int frame;
    int slot;
} layer;

typedef struct { int screen1; float speed; int n, fg; layer l[MAX_LAYERS]; } sscreen;

static sscreen screens[MAX_SCREENS];
static int nscreens, loaded = -2, gen;
static GSTEXTURE slot_tex[MAX_LAYERS];

static float rf(const unsigned char *p) { float f; memcpy(&f, p, 4); return f; }

int scroll_init(void)
{
    int n;
    unsigned char *b = assets_load("scroll.bin", &n);
    if (!b || memcmp(b, "JKSC", 4) != 0) return -1;
    int count = b[4] | b[5] << 8;
    const unsigned char *p = b + 6;
    for (int i = 0; i < count && nscreens < MAX_SCREENS; i++) {
        sscreen *s = &screens[nscreens++];
        s->screen1 = p[0] | p[1] << 8; s->speed = rf(p + 2); int nl = p[6]; s->fg = p[7]; p += 8;
        s->n = 0;
        for (int k = 0; k < nl; k++) {
            layer L; memset(&L, 0, sizeof L);
            L.tex = p[0] | p[1] << 8; L.pos = (short)(p[2] | p[3] << 8); L.mul = rf(p + 4); L.mode = p[8];
            L.fps = rf(p + 9); L.cx = p[13]; L.cy = p[14]; L.nframes = p[15]; p += 16;
            for (int f = 0; f < L.nframes; f++, p += 4) if (f < 16) L.frames[f] = rf(p);
            if (L.nframes > 16) L.nframes = 16;
            L.slot = -1;
            if (s->n < MAX_LAYERS) s->l[s->n++] = L;
        }
    }
    return 0;
}

static sscreen *find(int screen)
{
    for (int i = 0; i < nscreens; i++) if (screens[i].screen1 - 1 == screen) return &screens[i];
    return NULL;
}

static void load_screen(GSGLOBAL *gs, int screen)
{
    sscreen *s = find(screen);
    if (s)
        for (int i = 0; i < s->n; i++) {
            char name[32]; snprintf(name, sizeof name, "scroll/s%d.t8", s->l[i].tex);
            memset(&slot_tex[i], 0, sizeof slot_tex[i]);
            s->l[i].slot = assets_t8_pool(gs, name, &slot_tex[i]) == 0 ? i : -1;
        }
    loaded = screen;
    gen = assets_pool_generation();
}

/* Layer.Update: avanza con dt * scroll_speed * multiplicador y da la vuelta cada 480 px */
void scroll_update(GSGLOBAL *gs, int screen, float dt)
{
    if (screen != loaded || gen != assets_pool_generation()) load_screen(gs, screen);
    sscreen *s = find(screen);
    if (!s) return;
    for (int i = 0; i < s->n; i++) {
        layer *L = &s->l[i];
        L->timer += dt * s->speed * L->mul;
        while (L->timer < 0) L->timer += 480.0f;
        while (L->timer > 480.0f) L->timer -= 480.0f;
        int cells = L->cx * L->cy, nf = L->nframes ? L->nframes : cells;
        if (L->fps > 0 || L->nframes) {
            L->ftimer += dt;
            for (int g = 0; g < 32; g++) {
                float d = L->nframes ? L->frames[L->frame % nf] : 1.0f / L->fps;
                if (L->ftimer <= d || d <= 0) break;
                L->ftimer -= d; L->frame = (L->frame + 1) % nf;
            }
        }
    }
}

void scroll_draw(GSGLOBAL *gs, int screen, int foreground, float ox, float oy, float sx, float sy)
{
    sscreen *s = find(screen);
    if (!s) return;
    for (int i = 0; i < s->n; i++) {
        layer *L = &s->l[i];
        if (L->slot < 0) continue;
        /* fondo: todo menos Foreground; frente: Foreground siempre y sin modo solo si la pantalla
         * tiene foreground (ScrollingBackground.DrawForeground) */
        if (!foreground && L->mode == 2) continue;
        if (foreground && (L->mode == 1 || (L->mode == 0 && !s->fg))) continue;
        GSTEXTURE *t = &slot_tex[L->slot];
        int cx = L->cx ? L->cx : 1, cy = L->cy ? L->cy : 1;
        int w = t->Width / cx, h = t->Height / cy;
        int idx = L->frame % (cx * cy);
        float u0 = (idx % cx) * w, v0 = (idx / cx) * h;
        /* Draw (horizontal): dos copias desplazadas 480 px, centradas si son mas estrechas */
        int num = (480 - w) / 2;
        int x1 = (int)(L->timer - 480.0f) + num, x2 = x1 + 480;
        float y = (float)(L->pos - h) + oy;
        int xs[2] = { x1 + num, x2 + num };
        for (int c = 0; c < 2; c++) {
            float x = xs[c] + ox;
            gsKit_prim_sprite_texture(gs, t, x * sx, y * sy, u0, v0, (x + w) * sx, (y + h) * sy, u0 + w, v0 + h,
                                      foreground ? 3 : 2, 0x80808080);
        }
    }
}
