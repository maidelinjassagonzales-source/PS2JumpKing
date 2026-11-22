#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include "npc.h"
#include "assets.h"
#include "jk/values.h"

#define MAX_NPC    40
#define MAX_QUOTES 32
#define MAX_LINES  8
#define SLOTS      4
#define SLOT_BYTES (192 * 1024)

typedef struct { unsigned char idx; float dur; } frame;
typedef struct { unsigned char kind; unsigned short screen; unsigned char n; const char *line[MAX_LINES]; unsigned char len[MAX_LINES]; } quote;

typedef struct {
    int cells_x, cells_y, x, y, home;          /* home: pantalla 1-based, como en el XML */
    int tw, th; float tcx, tcy; int sr[4];
    float talk_delay, speed; int bubble_w;
    int nbusy, ntalk; frame busy[16], talk[16];
    int nq; quote q[MAX_QUOTES];
    /* estado */
    /* OldManState: presentacion pendiente, pila de frases y mejor pantalla vista por este NPC */
    int say_intro; int best; int pool[64]; int pool_n; int loop_i;
    float anim_t; int anim_f;
    float touch_t;
    int talking, cur_q, cur_line, shown; float char_t, hold_t;
    int slot;                                   /* textura cargada o -1 */
} npc;

static npc npcs[MAX_NPC];
static int npc_count;
static GSTEXTURE slot_tex[SLOTS];
static int slot_owner[SLOTS];
static GSTEXTURE font_tex;
static int font_ok;
static short font_line; static float font_spacing;
static struct { short g[4], c[4]; float k[3]; } glyph[224];
static int loaded_screen = -2, quote_screen = -1, blip;

static unsigned char *rd;
static int  rd_u8(void)  { return *rd++; }
static int  rd_u16(void) { int v = rd[0] | rd[1] << 8; rd += 2; return v; }
static int  rd_s16(void) { return (short)rd_u16(); }
static float rd_f32(void) { float v; memcpy(&v, rd, 4); rd += 4; return v; }

int npc_init(GSGLOBAL *gs)
{
    int n;
    unsigned char *fb = assets_load("font.bin", &n);
    if (fb && memcmp(fb, "JKFN", 4) == 0) {
        rd = fb + 4; font_line = rd_s16(); font_spacing = rd_f32();
        for (int i = 0; i < 224; i++) {
            for (int j = 0; j < 4; j++) glyph[i].g[j] = rd_s16();
            for (int j = 0; j < 4; j++) glyph[i].c[j] = rd_s16();
            for (int j = 0; j < 3; j++) glyph[i].k[j] = rd_f32();
        }
        memset(&font_tex, 0, sizeof font_tex);
        font_ok = assets_t8(gs, "font.t8", &font_tex, 0) == 0;
    }

    unsigned char *b = assets_load("npc.bin", &n);
    if (!b || memcmp(b, "JKNP", 4) != 0) return -1;
    rd = b + 4;
    npc_count = rd_u16();
    if (npc_count > MAX_NPC) npc_count = MAX_NPC;
    for (int i = 0; i < npc_count; i++) {
        npc *p = &npcs[i];
        memset(p, 0, sizeof *p);
        unsigned char *next = rd + 4 + (rd[0] | rd[1] << 8 | rd[2] << 16 | rd[3] << 24);
        rd += 4;
        p->cells_x = rd_u8(); p->cells_y = rd_u8(); p->x = rd_s16(); p->y = rd_s16(); p->home = rd_u16();
        p->tw = rd_s16(); p->th = rd_s16(); p->tcx = rd_f32(); p->tcy = rd_f32();
        for (int j = 0; j < 4; j++) p->sr[j] = rd_s16();
        p->talk_delay = rd_f32(); p->speed = rd_f32(); p->bubble_w = rd_u16();
        if (p->speed == 0.0f) p->speed = 0.05f;
        p->nbusy = rd_u8(); for (int j = 0; j < p->nbusy; j++) { p->busy[j].idx = rd_u8(); p->busy[j].dur = rd_f32(); }
        p->ntalk = rd_u8(); for (int j = 0; j < p->ntalk; j++) { p->talk[j].idx = rd_u8(); p->talk[j].dur = rd_f32(); }
        p->nq = rd_u8(); if (p->nq > MAX_QUOTES) p->nq = MAX_QUOTES;
        for (int q = 0; q < p->nq; q++) {
            quote *qq = &p->q[q];
            qq->kind = rd_u8(); qq->screen = rd_u16(); qq->n = rd_u8();
            if (qq->n > MAX_LINES) qq->n = MAX_LINES;
            for (int l = 0; l < qq->n; l++) { qq->len[l] = rd_u8(); qq->line[l] = (const char *)rd; rd += qq->len[l]; }
        }
        p->slot = -1;
        for (int q = 0; q < p->nq; q++) if (p->q[q].kind == 0) p->say_intro = 1;
        rd = next;
    }
    /* VRAM fija para las hojas de los NPC de la pantalla actual */
    for (int s = 0; s < SLOTS; s++) {
        memset(&slot_tex[s], 0, sizeof slot_tex[s]);
        slot_tex[s].Vram = gsKit_vram_alloc(gs, SLOT_BYTES, GSKIT_ALLOC_USERBUFFER);
        slot_tex[s].VramClut = gsKit_vram_alloc(gs, gsKit_texture_size(16, 16, GS_PSM_CT32), GSKIT_ALLOC_USERBUFFER);
        slot_owner[s] = -1;
    }
    return 0;
}

/* Posicion en el mundo: GetRealPosition (sube 360 px por cada pantalla). */
static int npc_wy(const npc *p) { return p->y - (p->home - 1) * JK_SCREEN_H; }

static void trigger_rect(const npc *p, jk_rect *r)
{
    if (p->sr[2] != 0) {                         /* OldManHitbox.screen_rect */
        int h = JK_SCREEN_H, py = npc_wy(p);
        r->x = p->sr[0]; r->y = p->sr[1] + (py - h) / h * h; r->w = p->sr[2]; r->h = p->sr[3];
    } else {
        r->x = (int)((float)p->x - p->tw * p->tcx);
        r->y = (int)((float)npc_wy(p) - p->th * p->tcy);
        r->w = p->tw; r->h = p->th;
    }
}

static void load_screen(GSGLOBAL *gs, int screen)
{
    int used = 0;
    for (int i = 0; i < npc_count; i++) npcs[i].slot = -1;
    for (int i = 0; i < npc_count && used < SLOTS; i++) {
        if (npcs[i].home - 1 != screen) continue;
        char name[24];
        snprintf(name, sizeof name, "npc%d.t8", i);
        if (assets_t8(gs, name, &slot_tex[used], 1) == 0) npcs[i].slot = used++;
    }
    loaded_screen = screen;
}

/* OldManComp.Update: al cambiar de pantalla, si es una pantalla nueva (mas alta que la mejor de
 * este NPC) con frases propias, se apilan en orden inverso para que salgan en el orden del XML. */
static void on_screen_change(int screen)
{
    int num = screen + 1;
    for (int i = 0; i < npc_count; i++) {
        npc *p = &npcs[i];
        if (num <= p->best) continue;
        int found = 0;
        for (int q = p->nq - 1; q >= 0; q--)
            if (p->q[q].kind == 1 && p->q[q].screen == num && p->pool_n < 64) {
                p->pool[p->pool_n++] = q;
                found = 1;
            }
        if (found) p->best = num;
    }
}

/* OldManState.GetAndConsumeQuote + QuoteIndexer: presentacion, luego la pila (ultima apilada
 * primero), y con la pila vacia las frases en bucle. */
static int pick_quote(npc *p)
{
    if (p->say_intro) {
        p->say_intro = 0;
        for (int q = 0; q < p->nq; q++) if (p->q[q].kind == 0) return q;
    }
    if (p->pool_n > 0) return p->pool[--p->pool_n];
    int loops = 0;
    for (int q = 0; q < p->nq; q++) if (p->q[q].kind == 2) loops++;
    if (!loops) return -1;
    int want = p->loop_i++ % loops;
    for (int q = 0; q < p->nq; q++) if (p->q[q].kind == 2 && want-- == 0) return q;
    return -1;
}

void npc_update(GSGLOBAL *gs, const jk_king *k, float dt)
{
    if (k->screen != quote_screen) { quote_screen = k->screen; on_screen_change(k->screen); }
    if (k->screen != loaded_screen) load_screen(gs, k->screen);
    jk_rect hb = jk_body_hitbox(&k->body);

    for (int i = 0; i < npc_count; i++) {
        npc *p = &npcs[i];
        if (p->home - 1 != k->screen) { p->talking = 0; p->touch_t = 0; continue; }

        /* animacion: hablar mientras escribe, si no la de "ocupado" */
        const frame *fr = p->talking && p->shown < p->q[p->cur_q].len[p->cur_line] ? p->talk : p->busy;
        int nf = fr == p->talk ? p->ntalk : p->nbusy;
        if (nf) {
            p->anim_t += dt;
            if (p->anim_f >= nf) p->anim_f = 0;
            if (p->anim_t >= fr[p->anim_f].dur) { p->anim_t = 0; p->anim_f = (p->anim_f + 1) % nf; }
        }

        if (!p->talking) {
            jk_rect tr; trigger_rect(p, &tr);
            p->touch_t = jk_rect_overlap(&tr, &hb) ? p->touch_t + dt : 0.0f;
            float wait = p->talk_delay > 0.5f ? p->talk_delay : 0.5f;
            if (p->touch_t >= wait) {
                int q = pick_quote(p);
                p->touch_t = 0;
                if (q >= 0) { p->talking = 1; p->cur_q = q; p->cur_line = 0; p->shown = 0; p->char_t = 0; p->hold_t = 0; }
            }
            continue;
        }
        const quote *qq = &p->q[p->cur_q];
        if (p->shown < qq->len[p->cur_line]) {
            p->char_t += dt;
            if (p->char_t >= p->speed) {
                p->char_t -= p->speed;
                if (qq->line[p->cur_line][p->shown] != ' ') blip = 1;
                p->shown++;
            }
        } else {
            p->hold_t += dt;
            if (p->hold_t >= 2.2f) {                 /* pausa tras cada linea */
                p->hold_t = 0; p->shown = 0; p->char_t = 0;
                if (++p->cur_line >= qq->n) p->talking = 0;
            }
        }
    }
}

int npc_take_talk_blip(void) { int b = blip; blip = 0; return b; }

float ui_text_width(const char *s, int n)
{
    float w = 0;
    for (int i = 0; i < n; i++) {
        int c = (unsigned char)s[i] - 32; if (c < 0 || c >= 224) c = '?' - 32;
        w += glyph[c].k[0] + glyph[c].k[1] + glyph[c].k[2] + font_spacing;
    }
    return w;
}

void ui_draw_text(GSGLOBAL *gs, const char *s, int n, float x, float y, float sx, float sy)
{
    float cx = x;
    for (int i = 0; i < n; i++) {
        int c = (unsigned char)s[i] - 32; if (c < 0 || c >= 224) c = '?' - 32;
        cx += glyph[c].k[0];
        const short *g = glyph[c].g, *cr = glyph[c].c;
        if (g[2] > 0 && font_ok) {
            float x0 = (cx + cr[0]) * sx, y0 = (y + cr[1]) * sy;
            gsKit_prim_sprite_texture(gs, &font_tex, x0, y0, g[0], g[1],
                                      x0 + g[2] * sx, y0 + g[3] * sy, g[0] + g[2], g[1] + g[3], 6, 0x80808080);
        }
        cx += glyph[c].k[1] + glyph[c].k[2] + font_spacing;
    }
}

/* Bocadillo: lineas partidas por palabras al ancho bubble_w, encima del NPC. */
static void draw_bubble(GSGLOBAL *gs, const npc *p, int screen, float sx, float sy)
{
    const quote *qq = &p->q[p->cur_q];
    const char *s = qq->line[p->cur_line];
    int n = p->shown;
    int starts[12], lens[12], rows = 0, i = 0;
    while (i < n && rows < 12) {
        int st = i, last_sp = -1;
        while (i < n && ui_text_width(s + st, i - st + 1) <= p->bubble_w) { if (s[i] == ' ') last_sp = i; i++; }
        if (i < n && last_sp > st) i = last_sp + 1;
        starts[rows] = st; lens[rows] = i - st; rows++;
    }
    if (!rows) return;
    float h = rows * (float)font_line + 8.0f, w = (float)p->bubble_w + 8.0f;
    float bx = (float)p->x - w / 2.0f, by = (float)(npc_wy(p) + screen * JK_SCREEN_H) - 70.0f - h;
    if (bx < 4) bx = 4;
    if (bx + w > JK_SCREEN_W - 4) bx = JK_SCREEN_W - 4 - w;
    if (by < 4) by = 4;
    gsKit_prim_sprite(gs, bx * sx, by * sy, (bx + w) * sx, (by + h) * sy, 5, GS_SETREG_RGBAQ(0, 0, 0, 0x60, 0));
    for (int r = 0; r < rows; r++)
        ui_draw_text(gs, s + starts[r], lens[r], bx + 4, by + 4 + r * font_line, sx, sy);
}

void npc_draw(GSGLOBAL *gs, int screen, float sx, float sy)
{
    for (int i = 0; i < npc_count; i++) {
        npc *p = &npcs[i];
        if (p->home - 1 != screen || p->slot < 0) continue;
        GSTEXTURE *t = &slot_tex[p->slot];
        int cw = t->Width / p->cells_x, ch = t->Height / p->cells_y;
        const frame *fr = p->talking && p->shown < p->q[p->cur_q].len[p->cur_line] ? p->talk : p->busy;
        int nf = fr == p->talk ? p->ntalk : p->nbusy;
        int idx = nf ? fr[p->anim_f % nf].idx : 0;
        float u0 = (idx % p->cells_x) * cw, v0 = (idx / p->cells_x) * ch;
        float ax = (float)p->x, ay = (float)(npc_wy(p) + screen * JK_SCREEN_H);   /* ancla abajo-centro */
        float x0 = (ax - cw / 2.0f) * sx, y0 = (ay - ch) * sy;
        gsKit_prim_sprite_texture(gs, t, x0, y0, u0, v0, x0 + cw * sx, y0 + ch * sy, u0 + cw, v0 + ch, 2, 0x80808080);
    }
}

/* Bocadillos aparte: van encima de la capa de frente para que no los tape el decorado. */
void npc_draw_bubbles(GSGLOBAL *gs, int screen, float sx, float sy)
{
    for (int i = 0; i < npc_count; i++)
        if (npcs[i].home - 1 == screen && npcs[i].talking) draw_bubble(gs, &npcs[i], screen, sx, sy);
}
