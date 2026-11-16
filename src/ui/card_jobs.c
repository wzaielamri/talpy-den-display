#include "card_jobs.h"
#include "app/clock.h"
#include "banner_text.h"
#include "talpy.h"
#include "talpy_set.h"
#include "theme.h"
#include "ui_common.h"
#include <stdio.h>
#include <string.h>

// One count in the banner: state-coloured dot and "<n> <word>" next to it.
typedef struct {
    lv_obj_t *dot;
    lv_obj_t *text;
} count_pair_t;

// One job row: dot, name (and where it runs under it), state text on the right.
typedef struct {
    lv_obj_t *row;
    lv_obj_t *dot;
    lv_obj_t *name;
    lv_obj_t *machine;
    lv_obj_t *state;
} job_row_t;

static lv_obj_t *banner;
static lv_obj_t *banner_headline_label;
static lv_obj_t *banner_talpy;
static lv_obj_t *jb_updated;
static lv_obj_t *jb_more;
static lv_obj_t *jb_empty; // in the rows' place: "Waiting for the PC", then "No jobs right now" while none
static count_pair_t pair_wait, pair_work, pair_done;
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

// One dot + text pair of the counts row (a flex child: the row places it, so two-digit counts re-flow).
static count_pair_t make_count_pair(lv_obj_t *row, uint32_t color) {
    count_pair_t p;
    p.dot = make_blob(row, 0, 0, 10, 10, LV_RADIUS_CIRCLE, color);
    p.text = make_label(row, &lv_font_montserrat_14, COLOR_MUTED, "");
    lv_obj_set_style_margin_right(p.text, 10, 0); // 6 px dot to text (row gap) + 10 = 16 px to the next pair
    return p;
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

void card_jobs_freeze(bool on) {
    talpy_freeze(banner_talpy, on);
}

// Sets a label's text only when it changed: a jobs message that changes nothing then redraws little.
static void set_text_if_changed(lv_obj_t *label, const char *text) {
    if (strcmp(lv_label_get_text(label), text) != 0)
        lv_label_set_text(label, text);
}

static void set_count(count_pair_t *p, int n, const char *word) {
    char text[24];
    snprintf(text, sizeof text, "%d %s", n, word);
    set_text_if_changed(p->text, text);
}

// The banner for these counts: headline, counts row, waiting colours, Talpy's mood.
static void set_banner(bool have_data, int waiting, int working, int done) {
    char head[40];
    banner_headline(head, sizeof head, have_data, waiting, working, done);
    set_text_if_changed(banner_headline_label, head);
    set_count(&pair_wait, waiting, "waiting");
    set_count(&pair_work, working, "working");
    set_count(&pair_done, done, "done");

    // While a chat waits the banner is amber (so it catches the eye from across the desk); the border is
    // always 2 px, only its opacity changes, so nothing moves.
    bool alert = waiting > 0;
    lv_obj_set_style_bg_color(banner, lv_color_hex(alert ? COLOR_ALERT : COLOR_TILE), 0);
    lv_obj_set_style_border_opa(banner, alert ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_color(banner_headline_label, lv_color_hex(alert ? COLOR_WAIT : COLOR_TEXT), 0);
    talpy_set_mood(banner_talpy, banner_mood(have_data, waiting, working, done));
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

    // The banner counts every job, also the ones that don't get a row.
    int working = 0, waiting = 0, done = 0;
    for (int i = 0; i < n; i++) {
        switch (u->jobs[i].state) {
        case JOB_WORKING: working++; break;
        case JOB_WAITING: waiting++; break;
        case JOB_DONE:    done++;    break;
        }
    }
    set_banner(true, waiting, working, done);
    char stamp[32];
    snprintf(stamp, sizeof stamp, "updated %s", u->updated ? u->updated : "--:--:--");
    set_text_if_changed(jb_updated, stamp);

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
        lv_obj_remove_flag(jb_empty, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(jb_empty, LV_OBJ_FLAG_HIDDEN);
    }
}

// Spec: the banner, 432 x 116 at (0, 0), radius 12, padding 0, 2 px border (amber, shown only while a chat
// waits). Inside (measured from inside the border): headline montserrat 24 at (16, 14); counts row at (16, 54);
// "updated" at (16, 84); Talpy (96 x 104) at the bottom right, 12 px in and 2 px up.
static void make_banner(lv_obj_t *parent) {
    banner = make_blob(parent, 0, 0, 432, 116, 12, COLOR_TILE);
    lv_obj_set_style_pad_all(banner, 0, 0);
    lv_obj_set_style_border_width(banner, 2, 0);
    lv_obj_set_style_border_color(banner, lv_color_hex(COLOR_WAIT), 0);
    lv_obj_set_style_border_opa(banner, LV_OPA_TRANSP, 0); // (till a chat waits)

    // Fixed one-line height: LONG_DOT only adds "..." when the text overflows the height. The width stops
    // short of Talpy.
    banner_headline_label = make_label(banner, &lv_font_montserrat_24, COLOR_TEXT, "");
    lv_obj_set_size(banner_headline_label, 432 - 16 - TALPY_SET.w - 16, lv_font_get_line_height(&lv_font_montserrat_24));
    lv_label_set_long_mode(banner_headline_label, LV_LABEL_LONG_DOT);
    lv_obj_align(banner_headline_label, LV_ALIGN_TOP_LEFT, 16, 14);

    // Counts row: three dot + text pairs, 16 px apart, left to right: waiting, working, done.
    lv_obj_t *counts = lv_obj_create(banner);
    lv_obj_set_size(counts, LV_SIZE_CONTENT, 18);
    lv_obj_align(counts, LV_ALIGN_TOP_LEFT, 16, 54);
    lv_obj_set_style_bg_opa(counts, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(counts, 0, 0);
    lv_obj_set_style_pad_all(counts, 0, 0);
    lv_obj_set_style_pad_column(counts, 6, 0);
    lv_obj_remove_flag(counts, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_flex_flow(counts, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(counts, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    pair_wait = make_count_pair(counts, COLOR_WAIT);
    pair_work = make_count_pair(counts, COLOR_WORKING);
    pair_done = make_count_pair(counts, COLOR_DONE);

    jb_updated = make_label(banner, &lv_font_montserrat_14, COLOR_DIM, "updated --:--:--");
    lv_obj_align(jb_updated, LV_ALIGN_TOP_LEFT, 16, 84);

    banner_talpy = talpy_create(banner);
    lv_obj_align(banner_talpy, LV_ALIGN_BOTTOM_RIGHT, -12, -2);
}

void card_jobs_create(lv_obj_t *parent) {
    make_banner(parent);

    for (int i = 0; i < JOBS_ROWS; i++) {
        rows[i] = make_job_row(parent, i);
        lv_obj_add_flag(rows[i].row, LV_OBJ_FLAG_HIDDEN); // (till the PC's first message)
    }

    // Spec: under the rows, lined up with the job names. Hidden while every job has a row.
    jb_more = make_label(parent, &lv_font_montserrat_14, COLOR_MUTED, "");
    lv_obj_align(jb_more, LV_ALIGN_BOTTOM_LEFT, 24, 0);
    lv_obj_add_flag(jb_more, LV_OBJ_FLAG_HIDDEN);

    lv_timer_create(age_tick, 10000, NULL);

    // Spec: jb_empty, in the middle of the rows' place; shown when data came with no jobs. Until the PC's
    // first jobs message nothing is made up (no rows, no "No jobs"): the banner says it waits.
    jb_empty = make_label(parent, &lv_font_montserrat_14, COLOR_MUTED, "No jobs right now");
    lv_obj_align(jb_empty, LV_ALIGN_TOP_MID, 0, 212);
    lv_obj_add_flag(jb_empty, LV_OBJ_FLAG_HIDDEN);

    set_banner(false, 0, 0, 0);
}
