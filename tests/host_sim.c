/* Pruebas de la fisica portada en el PC. Los valores esperados salen de las formulas del
 * original (docs/fisica.md), no de esta misma implementacion. */
#include <stdio.h>
#include <math.h>
#include "jk/values.h"
#include "jk/king.h"
#include <stdlib.h>

static int fails = 0;
#define CHECK(cond, ...) do { int ok_ = (cond); printf("%s ", ok_ ? "[ok]  " : "[FALLO]"); \
    printf(__VA_ARGS__); printf("\n"); if (!ok_) fails++; } while (0)

static const jk_input NONE = {0,0,0}, JUMP = {0,0,1}, RIGHT = {0,1,0}, JUMP_R = {0,1,1};
#define FLOOR_Y (344.0f - JK_KING_H)

static void settle(jk_king *k) { for (int i = 0; i < 5; i++) jk_king_update(k, &jk_test_level, NONE); }

/* Nivel real (assets_ps2/level.bin de tu copia), si existe: busca una celda y deja caer al rey. */
static unsigned char *load_file(const char *path, int *n)
{
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); *n = (int)ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char *b = malloc(*n); if (fread(b, 1, *n, f) != (size_t)*n) { free(b); b = NULL; }
    fclose(f); return b;
}

/* Primera celda del tipo pedido con aire encima (4 celdas) en la pantalla s. */
static int find_tile(const jk_level *lv, int s, int kind, int slope, int *tx, int *ty)
{
    for (int y = 4; y < JK_TILE_H; y++)
        for (int x = 3; x < JK_TILE_W - 3; x++) {
            unsigned char c = lv->grid[(s * JK_TILE_H + y) * JK_TILE_W + x];
            if ((c & 15) != kind || (slope >= 0 && (c >> 4) != slope)) continue;
            int clear = 1;
            for (int a = 1; a <= 4 && clear; a++)
                for (int dx = -2; dx <= 2; dx++)
                    if (lv->grid[(s * JK_TILE_H + y - a) * JK_TILE_W + x + dx] & 15) clear = 0;
            if (clear) { *tx = x; *ty = y; return 1; }
        }
    return 0;
}

static void real_level_tests(void)
{
    int n; unsigned char *bin = load_file("assets_ps2/level.bin", &n);
    jk_level lv;
    if (!bin || jk_level_from_bin(&lv, bin, n) != 0) { printf("(sin assets_ps2/level.bin: pruebas del nivel real omitidas)\n"); return; }
    const jk_input none = {0,0,0};
    int found = 0;
    for (int s = 0; s < lv.count && !found; s++) {
        int tx, ty;
        if (!find_tile(&lv, s, JK_B_SLOPE, JK_SLOPE_TL, &tx, &ty)) continue;
        found = 1;
        /* solo el borde derecho de la caja sobre la diagonal: si toca tambien el bloque plano de
         * al lado, el original anula la pendiente (caja a la misma altura que una pendiente superior) */
        jk_king k; jk_king_init(&k, (float)(tx * 8 + 2 - JK_KING_W), (float)((ty - s * JK_TILE_H) * 8 - 40));
        k.screen = s; k.body.on_ground = 0;
        float maxvx = 0;
        for (int f = 0; f < 60; f++) { jk_king_update(&k, &lv, none); if (fabsf(k.body.vx) > maxvx) maxvx = fabsf(k.body.vx); }
        printf("pendiente en pantalla %d: |vx| maximo %.2f\n", s, maxvx);
        CHECK(maxvx > 0.5f, "caer sobre una pendiente hace deslizar al rey");
    }
    if (!found) printf("(no se encontro pendiente libre para probar)\n");
    found = 0;
    for (int s = 0; s < lv.count && !found; s++) {
        int tx, ty;
        if (!find_tile(&lv, s, JK_B_ICE, -1, &tx, &ty)) continue;
        found = 1;
        jk_king k; jk_king_init(&k, (float)(tx * 8 - 5), (float)((ty - s * JK_TILE_H) * 8 - 26));
        k.screen = s;
        for (int f = 0; f < 5; f++) jk_king_update(&k, &lv, none);
        k.body.vx = 1.5f;
        jk_king_update(&k, &lv, none);
        printf("hielo en pantalla %d: en_hielo=%d vx tras 1 frame=%.2f\n", s, k.body.on_ice, k.body.vx);
        CHECK(k.body.on_ice && k.body.vx > 1.3f && k.body.vx < 1.5f, "en hielo el rey frena 0.1 por frame");
    }
    free(bin);
}

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

    real_level_tests();

    printf(fails ? "\n%d FALLOS\n" : "\nTodo bien\n", fails);
    return fails != 0;
}
