#include "card_jobs.h"
#include "app/clock.h"
#include "theme.h"
#include "ui_common.h"
#include <stdio.h>
#include <string.h>

// One count tile: big number on top, caption below.
typedef struct {
    lv_obj_t *tile;
    lv_obj_t *num;
    lv_obj_t *cap;
} count_tile_t;

// One job row: dot, name (and where it runs under it), state text on the right.
typedef struct {
    lv_obj_t *row;
    lv_obj_t *dot;
    lv_obj_t *name;
    lv_obj_t *machine;
    lv_obj_t *state;
} job_row_t;

static lv_obj_t *jb_title;
static lv_obj_t *jb_updated;
static lv_obj_t *jb_more;
static lv_obj_t *jb_empty; // in the rows' place: "Waiting for the PC", then "No jobs right now" while none
static count_tile_t tile_work, tile_wait, tile_done;
static job_row_t rows[JOBS_ROWS];

// What each shown row is about, kept so its "how long" moves on without a new message.
static job_state_t row_state[JOBS_ROWS];
static time_t row_since[JOBS_ROWS];

static uint32_t state_color(job_state_t s) {
    switch (s) {
    case JOB_WAITING: return COLOR_WAIT;
    case JOB_DONE:    return COLOR_DONE;
    default:          return COLOR_WORKING;
    }
}

static const char *state_word(job_state_t s) {
    switch (s) {
    case JOB_WAITING: return "waiting";
    case JOB_DONE:    return "done";
    default:          return "working";
    }
}

// Spec: 136 x 88 at (x, 28), radius 12, padding 12. Number font 40 at 0,0; caption font 14 at 0,48.
static count_tile_t make_count_tile(lv_obj_t *parent, int x, const char *caption, uint32_t color) {
    count_tile_t t;
    t.tile = make_blob(parent, x, 28, 136, 88, 12, COLOR_TILE);
    lv_obj_set_style_pad_all(t.tile, 12, 0);

    t.num = make_label(t.tile, &lv_font_montserrat_40, color, "0");
    lv_obj_align(t.num, LV_ALIGN_TOP_LEFT, 0, 0);

    t.cap = make_label(t.tile, &lv_font_montserrat_14, COLOR_MUTED, caption);
    lv_obj_align(t.cap, LV_ALIGN_TOP_LEFT, 0, 48);
    return t;
}

// Spec: 432 x 48 at (0, 124 + 48*i), no background, padding 0, 1 px divider at the bottom.
static job_row_t make_job_row(lv_obj_t *parent, int i) {
    job_row_t r;
    r.row = lv_obj_create(parent);
    lv_obj_set_size(r.row, lv_pct(100), 48);
    lv_obj_set_pos(r.row, 0, 124 + 48 * i);
    lv_obj_set_style_bg_opa(r.row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(r.row, 0, 0);
    lv_obj_set_style_pad_all(r.row, 0, 0);
    lv_obj_set_style_border_side(r.row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(r.row, lv_color_hex(COLOR_LINE), 0);
    lv_obj_set_style_border_width(r.row, 1, 0);
    // (not clickable: a tap goes through to the screen, which counts double taps: screen_main.c)
    lv_obj_remove_flag(r.row, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    r.dot = make_blob(r.row, 0, 18, 12, 12, LV_RADIUS_CIRCLE, COLOR_WORKING);

    // Fixed one-line height: LONG_DOT only adds "..." when the text overflows the height.
    r.name = make_label(r.row, &lv_font_montserrat_24, COLOR_TEXT, "");
    lv_obj_set_size(r.name, 280, lv_font_get_line_height(&lv_font_montserrat_24));
    lv_label_set_long_mode(r.name, LV_LABEL_LONG_DOT);
    lv_obj_align(r.name, LV_ALIGN_TOP_LEFT, 24, 10);

    // Spec: where it runs, small under the name (the name moves up to y 4 then). Hidden without one.
    r.machine = make_label(r.row, &lv_font_montserrat_14, COLOR_MUTED, "");
    lv_obj_set_size(r.machine, 280, lv_font_get_line_height(&lv_font_montserrat_14));
    lv_label_set_long_mode(r.machine, LV_LABEL_LONG_DOT);
    lv_obj_align(r.machine, LV_ALIGN_TOP_LEFT, 24, 29);
    lv_obj_add_flag(r.machine, LV_OBJ_FLAG_HIDDEN);

    r.state = make_label(r.row, &lv_font_montserrat_14, COLOR_WORKING, "");
    lv_obj_align(r.state, LV_ALIGN_RIGHT_MID, 0, 0);
    return r;
}

// Divider under every shown row except the last shown one.
static void update_dividers(void) {
    for (int i = 0; i < JOBS_ROWS; i++) {
        bool next_shown = i + 1 < JOBS_ROWS && !lv_obj_has_flag(rows[i + 1].row, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_border_width(rows[i].row, next_shown ? 1 : 0, 0);
    }
}

// The waiting tile's border opacity at a point of its breath (0..360 degrees): full at 0, faint (20%)
// at 180, full again at 360. A cosine, so the end of one breath is exactly the start of the next: no
// turning point where a step could show (a back-and-forth animation turns around at each end).
static void anim_breath(void *obj, int32_t deg) {
    int32_t mid = (LV_OPA_COVER + LV_OPA_20) / 2, amp = (LV_OPA_COVER - LV_OPA_20) / 2;
    int32_t opa = mid + ((amp * lv_trigo_cos((int16_t)deg)) >> LV_TRIGO_SHIFT);
    lv_obj_set_style_border_opa((lv_obj_t *)obj, (lv_opa_t)opa, 0);
}

static bool wants_pulse; // a job waits for you
static bool frozen;      // the pull-down panel is out: nothing moves under its dimming

// While a job waits for you, the waiting tile's amber border breathes, so it catches the eye from
// across the desk (held at full while frozen); with none waiting it's gone.
static void apply_pulse(void) {
    static bool pulsing;
    bool run = wants_pulse && !frozen;
    if (run == pulsing) return; // already so: a running breath keeps its rhythm
    pulsing = run;
    if (!run) {
        lv_anim_delete(tile_wait.tile, anim_breath);
        lv_obj_set_style_border_opa(tile_wait.tile, wants_pulse ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        return;
    }
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, tile_wait.tile);
    lv_anim_set_exec_cb(&a, anim_breath);
    lv_anim_set_values(&a, 0, 360);
    lv_anim_set_duration(&a, 1800); // one breath
    lv_anim_set_path_cb(&a, lv_anim_path_linear);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

static void pulse_waiting(bool on) {
    if (on == wants_pulse) return;
    wants_pulse = on;
    if (!on) lv_obj_set_style_border_opa(tile_wait.tile, LV_OPA_TRANSP, 0); // (also while frozen)
    apply_pulse();
}

void card_jobs_freeze(bool on) {
    frozen = on;
    apply_pulse();
}

static void set_counts(int working, int waiting, int done) {
    lv_label_set_text_fmt(tile_work.num, "%d", working);
    lv_label_set_text_fmt(tile_wait.num, "%d", waiting);
    lv_label_set_text_fmt(tile_done.num, "%d", done);
    pulse_waiting(waiting > 0);
}

// How long, in the unit that fits: "<1m", "3m", "2h", "4d"; a done job's as "just now", "5m ago".
static void format_age(char *buf, size_t size, job_state_t state, long long age) {
    bool done = state == JOB_DONE;
    if (age < 60) {
        snprintf(buf, size, "%s", done ? "just now" : "<1m");
        return;
    }
    int n = age < 3600 ? (int)(age / 60) : age < 86400 ? (int)(age / 3600) : (int)(age / 86400);
    char unit = age < 3600 ? 'm' : age < 86400 ? 'h' : 'd';
    snprintf(buf, size, done ? "%d%c ago" : "%d%c", n, unit);
}

// Row i's right side: "waiting • 3m", or the state word alone while since or the clock isn't known.
// Sets the text only when it changed: the 10 s refresh then redraws nothing most of the time.
static void update_state_text(int i) {
    char text[40];
    time_t now = time(NULL);
    if (row_since[i] > 0 && now >= CLOCK_SET) {
        char age[16];
        long long secs = (long long)(now - row_since[i]);
        format_age(age, sizeof age, row_state[i], secs < 0 ? 0 : secs); // (the PC's clock a bit ahead)
        snprintf(text, sizeof text, "%s " LV_SYMBOL_BULLET " %s", state_word(row_state[i]), age);
    } else {
        snprintf(text, sizeof text, "%s", state_word(row_state[i]));
    }
    if (strcmp(lv_label_get_text(rows[i].state), text) != 0)
        lv_label_set_text(rows[i].state, text);
}

// Every 10 s: the shown rows' "how long" moves on by itself, between the PC's messages.
static void age_tick(lv_timer_t *timer) {
    LV_UNUSED(timer);
    for (int i = 0; i < JOBS_ROWS; i++)
        if (!lv_obj_has_flag(rows[i].row, LV_OBJ_FLAG_HIDDEN))
            update_state_text(i);
}

// Row i: coloured dot, name with where it runs under it, and "<state> • <how long>" on the right.
static void show_row(int i, const job_info_t *job) {
    job_row_t *r = &rows[i];
    lv_color_t c = lv_color_hex(state_color(job->state));
    bool two_lines = job->machine && job->machine[0];

    row_state[i] = job->state;
    row_since[i] = job->since;
    lv_obj_set_style_bg_color(r->dot, c, 0);
    lv_label_set_text(r->name, job->name ? job->name : "--");
    lv_obj_align(r->name, LV_ALIGN_TOP_LEFT, 24, two_lines ? 4 : 10);
    if (two_lines) {
        lv_label_set_text(r->machine, job->machine);
        lv_obj_remove_flag(r->machine, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(r->machine, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_set_style_text_color(r->state, c, 0);
    update_state_text(i);
    lv_obj_remove_flag(r->row, LV_OBJ_FLAG_HIDDEN);
}

void card_jobs_set(const job_update_t *u) {
    if (!u || !jb_more) return; // no data, or card not created yet
    int n = u->jobs ? u->count : 0;

    // The tiles count every job, also the ones that don't get a row.
    int working = 0, waiting = 0, done = 0;
    for (int i = 0; i < n; i++) {
        switch (u->jobs[i].state) {
        case JOB_WORKING: working++; break;
        case JOB_WAITING: waiting++; break;
        case JOB_DONE:    done++;    break;
        }
    }
    set_counts(working, waiting, done);
    lv_label_set_text_fmt(jb_updated, "updated %s", u->updated ? u->updated : "--:--:--");

    // Fill the rows from the top: waiting jobs first, then working, then done.
    static const job_state_t order[] = {JOB_WAITING, JOB_WORKING, JOB_DONE};
    int shown = 0;
    for (int k = 0; k < 3; k++)
        for (int i = 0; i < n && shown < JOBS_ROWS; i++)
            if (u->jobs[i].state == order[k])
                show_row(shown++, &u->jobs[i]);
    for (int i = shown; i < JOBS_ROWS; i++)
        lv_obj_add_flag(rows[i].row, LV_OBJ_FLAG_HIDDEN);
    update_dividers();

    int more = working + waiting + done - shown;
    if (more > 0) {
        lv_label_set_text_fmt(jb_more, "+%d more in Talpy Den", more);
        lv_obj_remove_flag(jb_more, LV_OBJ_FLAG_HIDDEN);
        // (a divider under the last row too, or "+N more" reads as one more line of that job)
        lv_obj_set_style_border_width(rows[shown - 1].row, 1, 0);
    } else {
        lv_obj_add_flag(jb_more, LV_OBJ_FLAG_HIDDEN);
    }

    if (shown == 0) {
        lv_label_set_text(jb_empty, "No jobs right now");
        lv_obj_remove_flag(jb_empty, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(jb_empty, LV_OBJ_FLAG_HIDDEN);
    }
}

void card_jobs_create(lv_obj_t *parent) {
    jb_title = make_card_title(parent, "CLAUDE JOBS");

    jb_updated = make_label(parent, &lv_font_montserrat_14, COLOR_DIM, "");
    lv_obj_align(jb_updated, LV_ALIGN_TOP_RIGHT, 0, 0);

    //                          parent   x   caption    colour
    tile_work = make_count_tile(parent,   0, "working", COLOR_WORKING);
    tile_wait = make_count_tile(parent, 148, "waiting", COLOR_WAIT);
    tile_done = make_count_tile(parent, 296, "done",    COLOR_DONE);

    // Waiting tile border: always 2 px wide (only its opacity changes) with 2 px less padding,
    // so the number sits exactly where it does in the other tiles and never moves.
    lv_obj_set_style_border_width(tile_wait.tile, 2, 0);
    lv_obj_set_style_border_color(tile_wait.tile, lv_color_hex(COLOR_WAIT), 0);
    lv_obj_set_style_border_opa(tile_wait.tile, LV_OPA_TRANSP, 0); // (till a job waits)
    lv_obj_set_style_pad_all(tile_wait.tile, 10, 0);

    for (int i = 0; i < JOBS_ROWS; i++)
        rows[i] = make_job_row(parent, i);

    // Spec: under the rows, lined up with the job names. Hidden while every job has a row.
    jb_more = make_label(parent, &lv_font_montserrat_14, COLOR_MUTED, "");
    lv_obj_align(jb_more, LV_ALIGN_BOTTOM_LEFT, 24, 0);

    lv_timer_create(age_tick, 10000, NULL);

    // Spec: jb_empty, in the middle of the rows' place. Until the PC's first jobs message: nothing
    // made up (0 0 0, no rows), the card says it waits.
    jb_empty = make_label(parent, &lv_font_montserrat_14, COLOR_MUTED, "");
    lv_obj_align(jb_empty, LV_ALIGN_TOP_MID, 0, 212);
    job_update_t u = {NULL, 0, NULL};
    card_jobs_set(&u);
    lv_label_set_text(jb_empty, "Waiting for the PC");
}
