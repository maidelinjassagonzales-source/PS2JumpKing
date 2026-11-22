/* Pantalla de titulo y menu de pausa (GameTitleScreen / PauseManager, simplificados).
 * Logo, marco, cursor, textos y musica salen de la copia del usuario (tools/build_ui.py). */
#ifndef UI_H
#define UI_H
#include <gsKit.h>

enum { UI_UP = 1, UI_DOWN = 2, UI_OK = 4, UI_BACK = 8, UI_START = 16 };
enum { UI_NONE, UI_NEW_GAME, UI_CONTINUE, UI_RESUME, UI_EXIT_TO_TITLE };

int  ui_init(GSGLOBAL *gs);
void ui_title_enter(int has_save);
int  ui_title_update(int pressed);                 /* devuelve UI_NEW_GAME / UI_CONTINUE */
void ui_title_draw(GSGLOBAL *gs, float sx, float sy);
void ui_pause_enter(void);
int  ui_pause_update(int pressed);                 /* UI_RESUME / UI_EXIT_TO_TITLE */
void ui_pause_draw(GSGLOBAL *gs, float sx, float sy);
#endif
