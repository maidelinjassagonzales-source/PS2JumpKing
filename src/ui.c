/* Titulo y pausa con la composicion del original:
 * - GameTitleScreen: logo (LogoEntity: aparece en 2 s subiendo 15 px, golpe y temblor), espera,
 *   "pulsa X" centrado (PressStartEntity), parpadeo rapido 0.7 s, menu.
 * - Menus: GuiFormat de PauseManager (margen 16, separacion 8, relleno 16), marco GuiFrame de
 *   3x3 trozos, cursor a la izquierda, la opcion elegida se desplaza 5 px, fuente MenuFont.
 *   Titulo: anclado a la izquierda a y=160. Pausa: arriba a la derecha. Confirmacion: centrada. */
#include <stdio.h>
#include <string.h>
#include "ui.h"
#include "font.h"
#include "assets.h"
#include "audio.h"
#include "jk/values.h"

enum { T_PRESS_START, T_CONTINUE, T_NEW_GAME, T_RESUME, T_SAVE_EXIT, T_SURE, T_YES, T_NO, T_COUNT };

#define WHITE 0x80808080
#define RED   GS_SETREG_RGBAQ(0x80, 0x00, 0x00, 0x80, 0)

static char text[T_COUNT][256];
static GSTEXTURE logo, frame, cursor;
static int have_logo, have_frame, have_cursor;
static jk_font menufont;

/* ---- formato de menu (GuiFormat) ---- */

typedef struct { int bx, by, bw, bh; float ax, ay; } gui_format;
static const gui_format PAUSE_FMT = { 0, 0, 480, 360, 1.0f, 0.0f };

typedef struct { int id; int selectable; u64 color; } item;

static float item_w(const item *it) { return font_width(&menufont, text[it->id]); }
static float item_h(void) { return (float)menufont.line; }

/* GuiFormat.CalculateBounds */
static void bounds(const gui_format *f, const item *it, int n, int *x, int *y, int *w, int *h)
{
    int bx = f->bx + 16, by = f->by + 16, bw = f->bw - 32, bh = f->bh - 32;
    float mw = 0, mh = 0;
    for (int i = 0; i < n; i++) { if (item_w(&it[i]) > mw) mw = item_w(&it[i]); mh += item_h(); }
    *w = (int)mw + 32;
    *h = (int)mh + 8 * (n - 1) + 32;
    if (*w > bw) *w = bw;
    *x = (int)((float)bx + bw * f->ax) - (int)(*w * f->ax);
    *y = (int)((float)by + bh * f->ay) - (int)(*h * f->ay);
}

/* GuiFrame: esquinas de 16 px fijas, bordes y centro estirados */
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

/* MenuSelector.Draw -> GuiFormat.DrawMenuItemsWithCursor */
static void draw_menu(GSGLOBAL *gs, const gui_format *f, const item *it, int n, int sel, float sx, float sy)
{
    int x, y, w, h;
    bounds(f, it, n, &x, &y, &w, &h);
    draw_frame(gs, x, y, w, h, sx, sy);
    float off = 16.0f;
    for (int i = 0; i < n; i++) {
        float tx = x + 16.0f + (i == sel ? 5.0f : 0.0f);
        font_draw(gs, &menufont, text[it[i].id], tx, y + off, sx, sy, it[i].color);
        if (i == sel && it[i].selectable && have_cursor)
            gsKit_prim_sprite_texture(gs, &cursor, x * sx, (y + off) * sy, 0, 0,
                                      (x + cursor.Width) * sx, (y + off + cursor.Height) * sy,
                                      cursor.Width, cursor.Height, 6, WHITE);
        off += item_h() + 8.0f;
    }
}

static int move_sel(const item *it, int n, int sel, int dir)
{
    for (int k = 0; k < n; k++) {
        sel = (sel + dir + n) % n;
        if (it[sel].selectable) break;
    }
    audio_play_sfx(SFX_SELECT);
    return sel;
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
    memset(&logo, 0, sizeof logo); memset(&frame, 0, sizeof frame); memset(&cursor, 0, sizeof cursor);
    have_logo = assets_t8(gs, "title.t8", &logo, 0) == 0;
    have_frame = assets_t8(gs, "frame.t8", &frame, 0) == 0;
    have_cursor = assets_t8(gs, "cursor.t8", &cursor, 0) == 0;
    font_load(gs, &menufont, "menufont");
    return 0;
}

/* ---- titulo ---- */

enum { P_LOGO, P_WAIT, P_PRESS, P_PRESSED, P_MENU };
static int phase, t, logo_frames, shake_tick, shake_skip, shake_mag, hit_played;
static int has_save, title_sel;
static item title_items[2];
static int title_n;

void ui_title_enter(int save)
{
    phase = P_LOGO; t = 0; logo_frames = 0;
    shake_tick = -1; shake_skip = 0; shake_mag = 0; hit_played = 0;
    has_save = save;
    title_n = 0;
    if (has_save) title_items[title_n++] = (item){ T_CONTINUE, 1, WHITE };
    title_items[title_n++] = (item){ T_NEW_GAME, 1, WHITE };
    title_sel = 0;
    audio_music("menu_intro", "menu_loop");
}

/* Shake: magnitud = tick con signo alterno, baja 1 cada 4 frames desde 10 */
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
    /* logo: 2 s subiendo 0.125 px/frame, luego golpe + temblor */
    if (logo_frames < 120) {
        logo_frames++;
        if (pressed & (UI_OK | UI_START)) logo_frames = 120;
        if (logo_frames == 120 && !hit_played) { hit_played = 1; audio_play_sfx(SFX_TITLE_HIT); shake_tick = 10; }
    }
    shake_update();

    switch (phase) {
    case P_LOGO:                                    /* PlayLogo: hasta 2.5 s o pulsar */
        if (t >= 150 || (pressed & (UI_OK | UI_START))) { phase = P_WAIT; t = 0; }
        break;
    case P_WAIT:                                    /* 1 s o pulsar */
        if (t >= 60 || (pressed & (UI_OK | UI_START))) { phase = P_PRESS; t = 0; }
        break;
    case P_PRESS:
        if (pressed & (UI_OK | UI_START)) { phase = P_PRESSED; t = 0; audio_play_sfx(SFX_PRESS_START); }
        break;
    case P_PRESSED:                                 /* parpadeo rapido 0.7 s */
        if (t >= 42) { phase = P_MENU; t = 0; }
        break;
    case P_MENU:
        if (pressed & UI_UP)   title_sel = move_sel(title_items, title_n, title_sel, -1);
        if (pressed & UI_DOWN) title_sel = move_sel(title_items, title_n, title_sel, 1);
        if (pressed & UI_OK) {
            audio_music(NULL, NULL);
            return title_items[title_sel].id == T_CONTINUE ? UI_CONTINUE : UI_NEW_GAME;
        }
        break;
    }
    return UI_NONE;
}

void ui_title_draw(GSGLOBAL *gs, float sx, float sy)
{
    if (have_logo) {
        float py = 120.0f - 0.125f * logo_frames + shake_mag;     /* MoveUp + Shake */
        float alpha = logo_frames >= 120 ? 1.0f : logo_frames / 120.0f;   /* FadeNode 2 s */
        int a = (int)(alpha * 128.0f);
        float x = 240.0f - logo.Width / 2, y = (float)(int)py - logo.Height / 2;
        gsKit_prim_sprite_texture(gs, &logo, x * sx, y * sy, 0, 0, (x + logo.Width) * sx, (y + logo.Height) * sy,
                                  logo.Width, logo.Height, 5, GS_SETREG_RGBAQ(0x80, 0x80, 0x80, a, 0));
    }
    int show = 0;
    if (phase == P_PRESS)   show = (t / 30) % 2 == 0;      /* 0.5 s */
    if (phase == P_PRESSED) show = (t / 4) % 2 == 0;       /* 1/15 s */
    if (show) {
        float w = font_width(&menufont, text[T_PRESS_START]);
        font_draw(gs, &menufont, text[T_PRESS_START], 240.0f - w / 2.0f, 230.0f - menufont.line / 2.0f, sx, sy, WHITE);
    }
    if (phase == P_MENU) {
        gui_format f = PAUSE_FMT;
        f.ax = 0.0f;
        f.by = 160;
        draw_menu(gs, &f, title_items, title_n, title_sel, sx, sy);
    }
}

/* ---- pausa ---- */

static int pause_sel, confirm, confirm_sel;
static const item pause_items[2] = { { T_RESUME, 1, WHITE }, { T_SAVE_EXIT, 1, WHITE } };
static item confirm_items[3];

void ui_pause_enter(void)
{
    pause_sel = 0; confirm = 0;
    audio_play_sfx(SFX_MENU_OPEN);
}

int ui_pause_update(int pressed)
{
    if (confirm) {
        if (pressed & UI_UP)   confirm_sel = move_sel(confirm_items, 3, confirm_sel, -1);
        if (pressed & UI_DOWN) confirm_sel = move_sel(confirm_items, 3, confirm_sel, 1);
        if (pressed & UI_BACK) confirm = 0;
        if (pressed & UI_OK) {
            if (confirm_items[confirm_sel].id == T_YES) return UI_EXIT_TO_TITLE;
            confirm = 0;
        }
        return UI_NONE;
    }
    if (pressed & (UI_START | UI_BACK)) return UI_RESUME;
    if (pressed & UI_UP)   pause_sel = move_sel(pause_items, 2, pause_sel, -1);
    if (pressed & UI_DOWN) pause_sel = move_sel(pause_items, 2, pause_sel, 1);
    if (pressed & UI_OK) {
        if (pause_sel == 0) return UI_RESUME;
        /* CreateExitToMenu: "seguro?" en rojo, No, Si */
        confirm_items[0] = (item){ T_SURE, 0, RED };
        confirm_items[1] = (item){ T_NO, 1, WHITE };
        confirm_items[2] = (item){ T_YES, 1, WHITE };
        confirm = 1; confirm_sel = 1;
    }
    return UI_NONE;
}

void ui_pause_draw(GSGLOBAL *gs, float sx, float sy)
{
    draw_menu(gs, &PAUSE_FMT, pause_items, 2, pause_sel, sx, sy);
    if (confirm) {
        gui_format f = PAUSE_FMT;
        f.ax = 0.5f; f.ay = 0.5f; f.bx = 32;
        draw_menu(gs, &f, confirm_items, 3, confirm_sel, sx, sy);
    }
}
