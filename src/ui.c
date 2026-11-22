/* Menus con la estructura del original (GameTitleScreen.CreateMenu, MenuFactory, PauseManager):
 * pila de MenuSelector en cascada, GuiFormat (margen, separacion, relleno, anclaje), marco GuiFrame,
 * cursor, flecha de volver (IconButton.CreateBackButton), casillas (ITextToggle), deslizador de
 * volumen (ISlider/VolumeSlider) y selectores con flechas. Textos, fuentes e iconos salen de la copia
 * del usuario (tools/build_ui.py). Lo que en PS2 no tiene sentido (graficos de PC, reasignar teclas,
 * Taller) se muestra igual pero no hace nada (sonido de fallo). */
#include <stdio.h>
#include <string.h>
#include "ui.h"
#include "font.h"
#include "assets.h"
#include "audio.h"
#include "jk/values.h"
#include "items.h"

enum {
    T_PRESS_START, T_CONTINUE, T_NEW_GAME, T_RESUME, T_SAVE_EXIT, T_SURE, T_YES, T_NO,
    T_COPYRIGHT, T_SAVED, T_WORKSHOP, T_EXTRAS, T_OPTIONS, T_INVENTORY, T_CREDITS, T_LICENSE,
    T_TOTAL_STATS, T_FONT_ATTR, T_JKPLUS_ATTR, T_SESSION, T_TIME, T_JUMPS, T_FALLS,
    T_GRAPHICS, T_CONTROLS, T_AUDIO, T_INGAME, T_TIMER_OPTS, T_QUICKLOAD, T_SHOW_TIMER,
    T_TIMER_DEFAULT, T_TIMER_PRECISE, T_MUSIC, T_SFX, T_AMBIENCE, T_SCAN, T_BIND1, T_BIND2,
    T_PAUSE_OPTIONS, T_INTRO, T_COMPLETED, T_COUNT
};

#define WHITE 0x80808080
#define RED   GS_SETREG_RGBAQ(0x80, 0x00, 0x00, 0x80, 0)
#define GREEN GS_SETREG_RGBAQ(0x00, 0x80, 0x00, 0x80, 0)
#define GRAY  GS_SETREG_RGBAQ(0x40, 0x40, 0x40, 0x80, 0)

static char text[T_COUNT][256];
static GSTEXTURE logo, frame, cursor, back_ic, checkbox, slider_t, volume_t, arrows, nexile;
static int have_logo, have_frame, have_cursor, have_back, have_cb, have_sl, have_vol, have_arr, have_nexile;
static jk_font menufont, smallfont;

/* ---- opciones y estadisticas (se guardaran en la memory card) ---- */
static int opt_music = 1, opt_sfx = 1, opt_amb = 1, opt_quickload = 0, opt_show_timer = 0;
static int opt_timer_mode = 0, opt_screen_mode = 0, opt_size = 1;
static float opt_volume = 1.0f;
static int st_attempt = 1, st_session = 1, st_jumps, st_falls;
static float st_time;

static void update_stats_text(void);

void ui_stats_event(int jumps, int falls, float dt) { st_jumps += jumps; st_falls += falls; st_time += dt; }

static void apply_audio(void) { audio_set_options(opt_volume, opt_music, opt_sfx, opt_amb); }

typedef struct {
    int music, sfx, amb, quickload, show_timer, timer_mode, screen_mode, size;
    float volume;
    int attempt, session, jumps, falls;
    float time;
} ui_state;

int ui_save(unsigned char *out, int max)
{
    ui_state s = { opt_music, opt_sfx, opt_amb, opt_quickload, opt_show_timer, opt_timer_mode, opt_screen_mode,
                   opt_size, opt_volume, st_attempt, st_session, st_jumps, st_falls, st_time };
    if (max < (int)sizeof s) return 0;
    memcpy(out, &s, sizeof s);
    return sizeof s;
}

void ui_load(const unsigned char *in, int size)
{
    ui_state s;
    if (size < (int)sizeof s) return;
    memcpy(&s, in, sizeof s);
    opt_music = s.music; opt_sfx = s.sfx; opt_amb = s.amb; opt_quickload = s.quickload;
    opt_show_timer = s.show_timer; opt_timer_mode = s.timer_mode; opt_screen_mode = s.screen_mode;
    opt_size = s.size; opt_volume = s.volume;
    st_attempt = s.attempt; st_session = s.session; st_jumps = s.jumps; st_falls = s.falls; st_time = s.time;
    apply_audio();
    update_stats_text();
}

void ui_new_attempt(void) { st_attempt++; st_session = 1; st_jumps = st_falls = 0; st_time = 0; update_stats_text(); }
void ui_new_session(void) { st_session++; update_stats_text(); }

/* ---- elementos ---- */

enum { K_BUTTON, K_INFO, K_BACK, K_TOGGLE, K_SLIDER, K_CHOICE, K_SPACE };
enum { A_NONE, A_SUB, A_NEW_GAME, A_CONTINUE, A_RESUME, A_EXIT_CONFIRM, A_EXIT, A_FAIL };

typedef struct {
    int kind;
    const char *txt;          /* texto ya resuelto */
    u64 color;
    int small;                /* MenuFontSmall */
    int action, sub;          /* A_*, menu destino */
    int *val;                 /* casilla o indice de selector */
    const char *const *choices; int nchoices;
    int h;                    /* K_SPACE */
} item;

typedef struct {
    int bx, by, bw, bh;
    float ax, ay;
    int margin, elem, pad;
} gui_format;

#define MAX_ITEMS 40
typedef struct {
    gui_format f;
    item it[MAX_ITEMS];
    int n, sel, escape;
} menu;

enum { M_MAIN, M_NEWGAME, M_EXTRAS, M_INV, M_CREDITS, M_LICENSE, M_STATS, M_OPTIONS, M_GFX, M_GFX_MODE,
       M_CTRL, M_AUDIO, M_INGAME, M_TIMER, M_PAUSE, M_PAUSE_OPT, M_CONFIRM, M_COUNT };
static menu menus[M_COUNT];
static int stack[8], depth;

static const gui_format BASE = { 0, 0, 480, 360, 1.0f, 0.0f, 16, 8, 16 };   /* PauseManager.GUI_FORMAT */

static gui_format fmt_title(int dx)
{
    gui_format f = BASE;
    f.ax = 0.0f; f.by = 160; f.bx += dx;
    return f;
}

static const jk_font *fnt(const item *it) { return it->small ? &smallfont : &menufont; }

/* tamano de un texto con varias lineas */
static void text_size(const jk_font *f, const char *s, float *w, float *h)
{
    char line[256];
    *w = 0; *h = 0;
    while (1) {
        const char *nl = strchr(s, '\n');
        int len = nl ? (int)(nl - s) : (int)strlen(s);
        if (len > 255) len = 255;
        memcpy(line, s, len); line[len] = 0;
        float lw = font_width(f, line);
        if (lw > *w) *w = lw;
        *h += f->line;
        if (!nl) break;
        s = nl + 1;
    }
}

static void item_size(const item *it, float *w, float *h)
{
    switch (it->kind) {
    case K_BACK:   *w = back_ic.Width; *h = back_ic.Height; return;
    case K_SPACE:  *w = 0; *h = it->h; return;
    case K_SLIDER: *w = 8 * 3 + 50; *h = 8 * 2 + 1; return;
    case K_TOGGLE: text_size(fnt(it), it->txt, w, h); *w += 2 + 10; return;
    case K_CHOICE: text_size(fnt(it), it->choices[*it->val], w, h); *w += 2 * 8 + 4; return;
    default:       text_size(fnt(it), it->txt, w, h); return;
    }
}

/* GuiFormat.CalculateBounds */
static void bounds(const menu *m, int *x, int *y, int *w, int *h)
{
    const gui_format *f = &m->f;
    int bx = f->bx + f->margin, by = f->by + f->margin, bw = f->bw - 2 * f->margin, bh = f->bh - 2 * f->margin;
    float mw = 0, mh = 0;
    for (int i = 0; i < m->n; i++) {
        float iw, ih; item_size(&m->it[i], &iw, &ih);
        if (iw > mw) mw = iw;
        mh += ih;
    }
    *w = (int)mw + 2 * f->pad;
    *h = (int)mh + f->elem * (m->n - 1) + 2 * f->pad;
    if (*w > bw) *w = bw;
    *x = (int)((float)bx + bw * f->ax) - (int)(*w * f->ax);
    *y = (int)((float)by + bh * f->ay) - (int)(*h * f->ay);
}

static void draw_frame(GSGLOBAL *gs, int x, int y, int w, int h, float sx, float sy)
{
    if (!have_frame) {
        gsKit_prim_sprite(gs, x * sx, y * sy, (x + w) * sx, (y + h) * sy, 5, GS_SETREG_RGBAQ(0, 0, 0, 0x70, 0));
        return;
    }
    int c = frame.Width / 3;
    int xs[4] = { x, x + c, x + w - c, x + w }, ys[4] = { y, y + c, y + h - c, y + h };
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            gsKit_prim_sprite_texture(gs, &frame, xs[i] * sx, ys[j] * sy, i * c, j * c,
                                      xs[i + 1] * sx, ys[j + 1] * sy, (i + 1) * c, (j + 1) * c, 5, WHITE);
}

static void blit(GSGLOBAL *gs, GSTEXTURE *t, int u, int v, int w, int h, float x, float y, float dw, float sx, float sy)
{
    gsKit_prim_sprite_texture(gs, t, x * sx, y * sy, u, v, (x + dw) * sx, (y + h) * sy, u + w, v + h, 6, WHITE);
}

static void draw_text_ml(GSGLOBAL *gs, const jk_font *f, const char *s, float x, float y, float sx, float sy, u64 c)
{
    char line[256];
    while (1) {
        const char *nl = strchr(s, '\n');
        int len = nl ? (int)(nl - s) : (int)strlen(s);
        if (len > 255) len = 255;
        memcpy(line, s, len); line[len] = 0;
        font_draw(gs, f, line, x, y, sx, sy, c);
        y += f->line;
        if (!nl) break;
        s = nl + 1;
    }
}

static void draw_item(GSGLOBAL *gs, const item *it, float x, float y, float sx, float sy)
{
    float w, h;
    switch (it->kind) {
    case K_BACK:
        if (have_back) blit(gs, &back_ic, 0, 0, back_ic.Width, back_ic.Height, x, y, back_ic.Width, sx, sy);
        break;
    case K_TOGGLE:                                         /* texto + casilla centrada en la linea */
        text_size(fnt(it), it->txt, &w, &h);
        draw_text_ml(gs, fnt(it), it->txt, x, y, sx, sy, it->color);
        if (have_cb) blit(gs, &checkbox, *it->val ? 10 : 0, 0, 10, 10, x + w + 2, y + h / 2 - 5, 10, sx, sy);
        break;
    case K_SLIDER: {                                       /* icono de volumen + barra de 50 px */
        y += 4;
        int lvl = opt_volume <= 0.0f ? 0 : opt_volume < 0.34f ? 1 : opt_volume < 0.67f ? 2 : 3;
        int vw = volume_t.Width / 4;
        if (have_vol) blit(gs, &volume_t, lvl * vw, 0, vw, volume_t.Height, x, y, vw, sx, sy);
        x += vw;
        if (have_sl) {
            blit(gs, &slider_t, 0, 0, 8, 8, x, y, 8, sx, sy);
            blit(gs, &slider_t, 8, 0, 8, 8, x + 8, y, 50, sx, sy);
            blit(gs, &slider_t, 16, 0, 8, 8, x + 58, y, 8, sx, sy);
            blit(gs, &slider_t, 24, 0, 8, 8, x + 8 + 50.0f * opt_volume - 4, y, 8, sx, sy);
        }
        break;
    }
    case K_CHOICE: {                                       /* < valor > */
        const char *v = it->choices[*it->val];
        if (have_arr) blit(gs, &arrows, 0, 0, 8, 7, x, y + 4, 8, sx, sy);
        font_draw(gs, fnt(it), v, x + 10, y, sx, sy, GRAY);
        text_size(fnt(it), v, &w, &h);
        if (have_arr) blit(gs, &arrows, 8, 0, 8, 7, x + 12 + w, y + 4, 8, sx, sy);
        break;
    }
    case K_SPACE:
        break;
    default:
        draw_text_ml(gs, fnt(it), it->txt, x, y, sx, sy, it->color);
    }
}

static int selectable(const item *it) { return it->kind != K_INFO && it->kind != K_SPACE; }

/* MenuSelector.Draw -> GuiFormat.DrawMenuItemsWithCursor */
static void draw_menu(GSGLOBAL *gs, const menu *m, float sx, float sy)
{
    int x, y, w, h;
    bounds(m, &x, &y, &w, &h);
    draw_frame(gs, x, y, w, h, sx, sy);
    float off = m->f.pad;
    for (int i = 0; i < m->n; i++) {
        float iw, ih;
        item_size(&m->it[i], &iw, &ih);
        int sel = i == m->sel && selectable(&m->it[i]);
        draw_item(gs, &m->it[i], x + m->f.pad + (sel ? 5 : 0), y + off, sx, sy);
        if (sel && have_cursor)
            blit(gs, &cursor, 0, 0, cursor.Width, cursor.Height, x, y + off, cursor.Width, sx, sy);
        off += ih + m->f.elem;
    }
}

/* ---- construccion de los menus ---- */

static item *add(menu *m, item it) { if (m->n < MAX_ITEMS) m->it[m->n++] = it; return &m->it[m->n - 1]; }
static item btn(int t, int action, int sub) { return (item){ K_BUTTON, text[t], WHITE, 0, action, sub, 0, 0, 0, 0 }; }
static item info(const char *s, u64 c, int small) { return (item){ K_INFO, s, c, small, A_NONE, 0, 0, 0, 0, 0 }; }
static item toggle(int t, int *v) { return (item){ K_TOGGLE, text[t], WHITE, 0, A_NONE, 0, v, 0, 0, 0 }; }
static void finish(menu *m) { if (m->escape) add(m, (item){ K_BACK, "", WHITE, 0, A_NONE, 0, 0, 0, 0, 0 }); }

static char credit_lines[64][128];
static int credit_count;
static char stat_lines[4][96];
static const char *const MODES[] = { "Borderless", "Windowed", "Fullscreen" };
static const char *const SIZES[] = { "x1", "x2", "x3" };
static char timer_names[2][64];
static const char *timer_choices[2];
static char bind_lines[8][64];

/* "Fuente: {0}" -> sustituye {0}/{1}; si no hay hueco, concatena */
static void fmt2(char *out, int sz, const char *t, const char *a, const char *b)
{
    char tmp[256]; tmp[0] = 0;
    const char *p = t;
    int used_a = 0;
    while (*p && (int)strlen(tmp) < sz - 16) {
        if (p[0] == '{' && p[1] == '0' && p[2] == '}') { strcat(tmp, a); p += 3; used_a = 1; continue; }
        if (p[0] == '{' && p[1] == '1' && p[2] == '}' && b) { strcat(tmp, b); p += 3; continue; }
        int l = strlen(tmp); tmp[l] = *p++; tmp[l + 1] = 0;
    }
    if (!used_a) { strcat(tmp, " "); strcat(tmp, a); }
    strncpy(out, tmp, sz - 1); out[sz - 1] = 0;
}

/* Inventario (InventoryButton): un boton por objeto que se tiene; las monedas con su cantidad */
static char inv_lines[ITEM_KINDS][80];
static void build_inventory(void)
{
    menu *m = &menus[M_INV];
    m->n = 0; m->sel = 0;
    for (int i = 0; i < ITEM_KINDS; i++) {
        int c = items_count(i);
        if (c <= 0) continue;
        if (c > 1) snprintf(inv_lines[i], sizeof inv_lines[i], "%s x%d", items_name(i), c);
        else snprintf(inv_lines[i], sizeof inv_lines[i], "%s", items_name(i));
        if (items_is_wearable(i))             /* ToggleSkin: casilla para ponerse/quitarse la prenda */
            add(m, (item){ K_TOGGLE, inv_lines[i], WHITE, 0, A_NONE, 0, items_equip_flag(i), 0, 0, 0 });
        else
            add(m, (item){ K_BUTTON, inv_lines[i], WHITE, 0, A_NONE, 0, 0, 0, 0, 0 });
    }
    finish(m);
}

static void update_stats_text(void)
{
    char a[16], b[16];
    snprintf(a, sizeof a, "%d", st_attempt); snprintf(b, sizeof b, "%d", st_session);
    fmt2(stat_lines[0], sizeof stat_lines[0], text[T_SESSION], a, b);
    int ms = (int)(st_time * 1000.0f), hh = ms / 3600000, mm = ms / 60000 % 60, ss = ms / 1000 % 60;
    snprintf(stat_lines[1], sizeof stat_lines[1], "%s %dh %dm %ds %dms", text[T_TIME], hh, mm, ss, ms % 1000);
    snprintf(a, sizeof a, "%d", st_jumps);
    fmt2(stat_lines[2], sizeof stat_lines[2], text[T_JUMPS], a, 0);
    snprintf(a, sizeof a, "%d", st_falls);
    fmt2(stat_lines[3], sizeof stat_lines[3], text[T_FALLS], a, 0);
}

/* Creditos: nombre (MenuFont) y cargos partidos por comas al ancho (MenuFontSmall), sangrados */
static void build_credits(menu *m)
{
    int n;
    unsigned char *b = assets_load("credits.bin", &n);
    if (!b || memcmp(b, "JKCR", 4) != 0) return;
    int count = b[4] | b[5] << 8, p = 6;
    float maxw = 480 - 2 * 8 - 2 * 16 - 8;
    for (int i = 0; i < count && credit_count < 60; i++) {
        int l = b[p++];
        char *name = credit_lines[credit_count++];
        memcpy(name, b + p, l); name[l] = 0; p += l;
        add(m, info(name, GRAY, 0));
        l = b[p++];
        char role[256]; memcpy(role, b + p, l); role[l] = 0; p += l;
        /* TextInfo.CreateFittedInfo: cortar en comas */
        char *s = role;
        while (*s && credit_count < 60) {
            char *line = credit_lines[credit_count];
            strcpy(line, "  ");
            char *cut = s, *best = 0;
            while (1) {
                char *c = strchr(cut, ',');
                int len = c ? (int)(c - s + 1) : (int)strlen(s);
                char tryb[128]; snprintf(tryb, sizeof tryb, "  %.*s", len, s);
                if (font_width(&smallfont, tryb) > maxw && best) break;
                best = c ? c + 1 : s + strlen(s);
                if (!c) break;
                cut = c + 1;
            }
            int len = (int)(best - s);
            snprintf(line + 2, 126, "%.*s", len, s);
            credit_count++;
            add(m, info(line, GRAY, 1));
            s = best;
            while (*s == ' ') s++;
        }
        add(m, (item){ K_SPACE, "", 0, 0, 0, 0, 0, 0, 0, 4 });
    }
}

static void build_menus(void)
{
    memset(menus, 0, sizeof menus);
    for (int i = 0; i < M_COUNT; i++) { menus[i].escape = 1; menus[i].f = fmt_title(0); }

    menu *m = &menus[M_MAIN]; m->escape = 0;                 /* se llena en ui_title_enter */

    m = &menus[M_NEWGAME]; m->f = fmt_title(60);
    add(m, btn(T_NEW_GAME, A_NEW_GAME, 0)); finish(m);

    m = &menus[M_EXTRAS]; m->f = fmt_title(60);
    add(m, btn(T_INVENTORY, A_SUB, M_INV));
    add(m, btn(T_CREDITS, A_SUB, M_CREDITS));
    add(m, btn(T_LICENSE, A_SUB, M_LICENSE));
    add(m, btn(T_TOTAL_STATS, A_SUB, M_STATS));
    finish(m);

    m = &menus[M_INV]; m->f = BASE; m->f.elem = 1; finish(m);   /* sin objetos todavia */

    m = &menus[M_CREDITS]; m->f = BASE; m->f.margin = 8; m->f.elem = 0;
    build_credits(m);
    add(m, info("Aroy 24", GRAY, 0));                       /* port credits */
    add(m, info("  Port para PlayStation 2", GRAY, 1));
    finish(m);

    m = &menus[M_LICENSE]; m->f = fmt_title(120);
    add(m, info(text[T_FONT_ATTR], GRAY, 0));
    add(m, info(text[T_JKPLUS_ATTR], GRAY, 0));
    finish(m);

    m = &menus[M_STATS]; m->f = fmt_title(120);
    for (int i = 0; i < 4; i++) add(m, info(stat_lines[i], GRAY, 0));
    finish(m);

    m = &menus[M_OPTIONS]; m->f = fmt_title(60);
    add(m, btn(T_GRAPHICS, A_SUB, M_GFX));
    add(m, btn(T_CONTROLS, A_SUB, M_CTRL));
    add(m, btn(T_AUDIO, A_SUB, M_AUDIO));
    add(m, btn(T_INGAME, A_SUB, M_INGAME));
    finish(m);

    m = &menus[M_GFX]; m->f = fmt_title(120);
    add(m, (item){ K_CHOICE, "", WHITE, 0, A_NONE, 0, &opt_screen_mode, MODES, 3, 0 });
    add(m, (item){ K_CHOICE, "", WHITE, 0, A_NONE, 0, &opt_size, SIZES, 3, 0 });
    finish(m);

    m = &menus[M_CTRL]; m->f = BASE; m->f.ax = 0.0f; m->f.bx = 16;
    add(m, (item){ K_BUTTON, text[T_SCAN], WHITE, 1, A_FAIL, 0, 0, 0, 0, 0 });
    add(m, (item){ K_BUTTON, text[T_BIND1], WHITE, 1, A_FAIL, 0, 0, 0, 0, 0 });
    add(m, (item){ K_BUTTON, text[T_BIND2], WHITE, 1, A_FAIL, 0, 0, 0, 0, 0 });
    {
        static const char *const keys[8][2] = { { "Up", "Arriba" }, { "Down", "Abajo" }, { "Left", "Izquierda" },
            { "Right", "Derecha" }, { "Jump", "X" }, { "Confirm", "X" }, { "Cancel", "Circulo" }, { "Pause", "Start" } };
        for (int i = 0; i < 8; i++) {
            snprintf(bind_lines[i], sizeof bind_lines[i], "%s : %s", keys[i][0], keys[i][1]);
            add(m, info(bind_lines[i], GRAY, 1));
        }
    }
    finish(m);

    m = &menus[M_AUDIO]; m->f = fmt_title(120);
    add(m, (item){ K_SLIDER, "", WHITE, 0, A_NONE, 0, 0, 0, 0, 0 });
    add(m, toggle(T_MUSIC, &opt_music));
    add(m, toggle(T_SFX, &opt_sfx));
    add(m, toggle(T_AMBIENCE, &opt_amb));
    finish(m);

    m = &menus[M_INGAME]; m->f = fmt_title(120);
    add(m, btn(T_TIMER_OPTS, A_SUB, M_TIMER));
    add(m, toggle(T_QUICKLOAD, &opt_quickload));
    finish(m);

    strcpy(timer_names[0], text[T_TIMER_DEFAULT]); strcpy(timer_names[1], text[T_TIMER_PRECISE]);
    timer_choices[0] = timer_names[0]; timer_choices[1] = timer_names[1];
    m = &menus[M_TIMER]; m->f = fmt_title(180);
    add(m, toggle(T_SHOW_TIMER, &opt_show_timer));
    add(m, (item){ K_CHOICE, "", WHITE, 0, A_NONE, 0, &opt_timer_mode, timer_choices, 2, 0 });
    finish(m);

    /* pausa: arriba a la derecha (PauseManager) */
    m = &menus[M_PAUSE]; m->f = BASE; m->escape = 0;
    add(m, btn(T_RESUME, A_RESUME, 0));
    add(m, btn(T_INVENTORY, A_SUB, M_INV));
    add(m, btn(T_PAUSE_OPTIONS, A_SUB, M_PAUSE_OPT));
    add(m, btn(T_SAVE_EXIT, A_EXIT_CONFIRM, 0));
    m = &menus[M_PAUSE_OPT]; *m = menus[M_OPTIONS]; m->f = BASE; m->f.bx = -60;
    m = &menus[M_CONFIRM]; m->f = BASE; m->f.ax = 0.5f; m->f.ay = 0.5f; m->f.bx = 32;
    add(m, info(text[T_SURE], RED, 0));
    add(m, btn(T_NO, A_NONE, -1));
    add(m, btn(T_YES, A_EXIT, 0));
}

int ui_init(GSGLOBAL *gs)
{
    int n;
    unsigned char *b = assets_load("ui_text.bin", &n);
    for (int i = 0; i < T_COUNT; i++) strcpy(text[i], "?");
    if (b && memcmp(b, "JKTX", 4) == 0) {
        int count = b[4] | b[5] << 8, p = 6;
        for (int i = 0; i < count && i < T_COUNT; i++) {
            int l = b[p++];
            memcpy(text[i], b + p, l); text[i][l] = 0; p += l;
        }
    }
    GSTEXTURE *texs[] = { &logo, &frame, &cursor, &back_ic, &checkbox, &slider_t, &volume_t, &arrows, &nexile };
    int *have[] = { &have_logo, &have_frame, &have_cursor, &have_back, &have_cb, &have_sl, &have_vol, &have_arr, &have_nexile };
    const char *names[] = { "title.t8", "frame.t8", "cursor.t8", "back.t8", "checkbox.t8", "slider.t8", "volume.t8", "arrows.t8", "nexile.t8" };
    for (int i = 0; i < 9; i++) { memset(texs[i], 0, sizeof *texs[i]); *have[i] = assets_t8(gs, names[i], texs[i], 0) == 0; }
    font_load(gs, &menufont, "menufont");
    font_load(gs, &smallfont, "smallfont");
    update_stats_text();
    build_menus();
    apply_audio();
    return 0;
}

/* ---- navegacion ---- */

static menu *top(void) { return &menus[stack[depth - 1]]; }

static void push(int id)
{
    if (depth < 8) { stack[depth++] = id; menu *m = &menus[id]; m->sel = 0;
        while (m->sel < m->n - 1 && !selectable(&m->it[m->sel])) m->sel++; }
    audio_play_sfx(SFX_SELECT);
}

static void pop(void) { if (depth > 1) { depth--; audio_play_sfx(SFX_SELECT); } }

static void move(menu *m, int dir)
{
    for (int k = 0; k < m->n; k++) {
        m->sel = (m->sel + dir + m->n) % m->n;
        if (selectable(&m->it[m->sel])) break;
    }
    audio_play_sfx(SFX_SELECT);
}

/* devuelve una accion A_* que debe atender quien llama */
static int menu_input(int pressed)
{
    menu *m = top();
    item *it = &m->it[m->sel];
    if (pressed & UI_UP) move(m, -1);
    if (pressed & UI_DOWN) move(m, 1);
    if (pressed & (UI_LEFT | UI_RIGHT)) {
        int d = (pressed & UI_RIGHT) ? 1 : -1;
        if (it->kind == K_SLIDER) {
            opt_volume += d * 0.1f;
            if (opt_volume < 0) opt_volume = 0;
            if (opt_volume > 1) opt_volume = 1;
            apply_audio(); audio_play_sfx(SFX_SELECT);
        } else if (it->kind == K_CHOICE) {
            *it->val = (*it->val + d + it->nchoices) % it->nchoices;
            audio_play_sfx(SFX_SELECT);
        }
    }
    if (pressed & UI_BACK) { if (m->escape) pop(); return A_NONE; }
    if (!(pressed & UI_OK)) return A_NONE;
    switch (it->kind) {
    case K_BACK: pop(); return A_NONE;
    case K_TOGGLE: *it->val = !*it->val; apply_audio(); audio_play_sfx(SFX_SELECT); return A_NONE;
    case K_CHOICE: *it->val = (*it->val + 1) % it->nchoices; audio_play_sfx(SFX_SELECT); return A_NONE;
    case K_BUTTON:
        if (it->action == A_SUB) {
            if (it->sub == M_STATS) update_stats_text();
            if (it->sub == M_INV) build_inventory();
            push(it->sub);
            return A_NONE;
        }
        if (it->action == A_FAIL) { audio_play_sfx(SFX_MENU_FAIL); return A_NONE; }
        if (it->sub == -1) { pop(); return A_NONE; }
        if (it->action == A_NONE) return A_NONE;
        return it->action;
    }
    return A_NONE;
}

static void draw_stack(GSGLOBAL *gs, float sx, float sy)
{
    for (int i = 0; i < depth; i++) draw_menu(gs, &menus[stack[i]], sx, sy);
}

/* ---- titulo ---- */

enum { P_LOGO, P_WAIT, P_PRESS, P_PRESSED, P_MENU };
static int phase, t, logo_frames, shake_tick, shake_skip, shake_mag, hit_played, has_save;

void ui_title_enter(int save)
{
    phase = P_LOGO; t = 0; logo_frames = 0;
    shake_tick = -1; shake_skip = 0; shake_mag = 0; hit_played = 0;
    has_save = save;
    /* GameTitleScreen.CreateMenu: Continuar (si hay partida), Nueva partida y Taller en verde,
     * Extras, Opciones. Salir solo existe en PC. */
    menu *m = &menus[M_MAIN];
    m->n = 0; m->sel = 0;
    if (has_save) add(m, btn(T_CONTINUE, A_CONTINUE, 0));
    item *ng = add(m, btn(T_NEW_GAME, A_SUB, M_NEWGAME)); ng->color = GREEN;
    item *ws = add(m, btn(T_WORKSHOP, A_FAIL, 0)); ws->color = GREEN;
    add(m, btn(T_EXTRAS, A_SUB, M_EXTRAS));
    add(m, btn(T_OPTIONS, A_SUB, M_OPTIONS));
    stack[0] = M_MAIN; depth = 1;
    audio_music("menu_intro", "menu_loop");
}

static void shake_update(void)
{
    if (shake_tick < 0) return;
    shake_mag = (shake_tick / 2 * 2 == shake_tick) ? -shake_tick : shake_tick;
    if (shake_skip > 0) { shake_skip--; return; }
    shake_skip = 3;
    if (--shake_tick < 0) shake_mag = 0;
}

int ui_title_update(int pressed)
{
    t++;
    if (logo_frames < 120) {
        logo_frames++;
        if (pressed & (UI_OK | UI_START)) logo_frames = 120;
        if (logo_frames == 120 && !hit_played) { hit_played = 1; audio_play_sfx(SFX_TITLE_HIT); shake_tick = 10; }
    }
    shake_update();
    switch (phase) {
    case P_LOGO:
        if (t >= 150 || (pressed & (UI_OK | UI_START))) { phase = P_WAIT; t = 0; }
        break;
    case P_WAIT:
        if (t >= 60 || (pressed & (UI_OK | UI_START))) { phase = P_PRESS; t = 0; }
        break;
    case P_PRESS:
        if (pressed & (UI_OK | UI_START)) { phase = P_PRESSED; t = 0; audio_play_sfx(SFX_PRESS_START); }
        break;
    case P_PRESSED:
        if (t >= 42) { phase = P_MENU; t = 0; }
        break;
    case P_MENU: {
        int a = menu_input(pressed);
        if (a == A_NEW_GAME || a == A_CONTINUE) {
            audio_music(NULL, NULL);
            return a == A_CONTINUE ? UI_CONTINUE : UI_NEW_GAME;
        }
        break;
    }
    }
    return UI_NONE;
}

void ui_title_draw(GSGLOBAL *gs, float sx, float sy)
{
    if (have_logo) {
        float py = 120.0f - 0.125f * logo_frames + shake_mag;
        float alpha = logo_frames >= 120 ? 1.0f : logo_frames / 120.0f;
        int a = (int)(alpha * 128.0f);
        float x = 240.0f - logo.Width / 2, y = (float)(int)py - logo.Height / 2;
        gsKit_prim_sprite_texture(gs, &logo, x * sx, y * sy, 0, 0, (x + logo.Width) * sx, (y + logo.Height) * sy,
                                  logo.Width, logo.Height, 5, GS_SETREG_RGBAQ(0x80, 0x80, 0x80, a, 0));
    }
    int show = 0;
    if (phase == P_PRESS)   show = (t / 30) % 2 == 0;
    if (phase == P_PRESSED) show = (t / 4) % 2 == 0;
    if (show) {
        float w = font_width(&menufont, text[T_PRESS_START]);
        font_draw(gs, &menufont, text[T_PRESS_START], 240.0f - w / 2.0f, 230.0f - menufont.line / 2.0f, sx, sy, WHITE);
    }
    if (phase == P_MENU) {
        font_draw(gs, &menufont, text[T_COPYRIGHT], 337.0f, 30.0f + 0.5f * menufont.line, sx, sy, GRAY);
        float sw = font_width(&menufont, text[T_SAVED]);
        font_draw(gs, &menufont, text[T_SAVED], 462.0f - sw, 352.0f - menufont.line, sx, sy, GRAY);
        if (has_save) {                       /* CreateStatsDisplay: a la derecha, a y=160 */
            static menu box;
            box.f = BASE; box.f.by = 160; box.n = 0; box.sel = -1;
            update_stats_text();
            add(&box, info(text[T_NEW_GAME], GRAY, 0));
            for (int i = 0; i < 4; i++) add(&box, info(stat_lines[i], GRAY, 0));
            draw_menu(gs, &box, sx, sy);
        }
        draw_stack(gs, sx, sy);
    }
}

/* ---- cinematicas ---- */

static int logo_t;

/* NexileLogoScreen: logo centrado sobre blanco, 2 s o hasta pulsar */
int ui_logo_update(int pressed)
{
    logo_t++;
    return logo_t >= 120 || (logo_t > 10 && (pressed & (UI_OK | UI_START)));
}

void ui_logo_draw(GSGLOBAL *gs, float sx, float sy)
{
    gsKit_prim_sprite(gs, 0, 0, JK_SCREEN_W * sx, JK_SCREEN_H * sy, 4, GS_SETREG_RGBAQ(0x80, 0x80, 0x80, 0x80, 0));
    if (have_nexile) {
        float x = 240.0f - nexile.Width / 2.0f, y = 180.0f - nexile.Height / 2.0f;
        gsKit_prim_sprite_texture(gs, &nexile, x * sx, y * sy, 0, 0, (x + nexile.Width) * sx, (y + nexile.Height) * sy,
                                  nexile.Width, nexile.Height, 5, WHITE);
    }
}

/* IntroState: musica de apertura, 1 s, texto de la leyenda (aparece 1 s, queda 3 s, se va 1 s),
 * la pantalla negra se aclara en 1 s y 2 s de pausa. Con carga rapida: tiempos cortos. */
static float it_t;
static int it_fast;

void ui_intro_enter(void)
{
    it_t = 0;
    it_fast = opt_quickload;
    if (!it_fast) audio_music("opening", "");
}

static float it_pre(void)   { return it_fast ? 0.0f : 1.0f; }
static float it_in(void)    { return it_fast ? 0.5f : 1.0f; }
static float it_stay(void)  { return it_fast ? 1.0f : 3.0f; }
static float it_out(void)   { return it_fast ? 0.5f : 1.0f; }
static float it_text_end(void) { return it_pre() + it_in() + it_stay() + it_out(); }
static float it_end(void)   { return it_text_end() + 1.0f + (it_fast ? 0.0f : 2.0f); }

int ui_intro_update(void)
{
    it_t += JK_DT;
    return it_t >= it_end();
}

void ui_intro_draw(GSGLOBAL *gs, float sx, float sy)
{
    /* fundido negro encima del juego: opaco hasta que acaba el texto, luego se aclara en 1 s */
    float black = 1.0f;
    if (it_t > it_text_end()) black = 1.0f - (it_t - it_text_end());
    if (black < 0) black = 0;
    if (black > 0)
        gsKit_prim_sprite(gs, 0, 0, JK_SCREEN_W * sx, JK_SCREEN_H * sy, 6,
                          GS_SETREG_RGBAQ(0, 0, 0, (int)(black * 128.0f), 0));
    /* texto centrado (FadeTextEntity) */
    float t = it_t - it_pre(), a = 0;
    if (t >= 0 && t < it_in()) a = t / it_in();
    else if (t >= it_in() && t < it_in() + it_stay()) a = 1;
    else if (t >= it_in() + it_stay() && t < it_in() + it_stay() + it_out()) a = 1 - (t - it_in() - it_stay()) / it_out();
    if (a > 0) {
        float w, h;
        text_size(&menufont, text[T_INTRO], &w, &h);
        u64 c = GS_SETREG_RGBAQ(0x80, 0x80, 0x80, (int)(a * 128.0f), 0);
        draw_text_ml(gs, &menufont, text[T_INTRO], 240.0f - w / 2.0f, 180.0f - h / 2.0f, sx, sy, c);
    }
}

/* ---- creditos (GameEnding) ---- */

#define MAX_ROLL 16
static char roll_text[MAX_ROLL][8][96];
static int roll_lines[MAX_ROLL], roll_n, roll_i;
static float roll_t, roll_end_t;

void ui_credits_enter(void)
{
    int n;
    roll_n = 0; roll_i = 0; roll_t = 0; roll_end_t = -1;
    unsigned char *b = assets_load("roll.bin", &n);
    if (!b || memcmp(b, "JKRL", 4) != 0) return;
    int count = b[4], p = 5;
    for (int i = 0; i < count && roll_n < MAX_ROLL; i++) {
        int l = b[p++], k = 0;
        memcpy(roll_text[roll_n][0], b + p, l > 95 ? 95 : l); roll_text[roll_n][0][l > 95 ? 95 : l] = 0; p += l;
        int ppl = b[p++];
        for (int j = 0; j < ppl; j++) {
            l = b[p++];
            if (k < 7) { memcpy(roll_text[roll_n][1 + k], b + p, l > 95 ? 95 : l); roll_text[roll_n][1 + k][l > 95 ? 95 : l] = 0; k++; }
            p += l;
        }
        roll_lines[roll_n++] = 1 + k;
    }
    /* port credits */
    if (roll_n < MAX_ROLL) {
        strcpy(roll_text[roll_n][0], "PORT PARA PLAYSTATION 2");
        strcpy(roll_text[roll_n][1], "Aroy 24");
        roll_lines[roll_n++] = 2;
    }
}

/* tiempos de cada entrada: aparece 1 s, queda 3*(10/n)+2*(10/n)-2 s, se va 1 s */
static float roll_stay(void) { return roll_n ? 5.0f * (10.0f / roll_n) - 2.0f : 0; }

int ui_credits_update(int any_input)
{
    if (any_input && roll_end_t < 0) roll_end_t = 0;               /* SkipCredits */
    if (roll_end_t >= 0) { roll_end_t += JK_DT; return roll_end_t >= 5.0f || any_input; }  /* fundido 3 s + 2 s */
    roll_t += JK_DT;
    if (roll_t >= 1.0f + roll_stay() + 1.0f) { roll_t = 0; if (++roll_i >= roll_n) roll_end_t = 0; }
    return 0;
}

void ui_credits_draw(GSGLOBAL *gs, float sx, float sy)
{
    gsKit_prim_sprite(gs, 0, 0, JK_SCREEN_W * sx, JK_SCREEN_H * sy, 6, GS_SETREG_RGBAQ(0, 0, 0, 0x80, 0));
    if (roll_i >= roll_n || roll_end_t >= 0) return;
    float a = roll_t < 1.0f ? roll_t : roll_t < 1.0f + roll_stay() ? 1.0f : 1.0f - (roll_t - 1.0f - roll_stay());
    if (a <= 0) return;
    u64 c = GS_SETREG_RGBAQ(0x80, 0x80, 0x80, (int)(a * 128.0f), 0);
    float y = 255.0f;
    for (int j = 0; j < roll_lines[roll_i]; j++) {
        float w = font_width(&menufont, roll_text[roll_i][j]);
        font_draw(gs, &menufont, roll_text[roll_i][j], 240.0f - w / 2, y - menufont.line / 2.0f, sx, sy, c);
        y += menufont.line + (j == 0 ? 4 : 0);
    }
}

/* ---- estadisticas (StatsScreen) ---- */

static char stats_screen[4][128];
static int stats_t;

void ui_stats_enter(void)
{
    char a[16], b2[16];
    update_stats_text();
    snprintf(a, sizeof a, "%d", st_attempt); snprintf(b2, sizeof b2, "%d", st_session);
    fmt2(stats_screen[0], sizeof stats_screen[0], text[T_COMPLETED], a, b2);
    strcpy(stats_screen[1], stat_lines[1]);
    strcpy(stats_screen[2], stat_lines[2]);
    strcpy(stats_screen[3], stat_lines[3]);
    stats_t = 0;
}

int ui_stats_update(int any_input) { stats_t++; return stats_t > 30 && any_input; }

void ui_stats_draw(GSGLOBAL *gs, float sx, float sy)
{
    gsKit_prim_sprite(gs, 0, 0, JK_SCREEN_W * sx, JK_SCREEN_H * sy, 6, GS_SETREG_RGBAQ(0, 0, 0, 0x80, 0));
    float y = 180.0f - 50.0f;
    for (int i = 0; i < 4; i++) {
        float w = font_width(&menufont, stats_screen[i]);
        font_draw(gs, &menufont, stats_screen[i], 240.0f - w / 2, y - menufont.line / 2.0f, sx, sy, WHITE);
        y += menufont.line + 4;
    }
}

/* ---- pausa ---- */

void ui_pause_enter(void)
{
    stack[0] = M_PAUSE; depth = 1; menus[M_PAUSE].sel = 0;
    audio_play_sfx(SFX_MENU_OPEN);
}

int ui_pause_update(int pressed)
{
    if (depth == 1 && (pressed & (UI_START | UI_BACK))) return UI_RESUME;
    int a = menu_input(pressed);
    if (a == A_RESUME) return UI_RESUME;
    if (a == A_EXIT_CONFIRM) { push(M_CONFIRM); menus[M_CONFIRM].sel = 1; }
    if (a == A_EXIT) return UI_EXIT_TO_TITLE;
    return UI_NONE;
}

void ui_pause_draw(GSGLOBAL *gs, float sx, float sy)
{
    draw_stack(gs, sx, sy);
}
