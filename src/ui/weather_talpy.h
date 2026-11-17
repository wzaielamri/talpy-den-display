#pragma once
#include "lvgl.h"
#include "ui/talpy_set.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// The weather Talpy: Talpy (TALPY_WEATHER_SET) in front of his live sky (weather_sky.h), in one 104 x 104 box with no
// background that clips its children and is not clickable: the 104 x 96 scene at its bottom, and 8 px over it for
// the clouds and suns that reach higher. Layers, bottom to top: sky back, Talpy, sky front.
// Deleting the box frees everything. The functions do nothing on NULL.

// calm (the clock face): Talpy pace 3 and a calm sky. Created running.
lv_obj_t *weather_talpy_create(lv_obj_t *parent, bool calm);

// Show this Talpy mood and sky, at once. The same as shown: nothing. At a thunderstorm Talpy ducks at each flash.
// talpy TW_HIDDEN (down in his burrow): the sky's drops and flakes stop on the burrow, the fog's lamp glow sits on
// the lamp peeking out of it.
void weather_talpy_set(lv_obj_t *w, tw_mood_t talpy, tw_mood_t sky);

// on: Talpy and the sky stop where they are; off: they go on.
void weather_talpy_freeze(lv_obj_t *w, bool on);

#ifdef __cplusplus
} /*extern "C"*/
#endif
