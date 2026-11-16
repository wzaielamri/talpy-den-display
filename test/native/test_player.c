#include "check.h"
#include "ui/talpy_player.h"

// A small hand-made set: no frame data, the player only reads steps, clips, picks and moods.
// Rest frames: loading 0, needs 1, working 2, done 3. Clip A: frames 5,6. Clip B: frame 7. Finished: 8,9.
static const talpy_step_t steps[] = {
    {0, 100}, {1, 100}, {2, 100}, {3, 100}, // 0..3 rest clips
    {5, 100}, {6, 100},                     // 4,5 clip A
    {7, 100},                               // 6 clip B
    {8, 100}, {9, 100},                     // 7,8 finished
};
static const talpy_clip_t clips[] = {
    {0, 1}, {1, 1}, {2, 1}, {3, 1}, // rest clips: loading, needs, working, done
    {4, 2},                         // 4: A
    {6, 1},                         // 5: B
    {7, 2},                         // 6: finished
};
static const talpy_pick_t picks[] = {
    {4, 10},          // loading
    {4, 10},          // needs
    {4, 30}, {5, 10}, // working: A 30, B 10
    {4, 10},          // done
};
static const talpy_mood_def_t moods[] = {
    [TALPY_LOADING] = {0, 0, 1, 500, 1500},
    [TALPY_NEEDS] = {1, 1, 1, 500, 1500},
    [TALPY_WORKING] = {2, 2, 2, 500, 1500},
    [TALPY_DONE] = {3, 4, 1, 500, 1500},
    [TALPY_MOODS] = {2, 0, 0, 500, 1500},     // 4: no picks, rests forever
    [TALPY_MOODS + 1] = {2, 2, 2, 9000, 9000}, // 5: long pauses
    [TALPY_MOODS + 2] = {2, 2, 2, 1000, 1000}, // 6: pause exactly 1000
    [TALPY_MOODS + 3] = {2, 2, 2, 20000, 20000}, // 7: pause exactly 20 s
};
#define M_NOPICKS TALPY_MOODS
#define M_LONG (TALPY_MOODS + 1)
#define M_ONE (TALPY_MOODS + 2)
#define M_20S (TALPY_MOODS + 3)
static const talpy_set_t set = {
    .w = 10,
    .h = 10,
    .steps = steps,
    .step_count = 9,
    .clips = clips,
    .clip_count = 7,
    .picks = picks,
    .pick_count = 5,
    .moods = moods,
    .mood_count = 8,
    .react_clip = 6,
};

// Ticks every 10 ms from *now until the frame is a clip frame (5 or 7); false if it does not within limit_ms.
// Leaves *now at the tick that showed it.
static bool run_to_clip(talpy_player_t *p, uint32_t *now, uint32_t limit_ms) {
    for (uint32_t t = 0; t <= limit_ms; t += 10) {
        talpy_player_tick(p, *now);
        uint16_t f = talpy_player_frame(p);
        if (f == 5 || f == 7)
            return true;
        *now += 10;
    }
    return false;
}

static void test_init_rests_in_loading(void) {
    talpy_player_t p;
    talpy_player_init(&p, &set, 1, 1000);
    CHECK(talpy_player_frame(&p) == 0);
    CHECK(p.mood == TALPY_LOADING);
    CHECK(p.clip == -1);
}

static void test_zero_seed_not_stuck(void) {
    talpy_player_t p;
    talpy_player_init(&p, &set, 0, 0);
    CHECK(p.rng != 0);
}

static void test_set_mood_shows_rest_at_once(void) {
    talpy_player_t p;
    talpy_player_init(&p, &set, 1, 0);
    talpy_player_set_mood(&p, TALPY_WORKING, 50);
    CHECK(talpy_player_frame(&p) == 2);
    CHECK(p.step_due == 150);
    CHECK(p.pause_due - 50 >= 500 && p.pause_due - 50 <= 1500);
}

static void test_clip_starts_after_pause(void) {
    talpy_player_t p;
    talpy_player_init(&p, &set, 7, 0);
    talpy_player_set_mood(&p, TALPY_WORKING, 0);
    bool early = false;
    uint32_t now = 0;
    for (; now < 500; now += 10) {
        talpy_player_tick(&p, now);
        if (talpy_player_frame(&p) != 2)
            early = true;
    }
    CHECK(!early);
    CHECK(run_to_clip(&p, &now, 1000));
}

static void test_clip_plays_its_steps_then_rests(void) {
    // Find a seed that picks clip A, then follow it.
    for (uint32_t seed = 1; seed < 100; seed++) {
        talpy_player_t p;
        talpy_player_init(&p, &set, seed, 0);
        talpy_player_set_mood(&p, TALPY_WORKING, 0);
        uint32_t now = 0;
        CHECK(run_to_clip(&p, &now, 1600));
        if (talpy_player_frame(&p) != 5)
            continue;
        uint32_t start = now;
        talpy_player_tick(&p, start + 90);
        CHECK(talpy_player_frame(&p) == 5);
        talpy_player_tick(&p, start + 100);
        CHECK(talpy_player_frame(&p) == 6);
        talpy_player_tick(&p, start + 190);
        CHECK(talpy_player_frame(&p) == 6);
        talpy_player_tick(&p, start + 200);
        CHECK(talpy_player_frame(&p) == 2);
        CHECK(p.clip == -1);
        return;
    }
    CHECK(!"no seed picked clip A");
}

static void test_weighted_pick(void) {
    int a = 0, total = 4000;
    for (int i = 0; i < total; i++) {
        talpy_player_t p;
        talpy_player_init(&p, &set, 12345u + (uint32_t)i * 2654435761u, 0);
        talpy_player_set_mood(&p, TALPY_WORKING, 0);
        uint32_t now = 0;
        if (!run_to_clip(&p, &now, 2000)) {
            CHECK(!"no clip started");
            return;
        }
        if (talpy_player_frame(&p) == 5)
            a++;
    }
    int pct = a * 100 / total;
    CHECK(pct >= 71 && pct <= 79);
}

static void test_same_mood_keeps_timing(void) {
    talpy_player_t p;
    talpy_player_init(&p, &set, 3, 0);
    talpy_player_set_mood(&p, TALPY_WORKING, 0);
    uint32_t now = 0;
    CHECK(run_to_clip(&p, &now, 1600));
    uint16_t frame = talpy_player_frame(&p);
    uint32_t due = p.step_due;
    talpy_player_set_mood(&p, TALPY_WORKING, now + 30);
    CHECK(talpy_player_frame(&p) == frame);
    CHECK(p.step_due == due);
}

static void test_react_ignored_while_needs(void) {
    talpy_player_t p;
    talpy_player_init(&p, &set, 1, 0);
    talpy_player_set_mood(&p, TALPY_NEEDS, 0);
    CHECK(!talpy_player_react(&p, 10));
    CHECK(talpy_player_frame(&p) == 1);
    CHECK(!p.reacting);
}

static void test_react_plays_then_back_to_mood(void) {
    talpy_player_t p;
    talpy_player_init(&p, &set, 1, 0);
    talpy_player_set_mood(&p, TALPY_WORKING, 0);
    CHECK(talpy_player_react(&p, 10));
    CHECK(talpy_player_frame(&p) == 8);
    CHECK(talpy_player_tick(&p, 110));
    CHECK(talpy_player_frame(&p) == 9);
    CHECK(talpy_player_tick(&p, 210));
    CHECK(talpy_player_frame(&p) == 2);
    CHECK(!p.reacting);
    CHECK(p.mood == TALPY_WORKING);
}

static void test_mood_change_cancels_reaction(void) {
    talpy_player_t p;
    talpy_player_init(&p, &set, 1, 0);
    talpy_player_set_mood(&p, TALPY_WORKING, 0);
    talpy_player_react(&p, 10);
    talpy_player_set_mood(&p, TALPY_NEEDS, 20);
    CHECK(talpy_player_frame(&p) == 1);
    CHECK(!p.reacting);
}

static void test_long_freeze_advances_one_step(void) {
    for (uint32_t seed = 1; seed < 100; seed++) {
        talpy_player_t p;
        talpy_player_init(&p, &set, seed, 0);
        talpy_player_set_mood(&p, TALPY_WORKING, 0);
        uint32_t now = 0;
        CHECK(run_to_clip(&p, &now, 1600));
        if (talpy_player_frame(&p) != 5)
            continue;
        uint32_t late = now + 60000;
        CHECK(talpy_player_tick(&p, late));
        CHECK(talpy_player_frame(&p) == 6);
        CHECK(!talpy_player_tick(&p, late + 90));
        CHECK(talpy_player_tick(&p, late + 100));
        CHECK(talpy_player_frame(&p) == 2);
        return;
    }
    CHECK(!"no seed picked clip A");
}

// A freeze of 30 days (2^31 ms is 24.8 days): the due time is far behind, not in the future.
static void test_30_day_freeze_still_moves(void) {
    talpy_player_t p;
    talpy_player_init(&p, &set, 5, 0);
    talpy_player_set_mood(&p, TALPY_WORKING, 0);
    uint32_t now = 0;
    CHECK(run_to_clip(&p, &now, 1600));
    uint32_t late = now + 30u * 24 * 3600 * 1000;
    talpy_player_tick(&p, late); // (a clip step, or a new pause: either way it moves on)
    uint16_t seen = talpy_player_frame(&p);
    bool moved = false;
    for (uint32_t t = late; t <= late + 5000; t += 10) {
        talpy_player_tick(&p, t);
        if (talpy_player_frame(&p) != seen) moved = true;
    }
    CHECK(moved);
}

static void test_millis_wrap(void) {
    talpy_player_t p;
    uint32_t now = 0xFFFFFF00u; // 256 ms before the wrap; the shortest pause (500 ms) ends after it
    talpy_player_init(&p, &set, 9, now);
    talpy_player_set_mood(&p, TALPY_WORKING, now);
    uint32_t start = now;
    // No clip before pause_min, even though pause_due is numerically tiny after the wrap.
    for (; now - start < 500; now += 10) {
        talpy_player_tick(&p, now);
        CHECK(talpy_player_frame(&p) == 2);
    }
    // A clip starts by pause_max.
    bool started = false;
    for (; now - start <= 1500 + 10; now += 10) {
        talpy_player_tick(&p, now);
        uint16_t f = talpy_player_frame(&p);
        if (f == 5 || f == 7) {
            started = true;
            break;
        }
    }
    CHECK(started);
    // Later clips keep starting.
    int clips_started = 0;
    uint16_t last = talpy_player_frame(&p);
    for (int i = 0; i < 600; i++, now += 10) {
        talpy_player_tick(&p, now);
        uint16_t f = talpy_player_frame(&p);
        if (f != last && (f == 5 || f == 7))
            clips_started++;
        last = f;
    }
    CHECK(clips_started >= 2);
    CHECK(now < 0xFFFFFF00u); // really crossed the wrap
}

static void test_play_once_plays_then_rests(void) {
    talpy_player_t p;
    talpy_player_init(&p, &set, 1, 0);
    talpy_player_set_mood(&p, TALPY_WORKING, 0);
    CHECK(talpy_player_play_once(&p, 5, 10));
    CHECK(talpy_player_frame(&p) == 7);
    CHECK(talpy_player_tick(&p, 110));
    CHECK(talpy_player_frame(&p) == 2);
    CHECK(p.clip == -1);
}

static void test_play_once_bad_clip(void) {
    talpy_player_t p;
    talpy_player_init(&p, &set, 1, 0);
    talpy_player_set_mood(&p, TALPY_WORKING, 0);
    CHECK(!talpy_player_play_once(&p, 99, 10));
    CHECK(talpy_player_frame(&p) == 2);
}

static void test_mood_without_picks_rests(void) {
    talpy_player_t p;
    talpy_player_init(&p, &set, 1, 0);
    talpy_player_set_mood(&p, M_NOPICKS, 0);
    for (uint32_t now = 0; now <= 60000; now += 10) {
        talpy_player_tick(&p, now);
        CHECK(talpy_player_frame(&p) == 2);
    }
}

static void test_slow_pace_not_overdue(void) {
    talpy_player_t p;
    talpy_player_init(&p, &set, 1, 0);
    talpy_player_set_pace(&p, 3);
    talpy_player_set_mood(&p, M_LONG, 0);
    uint32_t now = 0;
    for (; now < 27000; now += 10) {
        talpy_player_tick(&p, now);
        CHECK(talpy_player_frame(&p) == 2);
    }
    CHECK(run_to_clip(&p, &now, 100));
}

// A paced pause longer than 65,535 ms (20 s x pace 8 = 160 s) is not taken for one long past.
static void test_max_pace_long_pause_not_overdue(void) {
    talpy_player_t p;
    talpy_player_init(&p, &set, 1, 0);
    talpy_player_set_pace(&p, 8);
    talpy_player_set_mood(&p, M_20S, 0);
    uint32_t now = 0;
    bool early = false;
    for (; now < 160000; now += 10) {
        talpy_player_tick(&p, now);
        if (talpy_player_frame(&p) != 2)
            early = true;
    }
    CHECK(!early);
    CHECK(run_to_clip(&p, &now, 100));
}

static void pace_clip_time(uint8_t pace, uint32_t expect) {
    talpy_player_t p;
    talpy_player_init(&p, &set, 1, 0);
    talpy_player_set_pace(&p, pace);
    talpy_player_set_mood(&p, M_ONE, 0);
    uint32_t now = 0;
    for (; now < expect; now += 10) {
        talpy_player_tick(&p, now);
        CHECK(talpy_player_frame(&p) == 2);
    }
    talpy_player_tick(&p, now);
    CHECK(talpy_player_frame(&p) != 2);
}

static void test_pace_clamped(void) {
    pace_clip_time(0, 1000);
    pace_clip_time(20, 8000);
}

int main(void) {
    test_play_once_plays_then_rests();
    test_play_once_bad_clip();
    test_mood_without_picks_rests();
    test_slow_pace_not_overdue();
    test_max_pace_long_pause_not_overdue();
    test_pace_clamped();
    test_init_rests_in_loading();
    test_zero_seed_not_stuck();
    test_set_mood_shows_rest_at_once();
    test_clip_starts_after_pause();
    test_clip_plays_its_steps_then_rests();
    test_weighted_pick();
    test_same_mood_keeps_timing();
    test_react_ignored_while_needs();
    test_react_plays_then_back_to_mood();
    test_mood_change_cancels_reaction();
    test_long_freeze_advances_one_step();
    test_30_day_freeze_still_moves();
    test_millis_wrap();
    return check_failures() ? 1 : 0;
}
