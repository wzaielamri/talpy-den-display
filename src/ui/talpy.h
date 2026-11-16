#pragma once
#include "lvgl.h"
#include "ui/talpy_player.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Talpy, the mole mascot, as an LVGL image. Its pictures come from talpy_set.h; the player decides which one is
// shown and this widget decodes it into a buffer of its own (internal RAM if it fits, else PSRAM) and draws it.
// The functions take what talpy_create / talpy_create_still returned, and do nothing on NULL.
// The image is not clickable, so a tap on it goes through to the screen.

// A moving Talpy (TALPY_SET.w x h), mood TALPY_LOADING, running. Deleting the object frees everything.
lv_obj_t *talpy_create(lv_obj_t *parent);

void talpy_set_mood(lv_obj_t *t, talpy_mood_t mood); // shown at once
void talpy_react_finished(lv_obj_t *t);              // plays the "finished" reaction once (not while NEEDS)

// on: its timer pauses and the current frame stays; off: it goes on.
void talpy_freeze(lv_obj_t *t, bool on);

// One frame of the set, at its own size, with no timer (the clock face's sleeping Talpy).
lv_obj_t *talpy_create_still(lv_obj_t *parent, uint16_t frame);

#ifdef __cplusplus
} /*extern "C"*/
#endif
