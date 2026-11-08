#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

// The pull-down panel, as on a phone: swipe down from the top of the screen (the header) and it slides
// down with the brightness slider. A tap under it, a swipe up on it, or 10 s without a touch closes it.
// It lives on lv_layer_top(), over whatever screen shows; create it after the screen is loaded.
void shade_create(void);

void shade_open(void);
void shade_close(void);

// Its "Stay on" toggle (next to the brightness): called with true when it's switched on, false off.
typedef void (*stay_on_cb_t)(bool on);
void shade_set_stay_on_callback(stay_on_cb_t cb);

#ifdef __cplusplus
} /*extern "C"*/
#endif
