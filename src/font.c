#include <stdio.h>
#include <string.h>
#include "font.h"
#include "assets.h"

int font_load(GSGLOBAL *gs, jk_font *f, const char *name)
{
    char path[48];
    int n;
    memset(f, 0, sizeof *f);
    snprintf(path, sizeof path, "%s.bin", name);
    unsigned char *b = assets_load(path, &n);
    if (!b || memcmp(b, "JKFN", 4) != 0) return -1;
    unsigned char *p = b + 4;
    f->line = (short)(p[0] | p[1] << 8); p += 2;
    memcpy(&f->spacing, p, 4); p += 4;
    for (int i = 0; i < 224; i++) {
        for (int j = 0; j < 4; j++, p += 2) f->glyph[i].g[j] = (short)(p[0] | p[1] << 8);
        for (int j = 0; j < 4; j++, p += 2) f->glyph[i].c[j] = (short)(p[0] | p[1] << 8);
        for (int j = 0; j < 3; j++, p += 4) memcpy(&f->glyph[i].k[j], p, 4);
    }
    snprintf(path, sizeof path, "%s.t8", name);
    f->ok = assets_t8(gs, path, &f->tex, 0) == 0;
    return f->ok ? 0 : -1;
}

static int gi(unsigned char c) { int i = c - 32; return (i < 0 || i >= 224) ? '?' - 32 : i; }

/* SpriteFont.MeasureString (una linea): el primer glifo no suma espaciado ni sangria negativa. */
float font_width(const jk_font *f, const char *s)
{
    float x = 0, w = 0;
    int first = 1;
    for (; *s; s++) {
        const float *k = f->glyph[gi((unsigned char)*s)].k;
        if (first) { x = k[0] > 0 ? k[0] : 0; first = 0; }
        else x += f->spacing + k[0];
        x += k[1];
        float right = x + (k[2] > 0 ? k[2] : 0);
        if (right > w) w = right;
        x += k[2];
    }
    return w;
}

void font_draw(GSGLOBAL *gs, const jk_font *f, const char *s, float x, float y, float sx, float sy, u64 color)
{
    if (!f->ok) return;
    float ox = 0;
    int first = 1;
    for (; *s; s++) {
        int i = gi((unsigned char)*s);
        const short *g = f->glyph[i].g, *c = f->glyph[i].c;
        const float *k = f->glyph[i].k;
        if (first) { ox = k[0] > 0 ? k[0] : 0; first = 0; }
        else ox += f->spacing + k[0];
        if (g[2] > 0) {
            float x0 = (x + ox + c[0]) * sx, y0 = (y + c[1]) * sy;
            gsKit_prim_sprite_texture(gs, &f->tex, x0, y0, g[0], g[1], x0 + g[2] * sx, y0 + g[3] * sy,
                                      g[0] + g[2], g[1] + g[3], 7, color);
        }
        ox += k[1] + k[2];
    }
}
