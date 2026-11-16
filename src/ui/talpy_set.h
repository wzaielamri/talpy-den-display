#ifndef TALPY_SET_H
#define TALPY_SET_H

#include <stdint.h>

// Talpy's pictures for the display, made by tools/make_talpy_frames.py from Talpy Den's own drawing code (the data
// is src/ui/talpy_data.c). Pure C: built for the board and by host gcc for the tests.
//
// Each frame is compressed (see talpy_codec.h); a clip is a list of steps (a frame shown for so many ms); each mood
// has a rest clip looped during its pauses and a weighted pick of clips, as Talpy Den's MOODS.

typedef enum { TALPY_LOADING, TALPY_NEEDS, TALPY_WORKING, TALPY_DONE, TALPY_MOODS } talpy_mood_t;

typedef struct { uint32_t offset, size; uint16_t w, h; uint32_t check; } talpy_frame_t; // into data; check: FNV-1a of the decoded frame
typedef struct { uint16_t frame; uint16_t ms; } talpy_step_t;                          // show frame for ms
typedef struct { uint16_t first_step, steps; } talpy_clip_t;                            // steps[first_step .. +steps)
typedef struct { uint16_t clip; uint16_t weight; } talpy_pick_t;                        // weight: Talpy Den's weight × 10
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
    talpy_mood_def_t moods[TALPY_MOODS];
    uint16_t finished_clip;              // the reaction
    uint16_t sleep_still;                // a frame (cropped to itself) for the clock face
} talpy_set_t;

extern const talpy_set_t TALPY_SET;

#endif
