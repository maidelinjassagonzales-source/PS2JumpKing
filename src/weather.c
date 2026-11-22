#include <stdio.h>
#include <string.h>
#include "weather.h"
#include "assets.h"

#define MAX_W 16
#define MAX_SC 16
#define MAX_F 8

typedef struct { int fps, fg, nf, tw, th, nsc, nmask; int sc[MAX_SC], mask[MAX_SC]; } wdef;

static wdef defs[MAX_W];
static int ndefs, loaded = -2, gen, cur = -1, cur_masked, frame;
static float timer;
static GSTEXTURE tex[MAX_F];
static int tex_ok[MAX_F];

int weather_init(void)
{
    int n;
    unsigned char *b = assets_load("weather.bin", &n);
    if (!b || memcmp(b, "JKWE", 4) != 0) return -1;
    int count = b[4];
    const unsigned char *p = b + 5;
    for (int i = 0; i < count && ndefs < MAX_W; i++) {
        wdef *d = &defs[ndefs++];
        d->fps = p[0]; d->fg = p[1]; d->nf = p[2]; d->tw = p[3] | p[4] << 8; d->th = p[5] | p[6] << 8;
        int ns = p[7], nm = p[8]; p += 9;
        d->nsc = 0; d->nmask = 0;
        for (int k = 0; k < ns; k++, p += 2) if (d->nsc < MAX_SC) d->sc[d->nsc++] = p[0] | p[1] << 8;
        for (int k = 0; k < nm; k++, p += 2) if (d->nmask < MAX_SC) d->mask[d->nmask++] = p[0] | p[1] << 8;
    }
    return 0;
}

/* WeatherManager.GetFromScreenIndex: el efecto cuya lista incluye la pantalla (1-based) */
static int find(int screen1, int *masked)
{
    for (int i = 0; i < ndefs; i++)
        for (int k = 0; k < defs[i].nsc; k++)
            if (defs[i].sc[k] == screen1) {
                *masked = 0;
                for (int m = 0; m < defs[i].nmask; m++) if (defs[i].mask[m] == screen1) *masked = 1;
                return i;
            }
    return -1;
}

static void load_screen(GSGLOBAL *gs, int screen)
{
    memset(tex_ok, 0, sizeof tex_ok);
    cur = find(screen + 1, &cur_masked);
    if (cur >= 0)
        for (int f = 0; f < defs[cur].nf && f < MAX_F; f++) {
            char name[40];
            if (cur_masked) snprintf(name, sizeof name, "weather/m%d_%d_%d.t8", cur, screen + 1, f);
            else snprintf(name, sizeof name, "weather/w%d_%d.t8", cur, f);
            memset(&tex[f], 0, sizeof tex[f]);
            tex_ok[f] = assets_t8_pool(gs, name, &tex[f]) == 0;
        }
    loaded = screen;
    gen = assets_pool_generation();
}

void weather_update(GSGLOBAL *gs, int screen, float dt)
{
    if (screen != loaded || gen != assets_pool_generation()) load_screen(gs, screen);
    if (cur < 0 || defs[cur].fps <= 0) return;
    timer += dt;                                             /* TimerIndex(1/fps) */
    while (timer >= 1.0f / defs[cur].fps) { timer -= 1.0f / defs[cur].fps; frame = (frame + 1) % defs[cur].nf; }
}

/* LevelScreen.DrawWeather: en la pasada de frente si foreground, si no en la de fondo */
void weather_draw(GSGLOBAL *gs, int screen, int foreground, float sx, float sy)
{
    if (cur < 0 || screen != loaded || defs[cur].fg != foreground) return;
    int f = frame % defs[cur].nf;
    if (f >= MAX_F || !tex_ok[f]) return;
    GSTEXTURE *t = &tex[f];
    if (cur_masked) {
        gsKit_prim_sprite_texture(gs, t, 0, 0, 0, 0, 480 * sx, 360 * sy, t->Width, t->Height, 3, 0x80808080);
        return;
    }
    for (int y = 0; y < 360; y += t->Height)
        for (int x = 0; x < 480; x += t->Width) {
            float tw = (float)t->Width, th = (float)t->Height;
            float w = x + tw > 480 ? 480 - x : tw, h = y + th > 360 ? 360 - y : th;
            gsKit_prim_sprite_texture(gs, t, x * sx, y * sy, 0, 0, (x + w) * sx, (y + h) * sy, w, h, 3, 0x80808080);
        }
}
