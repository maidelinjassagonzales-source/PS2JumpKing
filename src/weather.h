/* Clima (WeatherInstance + MaskShader): frames animados en mosaico, o precalculados con la mascara
 * de la pantalla. Datos: assets_ps2/weather.bin + los .t8 de weather (copia del usuario). */
#ifndef WEATHER_H
#define WEATHER_H
#include <gsKit.h>

int  weather_init(void);
void weather_update(GSGLOBAL *gs, int screen, float dt);
void weather_draw(GSGLOBAL *gs, int screen, int foreground, float sx, float sy);
#endif
