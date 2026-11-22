#ifndef JK_CAMERA_H
#define JK_CAMERA_H
/* Cambio de pantalla segun el centro del rey y su velocidad vertical. */
int jk_camera_update(int current, int total, int center_y, float vy, int force);
#endif
