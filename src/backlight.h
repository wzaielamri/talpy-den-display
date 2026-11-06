#pragma once
#include <Arduino.h>

#define DIMMEST 244

#if DIMMEST > 245
#error "DIMMEST must be 245 or less: higher values are commands, not brightness"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

static void set_backlight(int32_t x);
void backlight_set_percent(int32_t percent);

#ifdef __cplusplus
} /*extern "C"*/
#endif