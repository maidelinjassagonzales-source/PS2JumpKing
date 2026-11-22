/* Jump King PS2 - bucle principal: entrada del mando, logica a 60 Hz, dibujo con gsKit.
 * Graficos provisionales: rectangulos de color (sin assets del juego). */
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <libpad.h>
#include <gsKit.h>
#include <dmaKit.h>
#include "jk/values.h"
#include "jk/level.h"
#include "jk/king.h"

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

/* Lee los dos puertos: cruceta o stick izquierdo para andar, X para saltar. */
static jk_input pad_read(void)
{
    jk_input in = {0, 0, 0};
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
        int lx = 0;
        if ((st.mode >> 4) == 0x7)          /* DualShock en analogico */
            lx = (int)st.ljoy_h - 128;
        in.left  |= (b & PAD_LEFT) != 0  || lx < -77;
        in.right |= (b & PAD_RIGHT) != 0 || lx >  77;
        in.jump  |= (b & PAD_CROSS) != 0;
    }
    return in;
}

static void draw_rect(GSGLOBAL *gs, int x, int y, int w, int h, int screen, u64 c)
{
    float sy = (float)(y + screen * JK_SCREEN_H);
    float x0 = VIEW_X + (float)x, y0 = VIEW_Y + sy;
    float x1 = x0 + (float)w,     y1 = y0 + (float)h;
    if (y1 < VIEW_Y || y0 > VIEW_Y + JK_SCREEN_H) return;
    if (y0 < VIEW_Y) y0 = VIEW_Y;
    if (y1 > VIEW_Y + JK_SCREEN_H) y1 = VIEW_Y + JK_SCREEN_H;
    if (x0 < VIEW_X) x0 = VIEW_X;
    if (x1 > VIEW_X + JK_SCREEN_W) x1 = VIEW_X + JK_SCREEN_W;
    gsKit_prim_sprite(gs, x0, y0, x1, y1, 1, c);
}

int main(void)
{
    pad_init();
    jk_values_init();

    GSGLOBAL *gs = gsKit_init_global();
    gs->PrimAlphaEnable = GS_SETTING_OFF;
    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC,
                D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);
    gsKit_init_screen(gs);
    gsKit_mode_switch(gs, GS_ONESHOT);

    const u64 border = GS_SETREG_RGBAQ(0x00, 0x00, 0x00, 0x80, 0x00);
    const u64 sky    = GS_SETREG_RGBAQ(0x30, 0x40, 0x60, 0x80, 0x00);
    const u64 wall   = GS_SETREG_RGBAQ(0x70, 0x68, 0x60, 0x80, 0x00);
    const u64 king   = GS_SETREG_RGBAQ(0x40, 0x60, 0xE0, 0x80, 0x00);
    const u64 knock  = GS_SETREG_RGBAQ(0xE0, 0x80, 0x30, 0x80, 0x00);
    const u64 splat  = GS_SETREG_RGBAQ(0xE0, 0x30, 0x30, 0x80, 0x00);
    const u64 bar    = GS_SETREG_RGBAQ(0xF0, 0xF0, 0x40, 0x80, 0x00);

    const jk_level *lv = &jk_test_level;
    jk_king k;
    jk_king_init(&k, 240.0f, 200.0f);   /* posicion inicial del original */

    for (;;) {
        jk_king_update(&k, lv, pad_read());

        gsKit_clear(gs, border);
        gsKit_prim_sprite(gs, VIEW_X, VIEW_Y, VIEW_X + JK_SCREEN_W, VIEW_Y + JK_SCREEN_H, 1, sky);
        for (int s = 0; s < lv->count; s++)
            for (int i = 0; i < lv->screens[s].count; i++) {
                const jk_rect *r = &lv->screens[s].blocks[i];
                draw_rect(gs, r->x, r->y, r->w, r->h, k.screen, wall);
            }
        jk_rect hb = jk_body_hitbox(&k.body);
        u64 c = k.splatted ? splat : (k.body.knocked ? knock : king);
        draw_rect(gs, hb.x, hb.y, hb.w, hb.h, k.screen, c);
        /* barra de carga del salto */
        if (k.charge > 0.0f)
            gsKit_prim_sprite(gs, VIEW_X, VIEW_Y + JK_SCREEN_H + 6,
                              VIEW_X + JK_SCREEN_W * (k.charge / JK.charge_time),
                              VIEW_Y + JK_SCREEN_H + 14, 1, bar);

        gsKit_queue_exec(gs);
        gsKit_sync_flip(gs);
    }
    return 0;
}
