#pragma once
#include <stdint.h>

#define DIMMEST 244

#if DIMMEST > 245
#error "DIMMEST must be 245 or less: higher values are commands, not brightness"
#endif

#ifdef __cplusplus
extern "C" {
#endif

void backlight_set_percent(int32_t percent);

#ifdef __cplusplus
} /*extern "C"*/
#endif