#ifndef TALPY_PLAYER_H
#define TALPY_PLAYER_H

#include "ui/talpy_set.h"
#include <stdbool.h>
#include <stdint.h>

// Decides which of Talpy's frames to show at a given time. Pure C: no clock of its own (the caller passes
// milliseconds, compared wrap-safe), so the host tests can drive it by hand.
//
// A mood rests (its rest clip loops) for a random pause, then plays one clip picked by weight, then rests again.
// A reaction plays the finished clip once, then the mood goes on resting.

typedef struct {
    const talpy_set_t *set;
    uint32_t rng;       // xorshift32 state, never 0
    talpy_mood_t mood;
    int16_t clip;       // playing clip, or -1: resting (the mood's rest clip loops)
    bool reacting;      // the playing clip is the reaction
    uint16_t step;      // step within the playing clip or the rest clip
    uint32_t step_due;  // when the current step ends
    uint32_t pause_due; // while resting: when the next clip starts
} talpy_player_t;

void talpy_player_init(talpy_player_t *p, const talpy_set_t *set, uint32_t seed, uint32_t now_ms); // TALPY_LOADING, resting
void talpy_player_set_mood(talpy_player_t *p, talpy_mood_t mood, uint32_t now_ms); // same mood: nothing; else rest of the new mood, a new pause, a reaction stops
bool talpy_player_react(talpy_player_t *p, uint32_t now_ms); // starts finished_clip; false and nothing while mood is TALPY_NEEDS
bool talpy_player_tick(talpy_player_t *p, uint32_t now_ms);  // true if the frame to show changed
uint16_t talpy_player_frame(const talpy_player_t *p);        // index into set->frames

#endif
