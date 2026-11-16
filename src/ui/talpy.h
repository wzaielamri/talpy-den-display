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

// A moving Talpy drawn from any set (the weather Talpy's), mood 0, running. Deleting the object frees everything.
lv_obj_t *talpy_create_set(lv_obj_t *parent, const talpy_set_t *set);

void talpy_set_mood(lv_obj_t *t, uint16_t mood);     // shown at once (a talpy_mood_t or tw_mood_t value)
void talpy_set_pace(lv_obj_t *t, uint8_t pace);      // 1..8: slower pauses between idle clips
void talpy_play_once(lv_obj_t *t, uint16_t clip);    // plays one clip of the set once, then back to the mood
void talpy_react_finished(lv_obj_t *t);              // plays the "finished" reaction once (not while NEEDS)

// on: its timer pauses and the current frame stays; off: it goes on.
void talpy_freeze(lv_obj_t *t, bool on);

// One frame of the set, at its own size, with no timer (the clock face's sleeping Talpy).
lv_obj_t *talpy_create_still(lv_obj_t *parent, uint16_t frame);

#ifdef __cplusplus
} /*extern "C"*/
#endif
