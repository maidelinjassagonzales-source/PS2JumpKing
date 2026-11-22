/* SpriteFont convertida (font.bin + font.t8): medir y dibujar como SpriteBatch.DrawString de MonoGame. */
#ifndef FONT_H
#define FONT_H
#include <gsKit.h>

typedef struct {
    GSTEXTURE tex;
    short line;
    float spacing;
    struct { short g[4], c[4]; float k[3]; } glyph[224];   /* Latin-1 32..255 */
    int ok;
} jk_font;

int   font_load(GSGLOBAL *gs, jk_font *f, const char *name);   /* "menufont" -> menufont.bin/.t8 */
float font_width(const jk_font *f, const char *s);
void  font_draw(GSGLOBAL *gs, const jk_font *f, const char *s, float x, float y, float sx, float sy, u64 color);
#endif
