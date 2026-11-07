#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

void header_clock_create(lv_obj_t *parent);
void header_clock_set_time(const char *time, const char *date);

#ifdef __cplusplus
} /*extern "C"*/
#endif