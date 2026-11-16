#pragma once
#include "lvgl.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// The clock face: the time big in the middle, the date under it, Talpy asleep (a still picture),
// the weather (icon and temperature) under that, over everything else on the main screen. screen_power.c shows it after 2 min without
// a touch (no job working or waiting, no "Stay on") and while the PC is away, dimmed; a touch on it
// calls the touch callback (back to the cards). It's what the screen shows 24/7 when nobody looks,
// so nothing on it moves: the time is drawn again once a minute, and the whole face moves a few
// pixels each minute, so no picture stays in one place on the LCD for hours.
void clock_face_create(lv_obj_t *screen);

typedef void (*clock_face_touch_cb_t)(void);
void clock_face_on_touch(clock_face_touch_cb_t cb);

void clock_face_show(void);
void clock_face_hide(void);

// The current weather (the weather card's), under the date; till the first, none.
void clock_face_set_weather(int wmo_code, bool is_day, int temp);

// "PC offline" at its bottom while the PC isn't there.
void clock_face_set_pc(bool online);

#ifdef __cplusplus
} /*extern "C"*/
#endif
