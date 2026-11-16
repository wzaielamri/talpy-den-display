#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

// Building blocks of the live sky (weather_sky.c): an invisible box that moves shapes as one piece (the small weather
// icons, weather_icon.c, group their shapes in it too), and the two forever animations (back and forth, falling).
// The animation functions only fill in a; the caller starts it (lv_anim_start), so it can decide when (a frozen sky
// keeps it to start later).

// Invisible box (no background, border, padding) at (x, y) in parent; it clips what's inside it.
lv_obj_t *wx_group(lv_obj_t *parent, int x, int y, int w, int h);

// lv_anim callback: move an object (var) to x = v.
void wx_anim_x(void *obj, int32_t v);

// Forever back and forth: from -> to -> from, eased, ms each way, after delay. Drifting clouds, glows, stars.
void wx_anim_bounce(lv_anim_t *a, void *var, lv_anim_exec_xcb_t cb, int32_t from, int32_t to, uint32_t ms,
                    uint32_t delay);

// Forever falling: cb(var, from .. to) over ms, then again from from, after delay. Rain, drizzle, snow.
void wx_anim_fall(lv_anim_t *a, void *var, lv_anim_exec_xcb_t cb, int32_t from, int32_t to, uint32_t ms,
                  uint32_t delay, lv_anim_path_cb_t path);

#ifdef __cplusplus
} /*extern "C"*/
#endif
