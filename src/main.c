/* Jump King PS2 - bucle principal: titulo, juego y pausa a 60 Hz con gsKit.
 * Los datos del juego se cargan de assets_ps2/ (generados desde la copia del usuario). */
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <libpad.h>
#include <gsKit.h>
#include <dmaKit.h>
#include "jk/values.h"
#include "jk/level.h"
#include "jk/king.h"
#include "assets.h"
#include "audio.h"
#include "npc.h"
#include "ui.h"
#include <stdio.h>
#include <time.h>
#include <string.h>

#define VIEW_X 80   /* 480x360 centrado en 640x448 */
#define VIEW_Y 44

static char pad_buf[2][256] __attribute__((aligned(64)));
static int  pad_mode_set[2];

static void pad_init(void)
{
    SifInitRpc(0);
    SifLoadModule("rom0:SIO2MAN", 0, NULL);
    SifLoadModule("rom0:PADMAN", 0, NULL);
    padInit(0);
    for (int p = 0; p < 2; p++)
        padPortOpen(p, 0, pad_buf[p]);
}

/* Botones de menu de este frame (UI_*), ya en flanco de pulsacion. */
static int pad_menu, pad_menu_prev;

/* Lee los dos puertos: cruceta o stick izquierdo para andar, X para saltar, Start para pausa. */
static jk_input pad_read(void)
{
    jk_input in = {0, 0, 0};
    int menu = 0;
    for (int p = 0; p < 2; p++) {
        struct padButtonStatus st;
        int s = padGetState(p, 0);
        if (s == PAD_STATE_DISCONN) { pad_mode_set[p] = 0; continue; }
        if (s != PAD_STATE_STABLE && s != PAD_STATE_FINDCTP1) continue;
        if (!pad_mode_set[p]) {
            padSetMainMode(p, 0, PAD_MMODE_DUALSHOCK, PAD_MMODE_LOCK);
            pad_mode_set[p] = 1;
            continue;
        }
        if (padRead(p, 0, &st) == 0) continue;
        u32 b = 0xFFFF ^ st.btns;
        int lx = 0, ly = 0;
        if ((st.mode >> 4) == 0x7) {        /* DualShock en analogico */
            lx = (int)st.ljoy_h - 128;
            ly = (int)st.ljoy_v - 128;
        }
        if ((b & PAD_UP) || ly < -77)   menu |= UI_UP;
        if ((b & PAD_DOWN) || ly > 77)  menu |= UI_DOWN;
        if (b & PAD_CROSS)              menu |= UI_OK;
        if (b & (PAD_CIRCLE | PAD_TRIANGLE)) menu |= UI_BACK;
        if (b & PAD_START)              menu |= UI_START;
        in.left  |= (b & PAD_LEFT) != 0  || lx < -77;
        in.right |= (b & PAD_RIGHT) != 0 || lx >  77;
        in.jump  |= (b & PAD_CROSS) != 0;
    }
    pad_menu = menu & ~pad_menu_prev;
    pad_menu_prev = menu;
    return in;
}

/* El juego es 480x360; se escala a toda la pantalla de 640x448. */
#define SX (640.0f / JK_SCREEN_W)
#define SY (448.0f / JK_SCREEN_H)

static void draw_rect(GSGLOBAL *gs, int x, int y, int w, int h, int screen, u64 c)
{
    float y0 = (float)(y + screen * JK_SCREEN_H) * SY, x0 = (float)x * SX;
    gsKit_prim_sprite(gs, x0, y0, x0 + w * SX, y0 + h * SY, 1, c);
}

/* Capas de la pantalla actual: fondo, medio, frente. Se recargan al cambiar de pantalla. */
static GSTEXTURE layer[3];
static int layer_ok[3], layer_vram[3], loaded_screen = -1;

static void load_screen_layers(GSGLOBAL *gs, int screen)
{
    static const char tags[3] = { 'b', 'm', 'f' };
    for (int i = 0; i < 3; i++) {
        char name[32];
        snprintf(name, sizeof name, "screens/%c%d.t8", tags[i], screen);
        layer_ok[i] = assets_t8(gs, name, &layer[i], layer_vram[i]) == 0;
        if (layer_ok[i]) layer_vram[i] = 1;
    }
    loaded_screen = screen;
}

static void draw_layer(GSGLOBAL *gs, int i)
{
    if (!layer_ok[i]) return;
    gsKit_prim_sprite_texture(gs, &layer[i], 0.0f, 0.0f, 0.0f, 0.0f, 640.0f, 448.0f,
                              (float)layer[i].Width, (float)layer[i].Height, 2, 0x80808080);
}

/* Hoja del rey: celdas de 48x48 en rejilla de 4x4 (orden de KingSpriteLayers.Regular). */
enum { F_IDLE, F_WALK1, F_WALK_SMEAR, F_WALK2, F_CHARGE, F_UP, F_FALL, F_BOUNCE, F_SPLAT, F_LOOKUP };
static const unsigned char frame_cell[][2] = {
    {0,0}, {1,0}, {2,0}, {3,0}, {0,1}, {1,1}, {2,1}, {0,2}, {3,1}, {0,3},
};

static int king_frame(const jk_king *k, jk_input in, float *walk_tick)
{
    const jk_body *b = &k->body;
    if (k->splatted) return F_SPLAT;
    if (k->charge > 0.0f) return F_CHARGE;
    if (!b->on_ground) {
        *walk_tick = 0;
        if (b->knocked) return F_BOUNCE;
        return b->vy < 0.0f ? F_UP : F_FALL;
    }
    if (!(in.left ^ in.right)) { *walk_tick = 0; return F_IDLE; }
    /* WalkAnim: quieto 10 ticks, borron 4, quieto 10, borron 4 */
    *walk_tick += 1.0f;
    if (*walk_tick > 28.0f) *walk_tick = 1.0f;
    if (*walk_tick > 24.0f) return F_WALK_SMEAR;
    if (*walk_tick > 14.0f) return F_WALK2;
    if (*walk_tick > 10.0f) return F_WALK_SMEAR;
    return F_WALK1;
}

static void draw_king(GSGLOBAL *gs, GSTEXTURE *sheet, const jk_king *k, int frame)
{
    /* Ancla abajo al centro en posicion + (9, 26) (PlayerEntity.Draw) */
    float ax = k->body.x + 9.0f, ay = k->body.y + 26.0f + (float)(k->screen * JK_SCREEN_H);
    float x0 = (ax - 24.0f) * SX, y0 = (ay - 48.0f) * SY;
    float u0 = frame_cell[frame][0] * 48.0f, v0 = frame_cell[frame][1] * 48.0f, u1 = u0 + 48.0f;
    if (k->facing < 0) { float t = u0; u0 = u1; u1 = t; }
    gsKit_prim_sprite_texture(gs, sheet, x0, y0, u0, v0, x0 + 48.0f * SX, y0 + 48.0f * SY,
                              u1, v0 + 48.0f, 3, 0x80808080);
}

int main(void)
{
    pad_init();
    jk_values_init();

    GSGLOBAL *gs = gsKit_init_global();
    /* VRAM: framebuffer doble de 16 bits y sin Z-buffer, para que quepan las 3 capas y el rey. */
    /* El original corre a 60 pasos por segundo y aqui hay un paso por refresco: forzar NTSC 60 Hz
     * (con BIOS PAL gsKit elegia 50 Hz y todo iba al 83 %). */
    gs->Mode = GS_MODE_NTSC;
    gs->Interlace = GS_INTERLACED;
    gs->Field = GS_FIELD;
    gs->Width = 640;
    gs->Height = 448;
    gs->PSM = GS_PSM_CT16;
    gs->ZBuffering = GS_SETTING_OFF;
    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC,
                D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);
    gsKit_init_screen(gs);
    gsKit_mode_switch(gs, GS_ONESHOT);
    gs->PrimAlphaEnable = GS_SETTING_ON;
    gsKit_set_primalpha(gs, GS_SETREG_ALPHA(0, 1, 0, 1, 0), 0);
    gsKit_set_test(gs, GS_ATEST_OFF);
    gsKit_set_test(gs, GS_ZTEST_OFF);

    const u64 border = GS_SETREG_RGBAQ(0x00, 0x00, 0x00, 0x80, 0x00);
    const u64 wall   = GS_SETREG_RGBAQ(0x70, 0x68, 0x60, 0x80, 0x00);
    const u64 kingc  = GS_SETREG_RGBAQ(0x40, 0x60, 0xE0, 0x80, 0x00);
    const u64 bar    = GS_SETREG_RGBAQ(0xF0, 0xF0, 0x40, 0x80, 0x00);

    /* Nivel real si se convirtieron los datos (make assets); si no, el de prueba. */
    jk_level real;
    const jk_level *lv = &jk_test_level;
    int n = 0;
    unsigned char *lvbin = assets_load("level.bin", &n);
    if (lvbin && jk_level_from_bin(&real, lvbin, n) == 0) lv = &real;
    GSTEXTURE sheet; memset(&sheet, 0, sizeof sheet);
    memset(layer, 0, sizeof layer);
    int have_sheet = assets_t8(gs, "king.t8", &sheet, 0) == 0;
    audio_init();
    npc_init(gs);

    ui_init(gs);

    jk_king k;
    jk_king_init(&k, 240.0f, 200.0f);   /* posicion inicial del original */
    float walk_tick = 0.0f;
    enum { ST_TITLE, ST_PLAY, ST_PAUSE } state = ST_TITLE;
    ui_title_enter(0);

    clock_t fps_t0 = clock();
    int fps_frames = 0, fps_reports = 0;
    for (;;) {
        /* medicion: frames de logica por segundo real (va al log de PCSX2) */
        if (++fps_frames == 300 && fps_reports < 20) {
            clock_t t1 = clock();
            printf("[jk] fps=%.2f\n", 300.0 * CLOCKS_PER_SEC / (double)(t1 - fps_t0));
            fps_t0 = t1; fps_frames = 0; fps_reports++;
        } else if (fps_frames == 300) fps_frames = 0;
        jk_input in = pad_read();
        if (state == ST_TITLE) {
            int r = ui_title_update(pad_menu);
            if (r == UI_NEW_GAME || r == UI_CONTINUE) {
                jk_king_init(&k, 240.0f, 200.0f);
                walk_tick = 0.0f;
                state = ST_PLAY;
                pad_menu_prev |= UI_OK;          /* que el X del menu no cuente como salto */
            }
            in.left = in.right = in.jump = 0;
        } else if (state == ST_PAUSE) {
            int r = ui_pause_update(pad_menu);
            if (r == UI_RESUME) state = ST_PLAY;
            if (r == UI_EXIT_TO_TITLE) { state = ST_TITLE; ui_title_enter(0); }
        } else if (pad_menu & UI_START) {
            state = ST_PAUSE;
            ui_pause_enter();
        }
        if (state == ST_PLAY) {
            jk_king_update(&k, lv, in);
            audio_set_screen(k.screen);
        } else {
            in.left = in.right = in.jump = 0;
        }
        if (lv->grid && k.screen != loaded_screen)
            load_screen_layers(gs, k.screen);
        if (k.events & JK_EV_JUMP)  audio_play_sfx(SFX_JUMP);
        if (k.events & JK_EV_LAND)  audio_play_sfx(SFX_LAND);
        if (k.events & JK_EV_BUMP)  audio_play_sfx(SFX_BUMP);
        if (k.events & JK_EV_SPLAT) audio_play_sfx(SFX_SPLAT);
        if (state == ST_PLAY) npc_update(gs, &k, JK_DT);
        if (npc_take_talk_blip()) audio_play_sfx(SFX_TALK);
        audio_update();

        gsKit_clear(gs, border);
        if (lv->grid) {
            draw_layer(gs, 0);
            draw_layer(gs, 1);
            npc_draw(gs, k.screen, SX, SY);
        } else {
            for (int s = 0; s < lv->count; s++)
                for (int i = 0; i < lv->screens[s].count; i++) {
                    const jk_rect *r = &lv->screens[s].blocks[i];
                    draw_rect(gs, r->x, r->y, r->w, r->h, k.screen, wall);
                }
        }
        int frame = king_frame(&k, in, &walk_tick);
        if (have_sheet) {                    /* en el titulo el rey espera de pie (FollyPlayer) */
            draw_king(gs, &sheet, &k, frame);
        } else {
            jk_rect hb = jk_body_hitbox(&k.body);
            draw_rect(gs, hb.x, hb.y, hb.w, hb.h, k.screen, kingc);
        }
        if (lv->grid) {
            draw_layer(gs, 2);
            npc_draw_bubbles(gs, k.screen, SX, SY);
        }
        if (state == ST_TITLE) ui_title_draw(gs, SX, SY);
        if (state == ST_PAUSE) ui_pause_draw(gs, SX, SY);
        (void)bar;

        gsKit_queue_exec(gs);
        gsKit_sync_flip(gs);
    }
    return 0;
}
