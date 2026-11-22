#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <audsrv.h>
#include <sbv_patches.h>
#include "audio.h"
#include "assets.h"

extern unsigned char audsrv_irx[];
extern unsigned int size_audsrv_irx;

#define RATE      22050
#define CHUNK     1024                  /* frames estereo por bloque mezclado */
#define MAX_TRACKS 69
#define MAX_PLAY   6

typedef struct { int restart; float fade_in, fade_out; } track_info;
typedef struct { unsigned short id; float vol; } amb_entry;
typedef struct { unsigned char n; amb_entry e[4]; } amb_screen;

typedef struct {
    int    id;          /* -1 libre */
    FILE  *f;
    unsigned frames, pos;
    float  vol, target, rate_in, rate_out;
} stream;

typedef struct { short *data; unsigned frames; } sfx;
typedef struct { int id; unsigned pos; } voice;

static int ok;
static int track_count, screen_count, cur_screen = -1;
static track_info tracks[MAX_TRACKS];
static amb_screen *screens;
static stream play[MAX_PLAY];
static sfx sfxs[SFX_COUNT];
static voice voices[8];
static short mixbuf[CHUNK * 2];
static short readbuf[CHUNK * 2];

static int open_path(stream *s, const char *name)
{
    s->f = fopen(name, "rb");
    if (!s->f) return -1;
    unsigned char h[16];
    if (fread(h, 1, 16, s->f) != 16 || memcmp(h, "JKPC", 4) != 0) { fclose(s->f); s->f = NULL; return -1; }
    s->frames = h[12] | h[13] << 8 | h[14] << 16 | (unsigned)h[15] << 24;
    s->pos = 0;
    return 0;
}

static int open_track(stream *s, int id)
{
    char name[40];
    snprintf(name, sizeof name, "host:assets_ps2/audio/amb/%03d.pcm", id);
    return open_path(s, name);
}

/* Musica de menu: un stream aparte; al acabar la intro pasa al loop. */
static stream music;
static char music_loop[64];
static int music_on;

void audio_music(const char *intro, const char *loop)
{
    if (music_on && music.f) fclose(music.f);
    music_on = 0;
    music.f = NULL;
    if (!intro) return;
    /* el ambiente se calla mientras suena el menu */
    for (int i = 0; i < MAX_PLAY; i++)
        if (play[i].id >= 0) { fclose(play[i].f); play[i].f = NULL; play[i].id = -1; }
    cur_screen = -1;
    char path[64];
    snprintf(path, sizeof path, "host:assets_ps2/audio/%s.pcm", intro);
    snprintf(music_loop, sizeof music_loop, "host:assets_ps2/audio/%s.pcm", loop ? loop : intro);
    if (open_path(&music, path) == 0) music_on = 1;
}

static void load_sfx(int i, const char *name)
{
    int n;
    unsigned char *b = assets_load(name, &n);
    if (!b || memcmp(b, "JKPC", 4) != 0) return;
    sfxs[i].frames = b[12] | b[13] << 8 | b[14] << 16 | (unsigned)b[15] << 24;
    sfxs[i].data = (short *)(b + 16);
}

int audio_init(void)
{
    int ret = 0;
    sbv_patch_enable_lmb();             /* permite cargar modulos del IOP desde la memoria del EE */
    if (SifLoadModule("rom0:LIBSD", 0, NULL) < 0
        || SifExecModuleBuffer(audsrv_irx, size_audsrv_irx, 0, NULL, &ret) < 0
        || audsrv_init() != 0) {
        printf("[jk] no se pudo iniciar el sonido\n");
        return -1;
    }
    struct audsrv_fmt_t fmt = { RATE, 16, 2 };
    audsrv_set_format(&fmt);
    audsrv_set_volume(MAX_VOLUME);

    int n;
    unsigned char *b = assets_load("audio/ambience.bin", &n);
    if (b && memcmp(b, "JKAM", 4) == 0) {
        track_count = b[4] | b[5] << 8;
        screen_count = b[6] | b[7] << 8;
        if (track_count > MAX_TRACKS) track_count = MAX_TRACKS;
        unsigned char *p = b + 8;
        for (int i = 0; i < track_count; i++, p += 12) {
            memcpy(&tracks[i].restart, p, 4);
            memcpy(&tracks[i].fade_in, p + 4, 4);
            memcpy(&tracks[i].fade_out, p + 8, 4);
        }
        screens = calloc(screen_count, sizeof *screens);
        for (int s = 0; s < screen_count; s++) {
            screens[s].n = *p++;
            for (int k = 0; k < 4; k++, p += 6) {
                screens[s].e[k].id = p[0] | p[1] << 8;
                memcpy(&screens[s].e[k].vol, p + 2, 4);
            }
        }
    }
    for (int i = 0; i < MAX_PLAY; i++) play[i].id = -1;
    for (int i = 0; i < 8; i++) voices[i].id = -1;
    load_sfx(SFX_JUMP, "audio/sfx_jump.pcm");
    load_sfx(SFX_LAND, "audio/sfx_land.pcm");
    load_sfx(SFX_BUMP, "audio/sfx_bump.pcm");
    load_sfx(SFX_SPLAT, "audio/sfx_splat.pcm");
    load_sfx(SFX_TALK, "audio/sfx_talk.pcm");
    ok = 1;
    return 0;
}

static stream *find_stream(int id)
{
    for (int i = 0; i < MAX_PLAY; i++) if (play[i].id == id) return &play[i];
    return NULL;
}

/* Velocidad de fundido por bloque: fade 0 = instantaneo. */
static float fade_step(float seconds)
{
    return seconds <= 0.0f ? 2.0f : (float)CHUNK / (RATE * seconds);
}

void audio_set_screen(int screen)
{
    if (!ok || !screens || screen == cur_screen) return;
    /* pistas de la pantalla anterior: a volumen 0 */
    for (int i = 0; i < MAX_PLAY; i++)
        if (play[i].id >= 0) play[i].target = 0.0f;
    /* pistas de la nueva: suben a su volumen; las que ya sonaban siguen donde iban */
    if (screen >= 0 && screen < screen_count)
        for (int k = 0; k < screens[screen].n; k++) {
            amb_entry e = screens[screen].e[k];
            stream *s = find_stream(e.id);
            if (!s) {
                s = find_stream(-1);
                if (!s || open_track(s, e.id) != 0) continue;
                s->id = e.id;
                s->vol = 0.0f;
            }
            s->target = e.vol;
            s->rate_in = fade_step(tracks[e.id].fade_in);
            s->rate_out = fade_step(tracks[e.id].fade_out);
        }
    cur_screen = screen;
}

void audio_play_sfx(int id)
{
    if (!ok || id < 0 || id >= SFX_COUNT || !sfxs[id].data) return;
    for (int i = 0; i < 8; i++)
        if (voices[i].id < 0) { voices[i].id = id; voices[i].pos = 0; return; }
}

static inline int clamp16(int v) { return v > 32767 ? 32767 : v < -32768 ? -32768 : v; }

static void mix_chunk(void)
{
    int acc[CHUNK * 2];
    memset(acc, 0, sizeof acc);
    for (int i = 0; i < MAX_PLAY; i++) {
        stream *s = &play[i];
        if (s->id < 0) continue;
        if (s->vol < s->target) { s->vol += s->rate_in;  if (s->vol > s->target) s->vol = s->target; }
        if (s->vol > s->target) { s->vol -= s->rate_out; if (s->vol < s->target) s->vol = s->target; }
        if (s->vol <= 0.0f && s->target <= 0.0f) {
            /* pista callada: las de "restart" se cierran; las demas tambien, para no gastar lectura */
            fclose(s->f); s->f = NULL; s->id = -1;
            continue;
        }
        int vol = (int)(s->vol * 256.0f);
        unsigned got = 0;
        while (got < CHUNK) {
            unsigned want = CHUNK - got;
            if (s->pos + want > s->frames) want = s->frames - s->pos;
            size_t r = fread(readbuf + got * 2, 4, want, s->f);
            got += r; s->pos += r;
            if (r < want || s->pos >= s->frames) { fseek(s->f, 16, SEEK_SET); s->pos = 0; }  /* bucle */
            if (r == 0 && want > 0) break;
        }
        for (unsigned j = 0; j < got * 2; j++) acc[j] += (readbuf[j] * vol) >> 8;
    }
    if (music_on) {
        unsigned got = 0;
        while (got < CHUNK && music_on) {
            unsigned want = CHUNK - got;
            if (music.pos + want > music.frames) want = music.frames - music.pos;
            size_t r = fread(readbuf + got * 2, 4, want, music.f);
            got += r; music.pos += r;
            if (music.pos >= music.frames || r == 0) {     /* fin: abrir el loop y seguir */
                fclose(music.f); music.f = NULL;
                if (open_path(&music, music_loop) != 0) music_on = 0;
            }
        }
        for (unsigned j = 0; j < got * 2; j++) acc[j] += readbuf[j];
    }
    for (int v = 0; v < 8; v++) {
        voice *vc = &voices[v];
        if (vc->id < 0) continue;
        const sfx *x = &sfxs[vc->id];
        for (int j = 0; j < CHUNK && vc->pos < x->frames; j++, vc->pos++) {
            acc[j * 2] += x->data[vc->pos];
            acc[j * 2 + 1] += x->data[vc->pos];
        }
        if (vc->pos >= x->frames) vc->id = -1;
    }
    for (int j = 0; j < CHUNK * 2; j++) mixbuf[j] = (short)clamp16(acc[j]);
}

void audio_update(void)
{
    if (!ok) return;
    while (audsrv_available() >= (int)sizeof mixbuf) {
        mix_chunk();
        audsrv_play_audio((char *)mixbuf, sizeof mixbuf);
    }
}
