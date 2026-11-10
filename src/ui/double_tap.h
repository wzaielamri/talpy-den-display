#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

// A double tap: two short taps, the second lifted within 500 ms of the first and close to it.
// LVGL 9.1 has no double-click event of its own (9.2 does), so this counts the taps itself.

typedef void (*double_tap_cb_t)(void);

// Who's told of a double tap (screen_power.c: the clock face now).
void double_tap_set_callback(double_tap_cb_t cb);

// Count the taps on obj. Only for what does nothing with one tap (the screen, the header): one with
// an action of its own (the weather card opens the hourly view) would do that on the first tap.
void double_tap_watch(lv_obj_t *obj);

#ifdef __cplusplus
} /*extern "C"*/
#endif
