/* Sonido: ambiente/musica por pantalla en streaming desde host: y efectos en RAM, mezclados en el EE. */
#ifndef AUDIO_H
#define AUDIO_H
enum { SFX_JUMP, SFX_LAND, SFX_BUMP, SFX_SPLAT, SFX_COUNT };
int  audio_init(void);              /* 0 = ok (sin datos convertidos queda en silencio) */
void audio_set_screen(int screen);  /* como AmbienceManager: para las pistas viejas, arranca las nuevas */
void audio_play_sfx(int id);
void audio_update(void);            /* llamar una vez por frame */
#endif
