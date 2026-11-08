#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// What moves on the main screen (the weather icon, the waiting frame) holds still while something
// covers it: drawn again and again under a see-through cover it showed through (the pull-down's
// dimming), and under the clock face nobody sees it anyway. Each cover holds and lets go on its own;
// it moves again once none holds it.
enum { MOTION_SHADE = 1, MOTION_CLOCK = 2 };

void motion_hold(unsigned who, bool on);

#ifdef __cplusplus
} /*extern "C"*/
#endif
