#include "check.h"
#include "ui/talpy_codec.h"
#include "ui/talpy_set.h"
#include <stdlib.h>

// The generated Talpy data (src/ui/talpy_data.c): every frame decodes to what the PC tool made, the tables hang
// together, and the weights are Talpy Den's.

static void test_every_frame_decodes(void) {
    for (uint16_t i = 0; i < TALPY_SET.frame_count; i++) {
        const talpy_frame_t *f = &TALPY_SET.frames[i];
        const size_t len = (size_t)f->w * f->h * 3;
        uint8_t *buf = malloc(len);
        CHECK(buf != NULL);
        if (!buf)
            return;
        CHECK(f->offset + f->size <= TALPY_SET.data_size);
        if (f->offset + f->size <= TALPY_SET.data_size) {
            const bool ok = talpy_decode(TALPY_SET.data + f->offset, f->size, f->w, f->h, buf);
            CHECK(ok);
            if (ok)
                CHECK(talpy_fnv1a(buf, len) == f->check);
        }
        free(buf);
    }
}

static void test_tables_consistent(void) {
    const talpy_set_t *s = &TALPY_SET;
    for (uint16_t i = 0; i < s->step_count; i++)
        CHECK(s->steps[i].frame < s->frame_count);
    for (uint16_t c = 0; c < s->clip_count; c++) {
        const talpy_clip_t *clip = &s->clips[c];
        CHECK(clip->steps > 0);
        CHECK(clip->first_step + clip->steps <= s->step_count);
        if (clip->first_step + clip->steps > s->step_count)
            continue;
        for (uint16_t k = 0; k < clip->steps; k++) {
            const talpy_step_t *step = &s->steps[clip->first_step + k];
            if (step->frame >= s->frame_count)
                continue;
            CHECK(s->frames[step->frame].w == s->w && s->frames[step->frame].h == s->h);
        }
    }
    for (uint16_t i = 0; i < s->pick_count; i++)
        CHECK(s->picks[i].clip < s->clip_count);
    for (int m = 0; m < s->mood_count; m++) {
        CHECK(s->moods[m].rest_clip < s->clip_count);
        CHECK(s->moods[m].picks >= 1);
        CHECK(s->moods[m].first_pick + s->moods[m].picks <= s->pick_count);
        CHECK(s->moods[m].pause_min_ms <= s->moods[m].pause_max_ms);
    }
    CHECK(s->react_clip < s->clip_count);
    CHECK(s->sleep_still < s->frame_count);
    CHECK(s->h <= 112);
    CHECK(s->data_size <= 2000000);
}

static unsigned total_weight(talpy_mood_t m) {
    unsigned total = 0;
    for (uint16_t i = 0; i < TALPY_SET.moods[m].picks; i++)
        total += TALPY_SET.picks[TALPY_SET.moods[m].first_pick + i].weight;
    return total;
}

static void test_weights_from_talpy_den(void) {
    // needs: alert 25, sign 20, wave 8 (the data doesn't name its clips: each weight once)
    const talpy_mood_def_t *needs = &TALPY_SET.moods[TALPY_NEEDS];
    CHECK(needs->picks == 3);
    if (needs->picks == 3) {
        int seen25 = 0, seen20 = 0, seen8 = 0;
        for (uint16_t i = 0; i < 3; i++) {
            const uint16_t w = TALPY_SET.picks[needs->first_pick + i].weight;
            seen25 += w == 25;
            seen20 += w == 20;
            seen8 += w == 8;
        }
        CHECK(seen25 == 1 && seen20 == 1 && seen8 == 1);
    }
    CHECK(total_weight(TALPY_NEEDS) == 53);
    CHECK(total_weight(TALPY_WORKING) == 68);
    CHECK(total_weight(TALPY_DONE) == 36);
    CHECK(total_weight(TALPY_LOADING) == 10);
}

int main(void) {
    test_every_frame_decodes();
    test_tables_consistent();
    test_weights_from_talpy_den();
    return check_failures() ? 1 : 0;
}
