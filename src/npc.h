/* NPC del juego (OldManEntity / MerchantEntity): animacion, zona de activacion y dialogos.
 * Datos de assets_ps2/npc.bin + npcN.t8 + font.*, generados desde la copia del usuario. */
#ifndef NPC_H
#define NPC_H
#include <gsKit.h>
#include "jk/king.h"

int  npc_init(GSGLOBAL *gs);
void npc_update(GSGLOBAL *gs, const jk_king *k, float dt);
void npc_draw(GSGLOBAL *gs, int screen, float sx, float sy);
void npc_draw_bubbles(GSGLOBAL *gs, int screen, float sx, float sy);
int  npc_take_talk_blip(void);     /* 1 si toca sonar una letra este frame */
#endif
