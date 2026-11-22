/* Nivel como pila de pantallas de 480x360 con bloques solidos (por ahora solo cajas).
 * La pantalla n ocupa y en [-(n+1)*360, -n*360): se sube hacia y negativa. */
#ifndef JK_LEVEL_H
#define JK_LEVEL_H

typedef struct { int x, y, w, h; } jk_rect;

typedef struct {
    const jk_rect *blocks;
    int count;
    int can_teleport;   /* si es 0 se limita la X del rey a los bordes de pantalla */
} jk_screen;

typedef struct {
    const jk_screen *screens;
    int count;
} jk_level;

int jk_rect_overlap(const jk_rect *a, const jk_rect *b);

/* Prueba la pantalla actual y sus dos vecinas, como el original. Devuelve 1 si hay bloque solido. */
int jk_level_collides(const jk_level *lv, int screen, const jk_rect *box);

extern const jk_level jk_test_level;

#endif
