#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// The screen's light and the board's sleep.
// - Dark after SLEEP_AFTER_MS (2 min) without a touch, unless "Stay on" is set or a job is working or
//   waiting for you; a touch brings it back at the brightness it had. That touch only wakes it: it
//   doesn't land on what's under the finger. While dark, LVGL draws nothing.
// - The PC away (no message for a while: suspended, shut down, unplugged): dark, and the board itself
//   sleeps in short naps (main.cpp's loop), waking on serial data or a touch. A touch lights it for
//   SLEEP_AFTER_MS, as a desk clock; the PC's next whole message brings it back for good.

// After screen_main_create(): makes the invisible catcher of the waking touch and starts the timer.
void screen_power_init(void);

// The brightness slider's value (0..100): set now if lit, and kept to come back to.
void screen_power_set_brightness(int32_t percent);

// "Stay on" in the pull-down panel: no going dark after a while without a touch (but still while the
// PC is away: a laptop that's suspended or shut down leaves the desk dark).
void screen_power_set_stay_on(bool on);

// Whether any job is working or waiting for you (serial_link.cpp, from each jobs message). While one
// is, the screen stays lit; one starting lights a dark screen; once none is, it goes dark
// SLEEP_AFTER_MS later (if not touched), so you see it done.
void screen_power_set_busy(bool on);

// Light up now (an alert came in, the PC is back) / go dark now.
void screen_power_wake(void);
void screen_power_sleep(void);

// Lit by a touch the board noticed while napping: lit, and that touch is kept from the screen.
void screen_power_wake_by_touch(void);

// The PC is there (a whole message came in after a while without) or away (none for a while, or it
// said it suspends).
void screen_power_pc_present(bool present);

// For main.cpp's loop: whether the board should nap now (dark, PC away, not listening), and that
// serial data woke it (stay up a while, so the PC's next message comes in whole).
bool screen_power_napping(void);
void screen_power_heard_serial(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif
