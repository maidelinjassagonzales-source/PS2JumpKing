/* Jump King PS2 - punto de entrada. Por ahora: inicializa gsKit y dibuja un marcador.
 * La logica del juego se porta desde la descompilacion de JumpKing.exe (ver docs/). */
#include <gsKit.h>
#include <dmaKit.h>

int main(void)
{
    GSGLOBAL *gs = gsKit_init_global();
    gs->PrimAlphaEnable = GS_SETTING_OFF;

    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC,
                D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);

    gsKit_init_screen(gs);
    gsKit_mode_switch(gs, GS_ONESHOT);

    const u64 bg  = GS_SETREG_RGBAQ(0x10, 0x10, 0x20, 0x80, 0x00);
    const u64 box = GS_SETREG_RGBAQ(0xE0, 0xC0, 0x40, 0x80, 0x00);

    for (;;) {
        gsKit_clear(gs, bg);
        /* Hitbox del jugador: 18x26 (PlayerValues.PLAYER_WIDTH/HEIGHT), x2 en pantalla */
        gsKit_prim_sprite(gs, 302.0f, 198.0f, 302.0f + 36.0f, 198.0f + 52.0f, 1, box);
        gsKit_queue_exec(gs);
        gsKit_sync_flip(gs);
    }
    return 0;
}
