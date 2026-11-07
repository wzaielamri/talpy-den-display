#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

void card_weather_create(lv_obj_t *parent);

// Fill the card. temp/hi/lo in °C; condition e.g. "Partly cloudy"; updated = fetch time, e.g. "14:45".
// NULL strings show "--". Call from the LVGL loop (loop()/serial handler), never from another task.
void card_weather_set(int temp, int hi, int lo, const char *condition, const char *updated);

#ifdef __cplusplus
} /*extern "C"*/
#endif