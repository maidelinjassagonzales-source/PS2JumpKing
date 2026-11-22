/* Nivel como pila de pantallas de 480x360. La pantalla n ocupa y en [-(n+1)*360, -n*360):
 * se sube hacia y negativa. Nivel real: rejilla de celdas de 8 px (tools/build_assets.py). */
#ifndef JK_LEVEL_H
#define JK_LEVEL_H

typedef struct { int x, y, w, h; } jk_rect;

typedef struct {
    const jk_rect *blocks;      /* nivel de prueba: cajas solidas */
    int count;
    int can_teleport;           /* hay enlace en algun borde: no se limita la X */
    int tele[2];                /* pantalla destino 1-based (borde izq., der.), -1 = ninguno */
    int wind_on, wind_dir;      /* viento; direccion -1 = oscila, 1 = fija a la izquierda, 0 = a la derecha */
    float wind_int;
} jk_screen;

typedef struct {
    const jk_screen *screens;
    int count;
    const unsigned char *grid;  /* count * 45 * 60 celdas, o NULL (nivel de prueba) */
} jk_level;

enum { JK_TILE_W = 60, JK_TILE_H = 45 };
enum { JK_B_NONE, JK_B_SOLID, JK_B_SLOPE, JK_B_ICE, JK_B_SNOW, JK_B_SAND, JK_B_NOWIND, JK_B_WATER, JK_B_QUARK };
enum { JK_SLOPE_TL, JK_SLOPE_TR, JK_SLOPE_BL, JK_SLOPE_BR, JK_SLOPE_NONE };

/* Resultado de una consulta de choque (AdvCollisionInfo). */
enum { JK_C_ICE = 1, JK_C_SNOW = 2, JK_C_SAND = 4, JK_C_WATER = 8, JK_C_QUARK = 16, JK_C_NOWIND = 32 };
typedef struct {
    int   slope;                /* JK_SLOPE_* */
    float nx, ny;               /* normal de la pendiente */
    int   flags;                /* JK_C_*: bloques tocados, solidos o no */
} jk_cinfo;

int jk_rect_overlap(const jk_rect *a, const jk_rect *b);

/* Prueba la pantalla actual y sus dos vecinas. Devuelve 1 si hay bloque solido; info puede ser NULL. */
int jk_level_query(const jk_level *lv, int screen, const jk_rect *box, jk_cinfo *info);
int jk_level_collides(const jk_level *lv, int screen, const jk_rect *box);

extern const jk_level jk_test_level;

/* level.bin v2 ('JKL2'): cabeceras por pantalla y rejilla. */
int jk_level_from_bin(jk_level *lv, const unsigned char *bin, int size);

#endif
