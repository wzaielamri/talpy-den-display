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

// Hold the card's icon still (true) or let it move again (false): the pull-down panel holds it still
// while it's out, so nothing changes under its dimming (a change there showed through undimmed).
void weather_icon_freeze(bool frozen);

// The same pictures at half size (52 x 48) and standing still, for the hourly columns: a box in parent
// at (0, 0) (place it yourself), then the icon of a code in it, drawn again on each call.
// bg: the colour behind the box (the moon's cut-out disc takes it to look like a hole).
lv_obj_t *weather_icon_small_create(lv_obj_t *parent);
void weather_icon_small_set(lv_obj_t *box, int wmo_code, bool is_day, uint32_t bg);

#ifdef __cplusplus
} /*extern "C"*/
#endif
