/* Sonido: ambiente/musica por pantalla en streaming desde host: y efectos en RAM, mezclados en el EE. */
#ifndef AUDIO_H
#define AUDIO_H
enum { SFX_JUMP, SFX_LAND, SFX_BUMP, SFX_SPLAT, SFX_TALK, SFX_TITLE_HIT, SFX_PRESS_START, SFX_SELECT, SFX_MENU_OPEN, SFX_MENU_FAIL, SFX_BIRD0, SFX_BIRD1, SFX_BIRD2, SFX_PLINK, SFX_BABE_JUMP, SFX_BABE_KISS, SFX_BABE_MOU, SFX_BABE_SCREAM, SFX_BABE_SURPRISED, SFX_COUNT };
int  audio_init(void);              /* 0 = ok (sin datos convertidos queda en silencio) */
void audio_set_screen(int screen);  /* como AmbienceManager: para las pistas viejas, arranca las nuevas */
void audio_play_sfx(int id);
void audio_update(void);
/* Musica suelta (menu): suena intro y luego loop en bucle; NULL para parar. Calla el ambiente. */
void audio_music(const char *intro, const char *loop);
/* Opciones de sonido (VolumeSlider / SoundToggle): volumen general 0..1 y tipos activos. */
void audio_set_options(float master, int music, int sfx, int ambience);            /* llamar una vez por frame */
#endif
