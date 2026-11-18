#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// What the screen shows and how bright, 24/7. Never dark, and the LCD always refreshed:
// - the cards (at the slider's brightness) while someone looks: a touch in the last 2 min, a job
//   waiting for you, or "Stay on" (a job working doesn't keep them: one can work for hours);
// - else the clock face (ui/clock_face.c), at the slider's brightness too (you're at the laptop), or at once after
//   a double tap on the cards. A touch on it brings the cards back for 2 min;
// - and only the clock face while the board is locked (app/board_lock.h: the laptop locked, or the PC away),
//   dimmed to 10% (where the backlight doesn't hum: app/screen_light.h).
// The brightness and "Stay on" are saved (app/settings.cpp) and come back after a restart.

// After screen_main_create(): the saved brightness and "Stay on" back, and the timer started.
void screen_power_init(void);

// The brightness slider's value (0..100): set now, and saved once the slider is still.
void screen_power_set_brightness(int32_t percent);

// "Stay on" in the pull-down panel: the cards stay (but not while the board is locked), saved.
void screen_power_set_stay_on(bool on);

// Whether any job is waiting for you (serial_link.cpp, from each jobs message). While one is, the
// cards stay; one starting to wait brings them back; once none is, the clock comes 2 min later.
void screen_power_set_busy(bool on);

// The cards now, for 2 min at least (a touch, an alert, the PC back).
void screen_power_wake(void);

// The board locked (true; app/board_lock.c says when: the laptop locked, or the PC away): the clock face, and
// it stays (a touch, a job waiting or "Stay on" don't bring the cards) till it's unlocked (false: the cards, as
// after a touch). A board that restarts starts unlocked (sync.py says otherwise within a second). Not saved.
void screen_power_set_locked(bool locked);
bool screen_power_locked(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif
