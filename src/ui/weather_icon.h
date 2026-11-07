#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include <stdbool.h>

// Animated weather icon in a 104 x 96 box at (x, y) inside parent.
void weather_icon_create(lv_obj_t *parent, int x, int y);

// Show the icon for an Open-Meteo / WMO weather code (current.weather_code).
// is_day (current.is_day) picks sun or moon; it only matters for clear and partly cloudy.
// Calling it again with the same icon does nothing, so the animation keeps running smoothly.
// Call from the LVGL loop (loop()/serial handler), never from another task.
void weather_icon_set(int wmo_code, bool is_day);

#ifdef __cplusplus
} /*extern "C"*/
#endif
