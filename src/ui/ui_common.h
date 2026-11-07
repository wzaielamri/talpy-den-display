#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

void styles_init(void);
lv_obj_t *make_card(lv_obj_t *parent, int x, int y, int w, int h);

#ifdef __cplusplus
} /*extern "C"*/
#endif