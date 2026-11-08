#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

typedef void (*brightness_cb_t)(int32_t percent);
void card_brightness_set_callback(brightness_cb_t cb);

void card_brightness_create(lv_obj_t *parent);

// Put the slider (and its "72%") at percent, without calling the callback: the saved value at start.
void card_brightness_set_value(int32_t percent);

#ifdef __cplusplus
} /*extern "C"*/
#endif