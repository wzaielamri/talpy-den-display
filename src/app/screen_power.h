#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// What the screen shows and how bright, 24/7. Never dark, and the LCD always refreshed:
// - the cards (at the slider's brightness) while someone looks: a touch in the last 2 min, a job
//   waiting for you, or "Stay on" (a job working doesn't keep them: one can work for hours);
// - else the clock face (ui/clock_face.c), dimmed to 10% (where the backlight doesn't hum), and always while
//   the PC is away (no message for a while: suspended, shut down, unplugged), or at once after a double
//   tap on the cards. A touch on it brings the cards back for 2 min.
// The brightness and "Stay on" are saved (app/settings.cpp) and come back after a restart.

// After screen_main_create(): the saved brightness and "Stay on" back, and the timer started.
void screen_power_init(void);

// The brightness slider's value (0..100): set now, and saved once the slider is still.
void screen_power_set_brightness(int32_t percent);

// "Stay on" in the pull-down panel: the cards stay (but not while the PC is away), saved.
void screen_power_set_stay_on(bool on);

// Whether any job is waiting for you (serial_link.cpp, from each jobs message). While one is, the
// cards stay; one starting to wait brings them back; once none is, the clock comes 2 min later.
void screen_power_set_busy(bool on);

// The cards now, for 2 min at least (a touch, an alert, the PC back).
void screen_power_wake(void);

// The PC is there (a message came in after a while without) or away (none for a while, or it said
// it suspends): away, the clock face, and an alert that shows goes.
void screen_power_pc_present(bool present);

#ifdef __cplusplus
} /*extern "C"*/
#endif
