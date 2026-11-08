#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16

#define LV_USE_STDLIB_MALLOC LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_STRING LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_BUILTIN

#define LV_USE_OS LV_OS_NONE
#define LV_USE_LOG 0

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
