#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "talpy_set.h"
#include <stdbool.h>

// The live sky of the weather Talpy: sun, clouds, rain, snow, fog, lightning, moon and stars, drawn from LVGL shapes
// and animated, in two 104 x 96 boxes the caller stacks: back (behind Talpy) and front (in front of him: the drops,
// flakes and fog bands). Each instance (the weather card's, the clock face's) has its own shapes and animations.
// Call everything from the LVGL loop.

typedef struct weather_sky weather_sky_t;

// A sky drawing into back and front (104 x 96 each, made by the caller, holding nothing else; both must live as long
// as the sky, which is freed with back). Shows nothing until weather_sky_set. NULL if out of memory.
weather_sky_t *weather_sky_create(lv_obj_t *back, lv_obj_t *front);

// Show the sky of a weather (TW_CLEAR_DAY .. TW_PARTLY_NIGHT); TW_PLAIN and TW_HIDDEN: nothing.
// calm (the clock face): half the drops and flakes (falling as fast), lightning about once a minute, drifting,
// turning and twinkling slower.
// talpy_out: Talpy is out (any picture but TW_HIDDEN): drops and flakes stop on him and the fog's lamp glow and beam
// sit on his lamp; false: he is down in his burrow, they stop on its mound and only the glow shows, on the lamp
// peeking out of it.
// The same sky, calm and talpy_out as shown (talpy_out only matters to fog, drizzle, rain, snow and thunder): nothing
// changes (its animations keep their rhythm).
void weather_sky_set(weather_sky_t *s, tw_mood_t sky, bool calm, bool talpy_out);

// on: every animation stops where it is (the shapes stay; no lightning, no flash callback); off: they go on from
// there. A sky set while frozen comes standing still.
void weather_sky_freeze(weather_sky_t *s, bool on);

// cb(user) at each lightning flash (as the thunderstorm's bolt lights up), from the LVGL loop. NULL: none.
void weather_sky_on_flash(weather_sky_t *s, void (*cb)(void *user), void *user);

#ifdef __cplusplus
} /*extern "C"*/
#endif
