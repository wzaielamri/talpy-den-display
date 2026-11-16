#include "ui/talpy_player.h"

// Seed 0 would leave xorshift32 at 0 forever, so it maps to a fixed non-zero value.
#define SEED_FALLBACK 0x9E3779B9u

static uint32_t next_random(talpy_player_t *p) {
    uint32_t x = p->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    p->rng = x;
    return x;
}

static bool due(uint32_t now, uint32_t when) {
    return (int32_t)(now - when) >= 0;
}

static const talpy_step_t *step_of(const talpy_player_t *p, uint16_t clip, uint16_t step) {
    return &p->set->steps[p->set->clips[clip].first_step + step];
}

static uint16_t current_clip(const talpy_player_t *p) {
    return p->clip >= 0 ? (uint16_t)p->clip : p->set->moods[p->mood].rest_clip;
}

// Rests in the current mood: the rest clip's first step shows at once and a new pause is drawn.
static void start_rest(talpy_player_t *p, uint32_t now) {
    const talpy_mood_def_t *m = &p->set->moods[p->mood];
    uint32_t span = (uint32_t)(m->pause_max_ms - m->pause_min_ms) + 1;
    p->clip = -1;
    p->reacting = false;
    p->step = 0;
    p->step_due = now + step_of(p, m->rest_clip, 0)->ms;
    p->pause_due = now + m->pause_min_ms + next_random(p) % span;
}

// Starts a clip chosen by weight from the mood's picks.
static void start_pick(talpy_player_t *p, uint32_t now) {
    const talpy_mood_def_t *m = &p->set->moods[p->mood];
    uint32_t total = 0;
    for (uint16_t i = 0; i < m->picks; i++)
        total += p->set->picks[m->first_pick + i].weight;
    uint32_t r = total ? next_random(p) % total : 0;
    uint16_t chosen = p->set->picks[m->first_pick].clip;
    for (uint16_t i = 0; i < m->picks; i++) {
        const talpy_pick_t *pick = &p->set->picks[m->first_pick + i];
        if (r < pick->weight) {
            chosen = pick->clip;
            break;
        }
        r -= pick->weight;
    }
    p->clip = (int16_t)chosen;
    p->step = 0;
    p->step_due = now + step_of(p, chosen, 0)->ms;
}

void talpy_player_init(talpy_player_t *p, const talpy_set_t *set, uint32_t seed, uint32_t now_ms) {
    p->set = set;
    p->rng = seed ? seed : SEED_FALLBACK;
    p->mood = TALPY_LOADING;
    start_rest(p, now_ms);
}

void talpy_player_set_mood(talpy_player_t *p, talpy_mood_t mood, uint32_t now_ms) {
    if (mood == p->mood)
        return;
    p->mood = mood;
    start_rest(p, now_ms);
}

bool talpy_player_react(talpy_player_t *p, uint32_t now_ms) {
    if (p->mood == TALPY_NEEDS)
        return false;
    p->clip = (int16_t)p->set->finished_clip;
    p->reacting = true;
    p->step = 0;
    p->step_due = now_ms + step_of(p, p->set->finished_clip, 0)->ms;
    return true;
}

bool talpy_player_tick(talpy_player_t *p, uint32_t now_ms) {
    uint16_t before = talpy_player_frame(p);
    if (p->clip < 0 && due(now_ms, p->pause_due)) {
        // The pause ends even in the middle of a rest step.
        start_pick(p, now_ms);
    } else if (due(now_ms, p->step_due)) {
        uint16_t clip = current_clip(p);
        p->step++;
        if (p->step < p->set->clips[clip].steps) {
            p->step_due = now_ms + step_of(p, clip, p->step)->ms;
        } else if (p->clip >= 0) {
            start_rest(p, now_ms);
        } else {
            p->step = 0; // the rest clip loops
            p->step_due = now_ms + step_of(p, clip, 0)->ms;
        }
    }
    return talpy_player_frame(p) != before;
}

uint16_t talpy_player_frame(const talpy_player_t *p) {
    return step_of(p, current_clip(p), p->step)->frame;
}
