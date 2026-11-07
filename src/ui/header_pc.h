#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include <stdbool.h>

void header_pc_create(lv_obj_t *parent);

// true:  green pill, "PC connected"
// false: grey pill,  "PC offline"
// Call from the LVGL loop (loop()/serial handler), never from another task.
void header_pc_set_online(bool online);

#ifdef __cplusplus
} /*extern "C"*/
#endif
