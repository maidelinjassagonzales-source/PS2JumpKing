#include <stddef.h>
#include <string.h>
#include <math.h>
#include "level.h"

int jk_rect_overlap(const jk_rect *a, const jk_rect *b)
{
    return a->x < b->x + b->w && b->x < a->x + a->w &&
           a->y < b->y + b->h && b->y < a->y + a->h;
}

/* Rectangle.Contains(Point): borde izquierdo/superior incluido, derecho/inferior no. */
static int contains(const jk_rect *r, int px, int py)
{
    return r->x <= px && px < r->x + r->w && r->y <= py && py < r->y + r->h;
}

/* ---- geometria de pendientes (SlopeBlock + LineUtil) ---- */

typedef struct { int x0, y0, x1, y1; } seg;

static int in_span(float a, float b, float p)
{
    if (a > b) { float t = a; a = b; b = t; }
    return p > a && p < b;
}

/* Recta y = k x + m. La pendiente se calcula con division entera, como en el original. */
static void line_km(seg l, int *vertical, float *k, float *m)
{
    if (l.x0 > l.x1) { seg t = { l.x1, l.y1, l.x0, l.y0 }; l = t; }
    if (l.x0 == l.x1) { *vertical = 1; *k = 0; *m = 0; return; }
    *vertical = 0;
    *k = (float)((l.y1 - l.y0) / (l.x1 - l.x0));
    *m = (float)l.y0 - *k * (float)l.x0;
}

static int segs_cross(seg a, seg b)
{
    int va, vb; float ka, ma, kb, mb;
    line_km(a, &va, &ka, &ma);
    line_km(b, &vb, &kb, &mb);
    if (va && vb) return 0;
    if (va || vb) {
        seg n = va ? b : a, v = va ? a : b;
        float kn = va ? kb : ka, mn = va ? mb : ma;
        if (!in_span((float)n.x0, (float)n.x1, (float)v.x0)) return 0;
        return in_span((float)v.y0, (float)v.y1, kn * (float)v.x0 + mn);
    }
    float x = (mb - ma) / (ka - kb);
    return in_span((float)a.x0, (float)a.x1, x) && in_span((float)b.x0, (float)b.x1, x);
}

static int slope_hits(int type, const jk_rect *t, const jk_rect *h)
{
    if (!jk_rect_overlap(t, h)) return 0;
    int x = t->x, y = t->y, w = t->w, hh = t->h;
    int p[3][2];
    switch (type) {
    case JK_SLOPE_TL: p[0][0] = x + w; p[0][1] = y; p[1][0] = x + w; p[1][1] = y + hh; p[2][0] = x; p[2][1] = y + hh; break;
    case JK_SLOPE_TR: p[0][0] = x; p[0][1] = y; p[1][0] = x + w; p[1][1] = y + hh; p[2][0] = x; p[2][1] = y + hh; break;
    /* BottomRight usa los mismos vertices que BottomLeft en el original */
    default:          p[0][0] = x; p[0][1] = y; p[1][0] = x + w; p[1][1] = y; p[2][0] = x; p[2][1] = y + hh; break;
    }
    seg tri[3] = {
        { p[0][0], p[0][1], p[1][0], p[1][1] },
        { p[1][0], p[1][1], p[2][0], p[2][1] },
        { p[2][0], p[2][1], p[0][0], p[0][1] },
    };
    seg box[4] = {
        { h->x, h->y, h->x + h->w, h->y },
        { h->x + h->w, h->y, h->x + h->w, h->y + h->h },
        { h->x + h->w, h->y + h->h, h->x, h->y + h->h },
        { h->x, h->y + h->h, h->x, h->y },
    };
    int hit = 0;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 4; j++)
            if (segs_cross(tri[i], box[j])) hit = 1;
    for (int i = 0; i < 3; i++) {
        if (contains(h, tri[i].x0, tri[i].y0) || contains(h, tri[i].x1, tri[i].y1)) hit = 1;
        else if (contains(h, (tri[i].x0 + tri[i].x1) / 2, (tri[i].y0 + tri[i].y1) / 2)) hit = 1;
    }
    return hit;
}

static void slope_normal(int type, float *nx, float *ny)
{
    const float s = 0.70710677f;
    switch (type) {
    case JK_SLOPE_TL: *nx = -s; *ny = -s; break;
    case JK_SLOPE_TR: *nx =  s; *ny = -s; break;
    case JK_SLOPE_BL: *nx = -s; *ny =  s; break;
    case JK_SLOPE_BR: *nx =  s; *ny =  s; break;
    default:          *nx = 0;  *ny = 0;  break;
    }
}

/* ---- consulta ---- */

static int floor_div(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

static int kind_blocks(int k)
{
    return k == JK_B_SOLID || k == JK_B_SLOPE || k == JK_B_ICE || k == JK_B_SNOW || k == JK_B_QUARK;
}

static int kind_flag(int k)
{
    switch (k) {
    case JK_B_ICE: return JK_C_ICE;
    case JK_B_SNOW: return JK_C_SNOW;
    case JK_B_SAND: return JK_C_SAND;
    case JK_B_WATER: return JK_C_WATER;
    case JK_B_QUARK: return JK_C_QUARK;
    case JK_B_NOWIND: return JK_C_NOWIND;
    default: return 0;
    }
}

/* LevelScreen.TryCollision sobre las celdas que toca la caja: bloques tocados, pendiente mas
 * cercana al centro y regla de "caja por encima de pendiente superior". */
static int grid_query(const jk_level *lv, int screen, const jk_rect *box, jk_cinfo *out)
{
    jk_cinfo info = { JK_SLOPE_NONE, 0, 0, 0 };
    int hit = 0, best_dx = -1, slope_top = 0, have_slope = 0, slope_top_type = JK_SLOPE_NONE;
    int box_top = 0, have_box = 0;
    int cx = box->x + box->w / 2;
    int tx0 = floor_div(box->x, 8), tx1 = floor_div(box->x + box->w - 1, 8);
    int ty0 = floor_div(box->y, 8), ty1 = floor_div(box->y + box->h - 1, 8);
    if (tx0 < 0) tx0 = 0;
    if (tx1 >= JK_TILE_W) tx1 = JK_TILE_W - 1;
    for (int ty = ty0; ty <= ty1; ty++) {
        int s = -floor_div(ty, JK_TILE_H);
        if (s < screen - 1 || s > screen + 1 || s < 0 || s >= lv->count)
            continue;
        const unsigned char *row = lv->grid + ((size_t)s * JK_TILE_H + (ty + s * JK_TILE_H)) * JK_TILE_W;
        for (int tx = tx0; tx <= tx1; tx++) {
            int k = row[tx] & 15, st = row[tx] >> 4;
            if (!k) continue;
            jk_rect t = { tx * 8, ty * 8, 8, 8 };
            if (k == JK_B_SLOPE) {
                if (!slope_hits(st, &t, box)) continue;
            }
            info.flags |= kind_flag(k);
            if (!kind_blocks(k)) continue;
            hit = 1;
            if (k == JK_B_SLOPE) {
                if (!have_slope || t.y < slope_top) { slope_top = t.y; slope_top_type = st; have_slope = 1; }
                int dx = t.x + 4 - cx; if (dx < 0) dx = -dx;
                if (best_dx < 0 || dx <= best_dx) {
                    best_dx = dx;
                    info.slope = st;
                    slope_normal(st, &info.nx, &info.ny);
                }
            } else if (!have_box || t.y < box_top) {
                box_top = t.y; have_box = 1;
            }
        }
    }
    if (have_box && have_slope && box_top <= slope_top &&
        (slope_top_type == JK_SLOPE_TL || slope_top_type == JK_SLOPE_TR))
        info.slope = JK_SLOPE_NONE;
    if (out) *out = info;
    return hit;
}

int jk_level_query(const jk_level *lv, int screen, const jk_rect *box, jk_cinfo *info)
{
    if (lv->grid)
        return grid_query(lv, screen, box, info);
    if (info) { info->slope = JK_SLOPE_NONE; info->nx = info->ny = 0; info->flags = 0; }
    for (int i = screen - 1; i <= screen + 1; i++) {
        if (i < 0 || i >= lv->count) continue;
        for (int b = 0; b < lv->screens[i].count; b++)
            if (jk_rect_overlap(&lv->screens[i].blocks[b], box)) return 1;
    }
    return 0;
}

int jk_level_collides(const jk_level *lv, int screen, const jk_rect *box)
{
    return jk_level_query(lv, screen, box, NULL);
}

/* Nivel de prueba propio (no es del juego): suelo, paredes y plataformas para medir saltos. */
static const jk_rect s0[] = {
    {   0, 344, 480, 16 },
    {   0,   0,   8, 360 },
    { 472,   0,   8, 360 },
    { 300, 296, 100,  48 },
    {  40, 220,  80,  12 },
    { 330, 150,  90,  12 },
    { 180,  60, 120,  12 },
};
static const jk_rect s1[] = {
    {   0, -360,   8, 360 },
    { 472, -360,   8, 360 },
    {  20,  -90, 100,  12 },
    { 360, -230, 100,  12 },
};
static const jk_screen screens[] = {
    { s0, sizeof s0 / sizeof s0[0], 0, { -1, -1 }, 0, -1, 0.0f },
    { s1, sizeof s1 / sizeof s1[0], 0, { -1, -1 }, 0, -1, 0.0f },
};
const jk_level jk_test_level = { screens, 2, 0 };

static jk_screen real_screens[256];

int jk_level_from_bin(jk_level *lv, const unsigned char *bin, int size)
{
    if (size < 8 || memcmp(bin, "JKL2", 4) != 0) return -1;
    int count = bin[4] | bin[5] << 8;
    if (count > 256 || size < 8 + count * (16 + JK_TILE_W * JK_TILE_H)) return -1;
    /* Las rejillas se compactan seguidas para indexarlas directamente. */
    static unsigned char grid[256 * JK_TILE_W * JK_TILE_H];
    const unsigned char *p = bin + 8;
    for (int i = 0; i < count; i++) {
        jk_screen *s = &real_screens[i];
        memset(s, 0, sizeof *s);
        s->wind_on = p[0];
        s->wind_dir = (signed char)p[1];
        memcpy(&s->wind_int, p + 4, 4);
        s->tele[0] = (short)(p[8] | p[9] << 8);
        s->tele[1] = (short)(p[10] | p[11] << 8);
        s->can_teleport = s->tele[0] != -1 || s->tele[1] != -1;
        memcpy(grid + (size_t)i * JK_TILE_W * JK_TILE_H, p + 16, JK_TILE_W * JK_TILE_H);
        p += 16 + JK_TILE_W * JK_TILE_H;
    }
    lv->screens = real_screens;
    lv->count = count;
    lv->grid = grid;
    return 0;
}
