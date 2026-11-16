#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include <stdbool.h>

// Weather icons at half size (52 x 48) and standing still, for the hourly columns: a box in parent
// at (0, 0) (place it yourself), then the icon of a code in it, drawn again on each call.
// bg: the colour behind the box (the moon's cut-out disc takes it to look like a hole).
lv_obj_t *weather_icon_small_create(lv_obj_t *parent);
void weather_icon_small_set(lv_obj_t *box, int wmo_code, bool is_day, uint32_t bg);

#ifdef __cplusplus
} /*extern "C"*/
#endif
