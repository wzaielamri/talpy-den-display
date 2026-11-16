#ifndef TALPY_PLAYER_H
#define TALPY_PLAYER_H

#include "ui/talpy_set.h"
#include <stdbool.h>
#include <stdint.h>

// Decides which of Talpy's frames to show at a given time. Pure C: no clock of its own (the caller passes
// milliseconds, compared wrap-safe), so the host tests can drive it by hand.
//
// A mood rests (its rest clip loops) for a random pause, then plays one clip picked by weight, then rests again.
// A mood with no picks rests forever. A clip played once (a reaction, or any clip by index) ends in the mood's rest.
// The pace stretches the drawn pauses (1 to 8 times).

typedef struct {
    const talpy_set_t *set;
    uint32_t rng;       // xorshift32 state, never 0
    uint16_t mood;      // index into set->moods
    uint8_t pace;       // pauses are drawn × pace
    int16_t clip;       // playing clip, or -1: resting (the mood's rest clip loops)
    bool reacting;      // the playing clip is the reaction (talpy_player_react)
    uint16_t step;      // step within the playing clip or the rest clip
    uint32_t step_due;  // when the current step ends
    uint32_t pause_due; // while resting: when the next clip starts
} talpy_player_t;

// TALPY_LOADING, resting
void talpy_player_init(talpy_player_t *p, const talpy_set_t *set, uint32_t seed, uint32_t now_ms);
// Same mood: nothing; else rest of the new mood, a new pause, a reaction stops.
void talpy_player_set_mood(talpy_player_t *p, uint16_t mood, uint32_t now_ms);
// Plays a clip once, then rests in the mood; a mood change stops it. False and nothing if clip >= clip_count.
bool talpy_player_play_once(talpy_player_t *p, uint16_t clip, uint32_t now_ms);
// The banner's reaction: plays react_clip once; false and nothing while mood is TALPY_NEEDS.
bool talpy_player_react(talpy_player_t *p, uint32_t now_ms);
// Drawn pauses × pace, 1 to 8 (clamped); default 1. Takes effect from the next pause drawn.
void talpy_player_set_pace(talpy_player_t *p, uint8_t pace);
bool talpy_player_tick(talpy_player_t *p, uint32_t now_ms);  // true if the frame to show changed
uint16_t talpy_player_frame(const talpy_player_t *p);        // index into set->frames

#endif
