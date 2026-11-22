#include <string.h>
#include <stdio.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <fcntl.h>
#include <libmc.h>
#include "save.h"

#define DIR  "/JKPS2SAVE"
#define FILE_ DIR "/save.bin"

static int ready, unformatted;
static char buf[SAVE_BLOB_MAX + 16] __attribute__((aligned(64)));

/* Memory card icon (the port's original character, tools/make_icon.py), embedded in the ELF */
extern unsigned char icon_sys[];
extern unsigned int size_icon_sys;
extern unsigned char icon_ico[];
extern unsigned int size_icon_ico;

static void write_raw(const char *path, const unsigned char *data, unsigned size)
{
    static char tmp[40 * 1024] __attribute__((aligned(64)));
    int fd, r;
    if (size > sizeof tmp) return;
    memcpy(tmp, data, size);
    mcOpen(0, 0, path, O_WRONLY | O_CREAT | O_TRUNC);
    mcSync(0, NULL, &fd);
    if (fd < 0) return;
    mcWrite(fd, tmp, size);
    mcSync(0, NULL, &r);
    mcClose(fd);
    mcSync(0, NULL, &r);
}

int save_init(void)
{
    int r;
    if (SifLoadModule("rom0:MCMAN", 0, NULL) < 0 || SifLoadModule("rom0:MCSERV", 0, NULL) < 0) {
        printf("[jk] no se pudo cargar el driver de memory card\n");
        return -1;
    }
    if (mcInit(MC_TYPE_MC) < 0) return -1;
    int type = 0, free_ = 0, format = 0;
    /* la primera consulta tras insertar la tarjeta devuelve -1 (tarjeta nueva): se repite */
    for (int i = 0; i < 2; i++) {
        mcGetInfo(0, 0, &type, &free_, &format);
        mcSync(0, NULL, &r);
    }
    ready = 1;
    unformatted = (r == -2 || (type == 2 && !format));
    printf("[jk] memory card: tipo %d, libre %d KB, formateada %d (r=%d)\n", type, free_, format, r);
    return 0;
}

/* Tarjeta sin formatear: esta vacia, se formatea antes del primer guardado (como los juegos de PS2). */
static int ensure_formatted(void)
{
    if (!unformatted) return 0;
    int r;
    printf("[jk] formateando la memory card del puerto 1 (estaba sin formatear)\n");
    mcFormat(0, 0);
    mcSync(0, NULL, &r);
    if (r < 0) { printf("[jk] no se pudo formatear (%d)\n", r); return -1; }
    unformatted = 0;
    return 0;
}

static int open_file(int mode)
{
    int fd;
    mcOpen(0, 0, FILE_, mode);
    mcSync(0, NULL, &fd);
    return fd;
}

int save_read(void *data, int max)
{
    if (!ready) return -1;
    int fd = open_file(O_RDONLY), r;
    if (fd < 0) return -1;
    mcRead(fd, buf, max + 8);
    mcSync(0, NULL, &r);
    int cl; mcClose(fd); mcSync(0, NULL, &cl);
    if (r < 8 || memcmp(buf, "JKSV", 4) != 0) return -1;
    int size = buf[4] | buf[5] << 8 | buf[6] << 16;
    if (size > max || size + 8 > r) return -1;
    memcpy(data, buf + 8, size);
    return size;
}

int save_exists(void)
{
    static char tmp[SAVE_BLOB_MAX];
    return save_read(tmp, sizeof tmp) > 0;
}

int save_write(const void *data, int size)
{
    if (!ready || size > SAVE_BLOB_MAX) return -1;
    if (ensure_formatted() != 0) return -1;
    int r;
    mcMkDir(0, 0, DIR);
    mcSync(0, NULL, &r);                      /* -4 if it already exists: not an error */
    write_raw(DIR "/icon.sys", icon_sys, size_icon_sys);    /* what the PS2 browser shows */
    write_raw(DIR "/jkps2.ico", icon_ico, size_icon_ico);
    int fd = open_file(O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0) { printf("[jk] no se pudo abrir el guardado (%d)\n", fd); return -1; }
    memcpy(buf, "JKSV", 4);
    buf[4] = size & 255; buf[5] = size >> 8 & 255; buf[6] = size >> 16 & 255; buf[7] = 1;
    memcpy(buf + 8, data, size);
    mcWrite(fd, buf, size + 8);
    mcSync(0, NULL, &r);
    int cl; mcClose(fd); mcSync(0, NULL, &cl);
    if (r != size + 8) { printf("[jk] guardado incompleto (%d)\n", r); return -1; }
    printf("[jk] partida guardada (%d bytes)\n", size);
    return 0;
}

void save_delete(void)
{
    if (!ready) return;
    int r;
    mcDelete(0, 0, FILE_);
    mcSync(0, NULL, &r);
}
