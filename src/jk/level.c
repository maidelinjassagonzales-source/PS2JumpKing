#include <stddef.h>
#include "level.h"

int jk_rect_overlap(const jk_rect *a, const jk_rect *b)
{
    return a->x < b->x + b->w && b->x < a->x + a->w &&
           a->y < b->y + b->h && b->y < a->y + a->h;
}

static int screen_hits(const jk_screen *s, const jk_rect *box)
{
    for (int i = 0; i < s->count; i++)
        if (jk_rect_overlap(&s->blocks[i], box))
            return 1;
    return 0;
}

/* Bloques que paran al rey (BoxBlock.canBlockPlayer): todos menos arena, agua y "sin viento".
 * Las pendientes se tratan como cajas hasta portar SlopeBlock. */
static int tile_blocks(unsigned char t)
{
    return t == JK_B_SOLID || t == JK_B_SLOPE || t == JK_B_ICE || t == JK_B_SNOW || t == JK_B_QUARK;
}

static int floor_div(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

/* Mismo resultado que recorrer las cajas de 8x8 de las pantallas screen-1..screen+1. */
static int grid_collides(const jk_level *lv, int screen, const jk_rect *box)
{
    int tx0 = floor_div(box->x, 8), tx1 = floor_div(box->x + box->w - 1, 8);
    int ty0 = floor_div(box->y, 8), ty1 = floor_div(box->y + box->h - 1, 8);
    if (tx0 < 0) tx0 = 0;
    if (tx1 >= JK_TILE_W) tx1 = JK_TILE_W - 1;
    for (int ty = ty0; ty <= ty1; ty++) {
        int s = -floor_div(ty, JK_TILE_H);          /* fila ty del mundo -> pantalla */
        if (s < screen - 1 || s > screen + 1 || s < 0 || s >= lv->count)
            continue;
        const unsigned char *row = lv->grid + ((size_t)s * JK_TILE_H + (ty + s * JK_TILE_H)) * JK_TILE_W;
        for (int tx = tx0; tx <= tx1; tx++)
            if (tile_blocks(row[tx]))
                return 1;
    }
    return 0;
}

int jk_level_collides(const jk_level *lv, int screen, const jk_rect *box)
{
    if (lv->grid)
        return grid_collides(lv, screen, box);
    for (int i = screen - 1; i <= screen + 1; i++)
        if (i >= 0 && i < lv->count && screen_hits(&lv->screens[i], box))
            return 1;
    return 0;
}

/* Nivel de prueba propio (no es del juego): suelo, paredes y plataformas para medir saltos.
 * Coordenadas del mundo; la pantalla 1 esta encima de la 0. */
static const jk_rect s0[] = {
    {   0, 344, 480, 16 },          /* suelo */
    {   0,   0,   8, 360 },         /* pared izquierda */
    { 472,   0,   8, 360 },         /* pared derecha */
    { 300, 296, 100,  48 },         /* escalon de 48 px */
    {  40, 220,  80,  12 },         /* plataforma media */
    { 330, 150,  90,  12 },         /* plataforma alta */
    { 180,  60, 120,  12 },         /* plataforma cerca del techo */
};
static const jk_rect s1[] = {
    {   0, -360,   8, 360 },
    { 472, -360,   8, 360 },
    {  20,  -90, 100,  12 },
    { 360, -230, 100,  12 },        /* desde aqui se cae hasta el suelo: prueba del splat */
};
static const jk_screen screens[] = {
    { s0, sizeof s0 / sizeof s0[0], 0 },
    { s1, sizeof s1 / sizeof s1[0], 0 },
};
const jk_level jk_test_level = { screens, 2, 0 };

/* Pantallas sin limite de X para el nivel real (falta leer el teletransporte del mapa). */
static const jk_screen real_screen = { 0, 0, 0 };
static jk_screen real_screens[256];

int jk_level_from_grid(jk_level *lv, const unsigned char *grid, int count)
{
    if (count > 256) return -1;
    for (int i = 0; i < count; i++) real_screens[i] = real_screen;
    lv->screens = real_screens;
    lv->count = count;
    lv->grid = grid;
    return 0;
}
