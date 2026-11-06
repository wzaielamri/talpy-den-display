#include <Arduino.h>

#include "lvgl.h"
#define DIMMEST 244

#if DIMMEST > 245
#error "DIMMEST must be 245 or less: higher values are commands, not brightness"
#endif