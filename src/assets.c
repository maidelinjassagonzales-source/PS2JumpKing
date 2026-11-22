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
