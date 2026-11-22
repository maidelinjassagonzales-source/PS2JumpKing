#include <stdio.h>
#include <string.h>
#include "ui.h"
#include "npc.h"
#include "assets.h"
#include "audio.h"
#include "jk/values.h"

enum { T_PRESS_START, T_CONTINUE, T_NEW_GAME, T_RESUME, T_SAVE_EXIT, T_SURE, T_YES, T_NO, T_COUNT };

static char text[T_COUNT][256];
static GSTEXTURE logo, frame, cursor;
static int have_logo, have_frame, have_cursor;

/* estado */
static int title_phase, title_sel, title_has_save, blink;
static int pause_sel, pause_confirm, confirm_sel;

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
    return 0;
}

static float tw(int id) { return ui_text_width(text[id], strlen(text[id])); }

static void text_at(GSGLOBAL *gs, int id, float x, float y, float sx, float sy)
{
    ui_draw_text(gs, text[id], strlen(text[id]), x, y, sx, sy);
}

static void text_center(GSGLOBAL *gs, int id, float y, float sx, float sy)
{
    text_at(gs, id, (JK_SCREEN_W - tw(id)) / 2.0f, y, sx, sy);
}

/* Marco de menu: textura de 48x48 en 9 trozos de 16 px (GuiFrame). */
static void draw_frame(GSGLOBAL *gs, float x, float y, float w, float h, float sx, float sy)
{
    if (!have_frame) {
        gsKit_prim_sprite(gs, x * sx, y * sy, (x + w) * sx, (y + h) * sy, 5, GS_SETREG_RGBAQ(0, 0, 0, 0x70, 0));
        return;
    }
    float c = 16.0f, xs[4] = { x, x + c, x + w - c, x + w }, ys[4] = { y, y + c, y + h - c, y + h };
    float us[4] = { 0, 16, 32, 48 };
    for (int j = 0; j < 3; j++)
        for (int i = 0; i < 3; i++)
            gsKit_prim_sprite_texture(gs, &frame, xs[i] * sx, ys[j] * sy, us[i], us[j],
                                      xs[i + 1] * sx, ys[j + 1] * sy, us[i + 1], us[j + 1], 5, 0x80808080);
}

static void draw_menu(GSGLOBAL *gs, const int *items, int n, int sel, float cy, float sx, float sy)
{
    float w = 0;
    for (int i = 0; i < n; i++) if (tw(items[i]) > w) w = tw(items[i]);
    w += 56.0f;
    float h = n * 18.0f + 24.0f, x = (JK_SCREEN_W - w) / 2.0f, y = cy - h / 2.0f;
    draw_frame(gs, x, y, w, h, sx, sy);
    for (int i = 0; i < n; i++) {
        float ty = y + 12.0f + i * 18.0f;
        text_at(gs, items[i], x + 32.0f, ty, sx, sy);
        if (i == sel && have_cursor)
            gsKit_prim_sprite_texture(gs, &cursor, (x + 12.0f) * sx, (ty + 1.0f) * sy, 0, 0,
                                      (x + 12.0f + cursor.Width) * sx, (ty + 1.0f + cursor.Height) * sy,
                                      cursor.Width, cursor.Height, 6, 0x80808080);
    }
}

/* ---- titulo ---- */

void ui_title_enter(int has_save)
{
    title_phase = 0;
    title_sel = 0;
    title_has_save = has_save;
    audio_music("menu_intro", "menu_loop");
}

int ui_title_update(int pressed)
{
    blink++;
    if (title_phase == 0) {                       /* "pulsa X para empezar" */
        if (pressed & (UI_OK | UI_START)) { title_phase = 1; audio_play_sfx(SFX_LAND); }
        return UI_NONE;
    }
    int n = title_has_save ? 2 : 1;
    if (pressed & UI_UP)   title_sel = (title_sel + n - 1) % n;
    if (pressed & UI_DOWN) title_sel = (title_sel + 1) % n;
    if (pressed & (UI_OK | UI_START)) {
        audio_music(NULL, NULL);
        return (title_has_save && title_sel == 0) ? UI_CONTINUE : UI_NEW_GAME;
    }
    return UI_NONE;
}

void ui_title_draw(GSGLOBAL *gs, float sx, float sy)
{
    if (have_logo) {
        float x = (JK_SCREEN_W - logo.Width) / 2.0f, y = 40.0f;
        gsKit_prim_sprite_texture(gs, &logo, x * sx, y * sy, 0, 0, (x + logo.Width) * sx, (y + logo.Height) * sy,
                                  logo.Width, logo.Height, 5, 0x80808080);
    }
    if (title_phase == 0) {
        if ((blink / 30) % 2 == 0) text_center(gs, T_PRESS_START, 250.0f, sx, sy);
        return;
    }
    int items[2], n = 0;
    if (title_has_save) items[n++] = T_CONTINUE;
    items[n++] = T_NEW_GAME;
    draw_menu(gs, items, n, title_sel, 250.0f, sx, sy);
}

/* ---- pausa ---- */

void ui_pause_enter(void) { pause_sel = 0; pause_confirm = 0; confirm_sel = 0; }

int ui_pause_update(int pressed)
{
    if (pause_confirm) {                          /* "seguro?" no / si */
        if (pressed & (UI_UP | UI_DOWN)) confirm_sel ^= 1;
        if (pressed & UI_BACK) pause_confirm = 0;
        if (pressed & UI_OK) {
            if (confirm_sel == 1) return UI_EXIT_TO_TITLE;
            pause_confirm = 0;
        }
        return UI_NONE;
    }
    if (pressed & (UI_START | UI_BACK)) return UI_RESUME;
    if (pressed & (UI_UP | UI_DOWN)) pause_sel ^= 1;
    if (pressed & UI_OK) {
        if (pause_sel == 0) return UI_RESUME;
        pause_confirm = 1; confirm_sel = 0;
    }
    return UI_NONE;
}

void ui_pause_draw(GSGLOBAL *gs, float sx, float sy)
{
    gsKit_prim_sprite(gs, 0, 0, JK_SCREEN_W * sx, JK_SCREEN_H * sy, 4, GS_SETREG_RGBAQ(0, 0, 0, 0x40, 0));
    if (pause_confirm) {
        int items[2] = { T_NO, T_YES };
        text_center(gs, T_SURE, 140.0f, sx, sy);
        draw_menu(gs, items, 2, confirm_sel, 190.0f, sx, sy);
    } else {
        int items[2] = { T_RESUME, T_SAVE_EXIT };
        draw_menu(gs, items, 2, pause_sel, 180.0f, sx, sy);
    }
}
