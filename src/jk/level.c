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

int jk_level_collides(const jk_level *lv, int screen, const jk_rect *box)
{
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
const jk_level jk_test_level = { screens, 2 };
