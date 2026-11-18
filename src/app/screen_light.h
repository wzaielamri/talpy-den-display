#pragma once
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// The backlight's percent for what shows: the slider's (slider, 0..100) on the cards, and on the clock face while
// you're at the laptop (it unlocked and there: you see it at a glance, as bright as the cards); the clock face
// while the board is locked (app/board_lock.h: the laptop locked or the PC away: nobody looks) dimmed to 10%,
// where the backlight doesn't hum and uses least, never brighter than the slider.
int32_t screen_light_percent(int32_t slider, bool clock_shown, bool locked);
#ifdef __cplusplus
} /*extern "C"*/
#endif
