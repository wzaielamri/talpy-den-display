#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16

// LVGL takes its memory from the board's heap (malloc: internal RAM for small blocks, PSRAM for big ones), not from
// a fixed pool of its own. Its default pool, 64 KB, ran out: the two weather Talpies' skies are made of many shapes,
// and each of the clock face's 128 px digits needs an ~8 KB glyph buffer while it's drawn.
#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_BUILTIN

#define LV_USE_OS LV_OS_NONE
#define LV_USE_LOG 0

// A failed LVGL check prints where and restarts the board (src/drivers/lvgl_assert.c), not while(1).
#define LV_ASSERT_HANDLER_INCLUDE "lvgl_assert.h"
#define LV_ASSERT_HANDLER lvgl_assert_failed(__FILE__, __LINE__);

// Read the touch panel and draw every 16 ms (LVGL's default is 33): a finger dragging the pull-down
// panel is followed twice as closely and a release is seen sooner. The flush waits for the panel's
// refresh (~44 Hz at 18 MHz), so drawing never goes faster than that.
#define LV_DEF_REFR_PERIOD 16

#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_MONTSERRAT_40 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR 0

#endif
