/* Final normal portado de la descompilacion: cada personaje sigue su secuencia (arbol de
 * comportamiento del original) como un guion de pasos; se coordinan con senales ("cherub play",
 * "cherub delivered", "cherub leave now", "KILL", "scroll over", "ending done"). Tiempos a 60 Hz;
 * los MoveNode/EndingScroll "From30FPS" duran el doble de ticks con la mitad de paso. */
#include <string.h>
#include <math.h>
#include "ending.h"
#include "assets.h"
#include "audio.h"
#include "items.h"
#include "jk/values.h"

#define ENDING_SCREEN0 42

/* ---- sprites (zona izquierda de la hoja del rey; centro en pixeles) ---- */
typedef struct { short x, y, w, h; float cx, cy; } spr;
enum {
    S_BABE_IDLE0, S_BABE_IDLE1, S_BABE_IDLE2, S_BABE_JUMP0, S_BABE_JUMP1, S_BABE_JUMP2, S_BABE_JUMP3, S_BABE_KISS,
    S_C_IDLE_L, S_C_IDLE_R, S_C_CHARGE, S_C_WL0, S_C_WL1, S_C_WL2, S_C_WR0, S_C_WR1, S_C_WR2, S_C_DUNK, S_C_PARASOL,
    S_C_FLY0, S_C_FLY1, S_C_FLY2, S_C_UP0, S_C_UP1, S_C_UP2,
    S_SNATCH0, S_SNATCH1, S_CH_DEL0, S_CH_DEL1, S_CH_ESC0, S_CH_ESC1,
    S_K_IDLE, S_K_LOOKUP, S_K_CHARGE,           /* de la hoja normal del rey (celdas de 48) */
    S_NONE
};
#define C64Y (0.5f + 16.0f / 96.0f) * 64.0f    /* BabeCouple p_center2 en sprites de 64 */
static const spr SPR[] = {
    { 64, 0, 32, 32, 16, 16 }, { 96, 0, 32, 32, 16, 16 }, { 128, 0, 32, 32, 16, 16 }, { 160, 0, 32, 32, 16, 16 },
    { 160, 32, 32, 32, 16, 16 }, { 128, 32, 32, 32, 16, 16 }, { 96, 32, 32, 32, 16, 16 }, { 64, 32, 32, 32, 16, 16 },
    { 0, 32, 32, 32, 16, 32 }, { 32, 32, 32, 32, 16, 32 }, { 32, 0, 32, 32, 16, 32 },
    { 64, 64, 32, 32, 16, 32 }, { 32, 64, 32, 32, 16, 32 }, { 0, 64, 32, 32, 16, 32 },
    { 160, 64, 32, 32, 16, 32 }, { 128, 64, 32, 32, 16, 32 }, { 96, 64, 32, 32, 16, 32 },
    { 64, 96, 64, 64, 32, C64Y }, { 0, 96, 64, 64, 32, C64Y },
    { 0, 160, 64, 64, 32, C64Y }, { 64, 160, 64, 64, 32, C64Y }, { 128, 160, 64, 64, 32, C64Y },
    { 0, 224, 64, 64, 32, C64Y }, { 64, 224, 64, 64, 32, C64Y }, { 128, 224, 64, 64, 32, C64Y },
    { 0, 0, 32, 32, 16, 32 }, { 128, 96, 64, 32, 32, 32 },
    { 192, 0, 32, 32, 16, 16 }, { 192, 32, 32, 32, 16, 16 }, { 192, 64, 32, 32, 16, 16 }, { 192, 96, 32, 32, 16, 16 },
    { 0, 0, 48, 48, 24, 48 }, { 0, 144, 48, 48, 24, 48 }, { 0, 48, 48, 48, 24, 48 },
};

/* ---- guion ---- */
enum { OP_END, OP_WAIT, OP_SIG, OP_PAUSE, OP_SPR, OP_SFX, OP_MOVE, OP_SCROLL, OP_MOVESCROLL, OP_ANIM,
       OP_JUMPUP, OP_FALL, OP_BABEJUMP, OP_ARC, OP_CROWN, OP_HIDE, OP_DELIVER, OP_ESCAPE };
enum { SIG_CHERUB_PLAY = 1, SIG_DELIVERED = 2, SIG_LEAVE = 4, SIG_KILL = 8, SIG_SCROLL_OVER = 16, SIG_DONE = 32 };
enum { AN_NONE, AN_WALK_L, AN_WALK_R, AN_FLY, AN_UPSKIRT, AN_BABE_IDLE };
typedef struct { int op; float a, b, c, d, e; } step;

#define SEC(s) ((float)(s) * 60.0f)
#define R30(n) ((float)(n) * 2.0f)                     /* repeticiones a 30 fps -> ticks a 60 */

static const step KING[] = {
    { OP_SPR, S_K_IDLE }, { OP_WAIT, SIG_CHERUB_PLAY }, { OP_PAUSE, SEC(1) }, { OP_SPR, S_K_LOOKUP },
    { OP_WAIT, SIG_DELIVERED }, { OP_SPR, S_K_CHARGE }, { OP_PAUSE, SEC(0.5) }, { OP_SFX, SFX_JUMP },
    { OP_JUMPUP }, { OP_SFX, SFX_PLINK }, { OP_CROWN }, { OP_PAUSE, SEC(1) }, { OP_SIG, SIG_LEAVE },
    { OP_FALL }, { OP_SFX, SFX_LAND }, { OP_SPR, S_K_IDLE }, { OP_PAUSE, SEC(1) }, { OP_SPR, S_SNATCH0 },
    { OP_PAUSE, SEC(1) }, { OP_SPR, S_SNATCH1 }, { OP_SIG, SIG_KILL }, { OP_PAUSE, 2 },
    { OP_SPR, S_C_IDLE_R }, { OP_PAUSE, SEC(1) }, { OP_ANIM, AN_WALK_L }, { OP_MOVE, R30(60), -0.75f, 0 },
    { OP_ANIM, AN_NONE }, { OP_SPR, S_C_IDLE_R }, { OP_PAUSE, SEC(2) }, { OP_ANIM, AN_WALK_R },
    { OP_SFX, SFX_BABE_SURPRISED }, { OP_MOVE, R30(16), 3.0f, 0 },
    { OP_MOVESCROLL, R30(7), 3.0f, 0, -1.5f, 0 },
    { OP_ANIM, AN_NONE }, { OP_SPR, S_C_CHARGE }, { OP_PAUSE, SEC(0.6) }, { OP_SFX, SFX_JUMP },
    { OP_SPR, S_C_DUNK }, { OP_SFX, SFX_BABE_SCREAM }, { OP_ARC },
    { OP_SPR, S_C_PARASOL }, { OP_MOVESCROLL, R30(10), 1.75f, -1.5f, -1.75f, -0.5f },
    { OP_ANIM, AN_FLY }, { OP_MOVESCROLL, R30(1), 1.75f, 0.5f, -1.75f, -0.5f },
    { OP_PAUSE, SEC(3.55) }, { OP_SFX, SFX_BABE_MOU }, { OP_ANIM, AN_UPSKIRT }, { OP_PAUSE, SEC(2) },
    { OP_ANIM, AN_FLY }, { OP_PAUSE, SEC(3) }, { OP_SIG, SIG_DONE }, { OP_END },
};
static const step BABE[] = {
    { OP_SPR, S_BABE_IDLE0 }, { OP_ANIM, AN_BABE_IDLE }, { OP_WAIT, SIG_SCROLL_OVER }, { OP_PAUSE, SEC(1) },
    { OP_ANIM, AN_NONE }, { OP_SPR, S_BABE_JUMP0 }, { OP_PAUSE, SEC(0.5) }, { OP_SFX, SFX_BABE_JUMP },
    { OP_BABEJUMP }, { OP_SPR, S_BABE_JUMP3 }, { OP_SFX, SFX_LAND }, { OP_PAUSE, SEC(0.1) },
    { OP_ANIM, AN_BABE_IDLE }, { OP_PAUSE, SEC(0.5) }, { OP_MOVE, R30(44), -0.5f, 0 }, { OP_PAUSE, SEC(0.4) },
    { OP_ANIM, AN_NONE }, { OP_SPR, S_BABE_KISS }, { OP_SFX, SFX_BABE_KISS }, { OP_PAUSE, SEC(0.5) },
    { OP_ANIM, AN_BABE_IDLE }, { OP_SIG, SIG_CHERUB_PLAY }, { OP_PAUSE, SEC(1) }, { OP_MOVE, R30(8), 0.5f, 0 },
    { OP_WAIT, SIG_KILL }, { OP_HIDE }, { OP_END },
};
static const step CHERUB[] = {
    { OP_SPR, S_CH_DEL0 }, { OP_WAIT, SIG_CHERUB_PLAY }, { OP_DELIVER }, { OP_SIG, SIG_DELIVERED },
    { OP_WAIT, SIG_LEAVE }, { OP_ESCAPE }, { OP_HIDE }, { OP_END },
};

typedef struct {
    const step *s; int pc;
    float x, y, vx, vy, t; int ticks, ticks2;
    int sprite, anim, hidden, flip;
    float anim_t; int anim_f;
} actor;

static actor king, babe, cherub;
static int signals, done, phase, pic_t;
static float cam_x, cam_y, scroll_left, scroll_dx, scroll_dy;
static GSTEXTURE sheet, image;
static int sheet_ok, image_ok;

/* JumpUp: sube con -CalculateJumpStrength(78) hasta que la velocidad deja de ser negativa */
static float jump_strength(int height)
{
    float h = 0, v = 0; int n = 0;
    while (h < (float)height) { v += JK.gravity; h += v; n++; }
    return (float)n * JK.gravity;
}

int ending_check(const jk_king *k)
{
    if (k->screen != ENDING_SCREEN0 || !k->body.on_ground) return 0;
    jk_rect hb = jk_body_hitbox(&k->body);
    return (float)(hb.x + hb.w / 2) >= 240.0f + 96.0f;            /* endingKingStartPosition.X */
}

void ending_start(GSGLOBAL *gs, const jk_king *k)
{
    (void)k;
    assets_pool_reset();
    memset(&sheet, 0, sizeof sheet); memset(&image, 0, sizeof image);
    sheet_ok = assets_t8_pool(gs, "ending_sheet.t8", &sheet) == 0;
    image_ok = assets_t8_pool(gs, "ending_image.t8", &image) == 0;
    float top = -(float)ENDING_SCREEN0 * JK_SCREEN_H;
    memset(&king, 0, sizeof king); memset(&babe, 0, sizeof babe); memset(&cherub, 0, sizeof cherub);
    king.s = KING;   king.x = 240 + 96;   king.y = top + 180 - 36;   /* INSIDE_SCREEN_POSITION */
    babe.s = BABE;   babe.x = 240 + 156;  babe.y = top + 180 - 52;
    cherub.s = CHERUB; cherub.x = 480;    cherub.y = 0;                /* coordenadas de pantalla */
    babe.flip = 1;
    signals = 0; done = 0; phase = 0; pic_t = 0;
    cam_x = cam_y = 0;
    /* EndingScroll.CreateStartPanCamera: (-96, 96) a 0.5 px por tick */
    float len = sqrtf(96.0f * 96.0f * 2.0f);
    scroll_left = roundf(len / 0.5f);
    scroll_dx = -96.0f / scroll_left; scroll_dy = 96.0f / scroll_left;
    audio_music("ending", "");
}

static void anim_tick(actor *a)
{
    a->anim_t += 1.0f;
    switch (a->anim) {
    case AN_WALK_L: case AN_WALK_R: {                              /* CoupleAnim: tiempos de WalkAnim */
        int base = a->anim == AN_WALK_L ? S_C_WL0 : S_C_WR0;
        float t = a->anim_t;
        if (t > 28) { a->anim_t = 1; t = 1; }
        a->sprite = t > 24 ? base + 1 : t > 14 ? base + 2 : t > 10 ? base + 1 : base;
        break;
    }
    case AN_FLY: case AN_UPSKIRT: {                                /* LoopingAnimNode */
        int base = a->anim == AN_FLY ? S_C_FLY0 : S_C_UP0;
        if (a->anim_t >= 6) { a->anim_t = 0; a->anim_f = (a->anim_f + 1) % 3; }
        a->sprite = base + a->anim_f;
        break;
    }
    case AN_BABE_IDLE:
        if (a->anim_t >= 20) { a->anim_t = 0; a->anim_f = (a->anim_f + 1) % 3; }
        a->sprite = S_BABE_IDLE0 + a->anim_f;
        break;
    }
}

/* ejecuta pasos hasta que uno necesite mas ticks */
static void run(actor *a)
{
    for (int guard = 0; guard < 64; guard++) {
        const step *st = &a->s[a->pc];
        switch (st->op) {
        case OP_END: return;
        case OP_WAIT: if (!(signals & (int)st->a)) return; a->pc++; continue;
        case OP_SIG: signals |= (int)st->a; if ((int)st->a == SIG_DONE) done = 1; a->pc++; continue;
        case OP_SPR: a->sprite = (int)st->a; a->pc++; continue;
        case OP_SFX: audio_play_sfx((int)st->a); a->pc++; continue;
        case OP_ANIM: a->anim = (int)st->a; a->anim_t = 0; a->anim_f = 0; a->pc++; continue;
        case OP_HIDE: a->hidden = 1; a->pc++; continue;
        case OP_CROWN: {                                         /* PutOnCrown: da la corona y se la pone */
            items_give(0);                                       /* Items.Crown */
            *items_equip_flag(0) = 1;
            a->pc++; continue;
        }
        case OP_PAUSE:
            if (a->t == 0) a->t = st->a;
            if (--a->t <= 0) { a->t = 0; a->pc++; continue; }
            return;
        case OP_MOVE: case OP_MOVESCROLL:
            if (a->t == 0) a->t = st->a;
            a->x += st->b; a->y += st->c;
            if (st->op == OP_MOVESCROLL) { cam_x += st->d; cam_y += st->e; }
            if (--a->t <= 0) { a->t = 0; a->pc++; continue; }
            return;
        case OP_JUMPUP:
            if (a->ticks == 0) { a->vy = -jump_strength(78); a->ticks = 1; }
            a->y += a->vy; a->vy += JK.gravity; a->ticks2++;
            if (a->vy >= 0) { a->ticks = 0; a->pc++; continue; }  /* ticks2 guarda cuanto subio */
            return;
        case OP_FALL:
            if (a->ticks == 0) { a->vy = 0; a->ticks = 1; }
            a->vy += JK.gravity; a->y += a->vy;
            if (--a->ticks2 <= 0) { a->ticks = 0; a->pc++; continue; }
            return;
        case OP_BABEJUMP: {                                      /* Babe.Jump: salto de 15 px y vuelta */
            if (a->ticks == 0) { a->vy = -jump_strength(15); a->vx = 0; a->ticks = 1; }
            a->vx += a->vy; a->y += a->vy; a->vy += JK.gravity;
            if (a->vy > 0 && a->vx >= 0) { a->ticks = 0; a->pc++; continue; }
            return;
        }
        case OP_ARC: {                                           /* JumpToTargetHeight + avance y camara */
            if (a->ticks == 0) { a->vy = JK.jump; a->vx = 0; a->ticks = 1; }
            a->vx += a->vy; a->y += a->vy; a->vy += JK.gravity;
            a->x += JK.air_speed; cam_x -= JK.air_speed;
            if (a->vy > 0 && a->vx >= 0) { a->ticks = 0; a->pc++; continue; }
            return;
        }
        case OP_DELIVER: {                                       /* CherubsDeliver: entra y baja con la corona */
            if (a->ticks == 0) { a->ticks = 1; a->ticks2 = 0; a->t = 0; }
            a->ticks2++;
            if (a->ticks2 % 4 == 0) {
                if (a->ticks2 / 4 < 40) a->x -= 5.0f;
                if (a->y < 135.0f) a->y += (135.0f - a->y) * 0.1f;
                a->sprite = (a->ticks2 / 8) % 2 ? S_CH_DEL1 : S_CH_DEL0;
            }
            if (a->ticks2 / 4 >= 60) { a->ticks = 0; a->pc++; continue; }
            return;
        }
        case OP_ESCAPE: {                                        /* CherubsEscape: se van arriba a la derecha */
            if (a->ticks == 0) { a->ticks = 1; a->vx = 0; a->vy = -0.1f; a->ticks2 = 0; }
            if (++a->ticks2 % 4 == 0) { a->vx += 0.5f; a->vy *= 1.2f; a->sprite = (a->ticks2 / 8) % 2 ? S_CH_ESC1 : S_CH_ESC0; }
            a->x += a->vx / 4.0f; a->y += a->vy / 4.0f;
            if (a->x > 520 || a->y < -40) { a->ticks = 0; a->pc++; continue; }
            return;
        }
        }
    }
}

int ending_update(int any_input)
{
    if (phase == 1) {                                            /* EndingPictureScreen: 2 s y cualquier boton */
        pic_t++;
        return pic_t > 120 && any_input;
    }
    if (scroll_left > 0) {
        cam_x += scroll_dx; cam_y += scroll_dy;
        if (--scroll_left <= 0) signals |= SIG_SCROLL_OVER;
    }
    if (done) return 1;                                          /* sigue: creditos (GameEnding) */
    run(&cherub); run(&babe); run(&king);
    anim_tick(&king); anim_tick(&babe);
    return done;
}

void ending_camera(float *ox, float *oy) { *ox = phase == 0 ? cam_x : 0; *oy = phase == 0 ? cam_y : 0; }
int  ending_in_picture(void) { return phase == 1; }
void ending_show_picture(void) { phase = 1; pic_t = 0; }

static void draw_spr(GSGLOBAL *gs, GSTEXTURE *t, const spr *s, float x, float y, int flip, float sx, float sy)
{
    float u0 = s->x, u1 = s->x + s->w;
    if (flip) { float tmp = u0; u0 = u1; u1 = tmp; }
    float x0 = x - s->cx, y0 = y - s->cy;
    gsKit_prim_sprite_texture(gs, t, x0 * sx, y0 * sy, u0, s->y, (x0 + s->w) * sx, (y0 + s->h) * sy, u1, s->y + s->h, 4, 0x80808080);
}

void ending_draw(GSGLOBAL *gs, GSTEXTURE *king_sheet, float sx, float sy)
{
    if (phase == 1) {
        if (image_ok)
            gsKit_prim_sprite_texture(gs, &image, 0, 0, 0, 0, JK_SCREEN_W * sx, JK_SCREEN_H * sy,
                                      image.Width, image.Height, 7, 0x80808080);
        return;
    }
    float oy = (float)ENDING_SCREEN0 * JK_SCREEN_H;                /* mundo -> pantalla 42 */
    if (!babe.hidden && sheet_ok) draw_spr(gs, &sheet, &SPR[babe.sprite], babe.x + cam_x, babe.y + oy + cam_y, babe.flip, sx, sy);
    if (king.sprite >= S_K_IDLE) {
        if (king_sheet) draw_spr(gs, king_sheet, &SPR[king.sprite], king.x + cam_x, king.y + oy + cam_y, 0, sx, sy);
    } else if (sheet_ok) {
        draw_spr(gs, &sheet, &SPR[king.sprite], king.x + cam_x, king.y + oy + cam_y, 0, sx, sy);
    }
    if (!cherub.hidden && sheet_ok && (signals & SIG_CHERUB_PLAY))
        draw_spr(gs, &sheet, &SPR[cherub.sprite], cherub.x - 1, cherub.y + 2, 0, sx, sy);
}
