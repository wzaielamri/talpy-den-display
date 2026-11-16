#ifndef TALPY_SET_H
#define TALPY_SET_H

#include <stdint.h>

// Talpy's pictures for the display, made by tools/make_talpy_frames.py from Talpy Den's own drawing code (the data:
// src/ui/talpy_data.c, the banner's set, and src/ui/talpy_weather_data.c, the weather set). Pure C: built for the
// board and by host gcc for the tests.
//
// Each frame is compressed (see talpy_codec.h); a clip is a list of steps (a frame shown for so many ms); each mood
// has a rest clip looped during its pauses and a weighted pick of clips, as Talpy Den's MOODS.

// The banner set's moods; a set has any number of moods (mood_count), a player's mood is an index into them.
typedef enum { TALPY_LOADING, TALPY_NEEDS, TALPY_WORKING, TALPY_DONE, TALPY_MOODS } talpy_mood_t;
// The weather set's moods.
typedef enum {
    TW_CLEAR_DAY, TW_PARTLY_DAY, TW_OVERCAST, TW_FOG, TW_DRIZZLE, TW_RAIN, TW_SNOW, TW_THUNDER,
    TW_CLEAR_NIGHT, TW_PARTLY_NIGHT, TW_PLAIN, TW_HIDDEN, TW_MOODS
} tw_mood_t;

// into data; check: FNV-1a of the decoded frame
typedef struct { uint32_t offset, size; uint16_t w, h; uint32_t check; } talpy_frame_t;
typedef struct { uint16_t frame; uint16_t ms; } talpy_step_t;    // show frame for ms
typedef struct { uint16_t first_step, steps; } talpy_clip_t;      // steps[first_step .. +steps)
typedef struct { uint16_t clip; uint16_t weight; } talpy_pick_t;  // weight: Talpy Den's weight × 10
typedef struct {
    uint16_t rest_clip;                  // looped during pauses
    uint16_t first_pick, picks;          // picks[first_pick .. +picks)
    uint16_t pause_min_ms, pause_max_ms; // Talpy Den's MOODS[m].pause
} talpy_mood_def_t;

typedef struct {
    const uint8_t *data;                 // all compressed frames
    uint32_t data_size;
    uint16_t w, h;                       // the player's canvas: every frame a clip or rest step names has this size
    const talpy_frame_t *frames; uint16_t frame_count;
    const talpy_step_t *steps;   uint16_t step_count;
    const talpy_clip_t *clips;   uint16_t clip_count;
    const talpy_pick_t *picks;   uint16_t pick_count;
    const talpy_mood_def_t *moods; uint16_t mood_count;
    uint16_t react_clip;                 // the reaction: the banner's "finished", the weather set's thunder duck
    int16_t x, y;                        // where the canvas sits in its scene box
} talpy_set_t;

extern const talpy_set_t TALPY_SET;
extern const talpy_set_t TALPY_WEATHER_SET;

#endif
