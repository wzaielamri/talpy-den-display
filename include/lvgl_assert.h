#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// LVGL's assert handler (lv_conf.h: LV_ASSERT_HANDLER). A failed LVGL check (out of memory, say) prints where it
// failed on the serial port and restarts the board, which comes back with the cards (sync.py sends everything again
// after "Setup done"). LVGL's own handler, while(1), froze the screen as it was, with no word on the serial port.
__attribute__((noreturn)) void lvgl_assert_failed(const char *file, int line);

#ifdef __cplusplus
} /*extern "C"*/
#endif
