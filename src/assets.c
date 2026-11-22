#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include "assets.h"

#define ROOT "host:assets_ps2/"

void *assets_load(const char *name, int *size)
{
    char path[160];
    snprintf(path, sizeof path, ROOT "%s", name);
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    void *buf = memalign(128, n);
    if (buf && fread(buf, 1, n, f) != (size_t)n) { free(buf); buf = NULL; }
    fclose(f);
    if (size) *size = (int)n;
    return buf;
}

/* Formato 'JKT8': u16 w, u16 h, paleta 256xRGBA (ya en orden CSM1), indices.
 * Si keep_vram, reutiliza la VRAM ya reservada en t (mismo tamano). */
int assets_t8(GSGLOBAL *gs, const char *name, GSTEXTURE *t, int keep_vram)
{
    int n;
    unsigned char *buf = assets_load(name, &n);
    if (!buf || memcmp(buf, "JKT8", 4) != 0) { free(buf); return -1; }
    unsigned w = buf[4] | buf[5] << 8, h = buf[6] | buf[7] << 8;

    if (t->Mem) free(t->Mem);
    if (t->Clut) free(t->Clut);
    t->Width = w; t->Height = h;
    t->PSM = GS_PSM_T8;
    t->ClutPSM = GS_PSM_CT32;
    t->Filter = GS_FILTER_NEAREST;
    t->Clut = memalign(128, 1024);
    memcpy(t->Clut, buf + 8, 1024);
    t->Mem = memalign(128, w * h);
    memcpy(t->Mem, buf + 8 + 1024, w * h);
    free(buf);

    if (!keep_vram) {
        t->Vram = gsKit_vram_alloc(gs, gsKit_texture_size(w, h, t->PSM), GSKIT_ALLOC_USERBUFFER);
        t->VramClut = gsKit_vram_alloc(gs, gsKit_texture_size(16, 16, t->ClutPSM), GSKIT_ALLOC_USERBUFFER);
        if (t->Vram == GSKIT_ALLOC_ERROR || t->VramClut == GSKIT_ALLOC_ERROR) {
            printf("[jk] sin VRAM para %s\n", name);
            return -1;
        }
    }
    gsKit_texture_upload(gs, t);
    return 0;
}

static u32 pool_base, pool_end, pool_cur;
static int pool_gen = 1;

void assets_pool_init(GSGLOBAL *gs)
{
    const u32 vram_total = 4 * 1024 * 1024;
    u32 avail = vram_total - gs->CurrentPointer;
    avail &= ~8191u;
    pool_base = gsKit_vram_alloc(gs, avail, GSKIT_ALLOC_USERBUFFER);
    if (pool_base == GSKIT_ALLOC_ERROR) { pool_base = pool_end = pool_cur = 0; return; }
    pool_end = pool_base + avail;
    pool_cur = pool_base;
    printf("[jk] pool de VRAM por pantalla: %u KB\n", avail / 1024);
}

void assets_pool_reset(void) { pool_cur = pool_base; pool_gen++; }
int  assets_pool_generation(void) { return pool_gen; }

static u32 pool_take(u32 size)
{
    size = (size + 8191u) & ~8191u;
    if (!pool_base || pool_cur + size > pool_end) return GSKIT_ALLOC_ERROR;
    u32 a = pool_cur; pool_cur += size; return a;
}

int assets_t8_pool(GSGLOBAL *gs, const char *name, GSTEXTURE *t)
{
    /* cabecera para saber el tamano antes de reservar */
    int n;
    unsigned char *buf = assets_load(name, &n);
    if (!buf || n < 8 || memcmp(buf, "JKT8", 4) != 0) { free(buf); return -1; }
    unsigned w = buf[4] | buf[5] << 8, h = buf[6] | buf[7] << 8;
    free(buf);
    u32 tv = pool_take(gsKit_texture_size(w, h, GS_PSM_T8));
    u32 cv = pool_take(gsKit_texture_size(16, 16, GS_PSM_CT32));
    if (tv == GSKIT_ALLOC_ERROR || cv == GSKIT_ALLOC_ERROR) { printf("[jk] pool lleno para %s\n", name); return -1; }
    t->Vram = tv; t->VramClut = cv;
    return assets_t8(gs, name, t, 1);
}
