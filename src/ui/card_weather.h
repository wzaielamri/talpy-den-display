#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

void card_weather_create(lv_obj_t *parent);

// Fill the card. temp/hi/lo in °C; condition e.g. "Partly cloudy"; updated = fetch time, e.g. "14:45".
// NULL strings show "--". Call from the LVGL loop (loop()/serial handler), never from another task.
void card_weather_set(int temp, int hi, int lo, const char *condition, const char *updated);

// The icon: see weather_icon.h -> weather_icon_set(wmo_code, is_day).

// Checking aid: shows every weather icon in turn (4 s each) with its name as the condition.
// Call once after screen_main_create() in setup(); remove the call when you're done looking.
void card_weather_demo_start(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif