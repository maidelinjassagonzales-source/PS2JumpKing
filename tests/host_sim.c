/* Pruebas de la fisica portada en el PC. Los valores esperados salen de las formulas del
 * original (docs/fisica.md), no de esta misma implementacion. */
#include <stdio.h>
#include <math.h>
#include "jk/values.h"
#include "jk/king.h"

static int fails = 0;
#define CHECK(cond, ...) do { int ok_ = (cond); printf("%s ", ok_ ? "[ok]  " : "[FALLO]"); \
    printf(__VA_ARGS__); printf("\n"); if (!ok_) fails++; } while (0)

static const jk_input NONE = {0,0,0}, JUMP = {0,0,1}, RIGHT = {0,1,0}, JUMP_R = {0,1,1};
#define FLOOR_Y (344.0f - JK_KING_H)

static void settle(jk_king *k) { for (int i = 0; i < 5; i++) jk_king_update(k, &jk_test_level, NONE); }

int main(void)
{
    jk_values_init();
    const float g = 153.0f / 595.0f;
    printf("gravedad=%.7f salto=%.7f\n", JK.gravity, JK.jump);
    CHECK(fabsf(JK.gravity - g) < 1e-6f, "gravedad = 153/595");
    CHECK(fabsf(JK.jump + 34.0f * g) < 1e-4f, "salto = -34 * gravedad");

    /* salto completo vertical: 36 frames de carga y unos 153 px de subida */
    jk_king k; jk_king_init(&k, 200.0f, FLOOR_Y); settle(&k);
    int charge_frames = 0, air = 0; float top = k.body.y, y0 = k.body.y;
    for (int f = 0; f < 200; f++) {
        jk_king_update(&k, &jk_test_level, f < 60 ? JUMP : NONE);
        if (k.charge > 0.0f) charge_frames++;
        if (!k.body.on_ground) air++;
        if (k.body.y < top) top = k.body.y;
    }
    printf("frames de carga=%d  subida=%.2f px  frames en el aire=%d\n", charge_frames, y0 - top, air);
    CHECK(charge_frames == 35 || charge_frames == 36, "carga completa en ~0.6 s");
    CHECK(fabsf((y0 - top) - 153.0f) <= 1.5f, "salto completo sube ~153 px");
    CHECK(k.body.on_ground && !k.splatted, "aterriza de pie tras un salto completo");

    /* andar: 1.5 px por frame */
    jk_king_init(&k, 100.0f, FLOOR_Y); settle(&k);
    float x0 = k.body.x;
    for (int f = 0; f < 10; f++) jk_king_update(&k, &jk_test_level, RIGHT);
    printf("andar 10 frames: %.2f px\n", k.body.x - x0);
    CHECK(fabsf(k.body.x - x0 - 13.5f) < 0.01f || fabsf(k.body.x - x0 - 15.0f) < 0.01f, "velocidad al andar 1.5 px/frame");

    /* salto a la derecha contra la pared: rebote a la mitad */
    jk_king_init(&k, 420.0f, FLOOR_Y); settle(&k);
    float vx_after = 0; int bounced = 0;
    for (int f = 0; f < 120 && !bounced; f++) {
        jk_king_update(&k, &jk_test_level, f < 60 ? JUMP_R : NONE);
        if (k.body.knocked) { bounced = 1; vx_after = k.body.vx; }
    }
    printf("rebote en pared: vx=%.3f\n", vx_after);
    CHECK(bounced && fabsf(vx_after + 0.5f * JK.air_speed) < 1e-4f, "rebote contra pared invierte vx y lo reduce a la mitad");

    /* caida corta: sin splat */
    jk_king_init(&k, 200.0f, 230.0f); k.body.on_ground = 0;
    for (int f = 0; f < 120; f++) jk_king_update(&k, &jk_test_level, NONE);
    CHECK(k.body.on_ground && !k.splatted, "caida corta (~90 px) no deja al rey tirado");

    /* caida larga desde la pantalla de arriba: splat */
    jk_king_init(&k, 200.0f, -300.0f); k.body.on_ground = 0; k.screen = 1;
    int splat_seen = 0, landed_frame = -1, recover_frame = -1;
    for (int f = 0; f < 400; f++) {
        jk_input in = (landed_frame >= 0 && f > landed_frame + 10) ? RIGHT : NONE;
        jk_king_update(&k, &jk_test_level, in);
        if (k.splatted && !splat_seen) { splat_seen = 1; landed_frame = f; }
        if (splat_seen && !k.splatted && recover_frame < 0) recover_frame = f;
    }
    printf("splat en frame %d, se levanta en frame %d, pantalla final %d\n", landed_frame, recover_frame, k.screen);
    CHECK(splat_seen, "caida a velocidad maxima produce splat");
    CHECK(recover_frame - landed_frame == 29, "el splat bloquea 30 ticks (0.5 s, contando el del aterrizaje)");
    CHECK(k.screen == 0, "la camara baja a la pantalla 0");

    printf(fails ? "\n%d FALLOS\n" : "\nTodo bien\n", fails);
    return fails != 0;
}
