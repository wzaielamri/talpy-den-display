#include "card_jobs.h"
#include "app/clock.h"
#include "banner_text.h"
#include "job_order.h"
#include "talpy.h"
#include "talpy_set.h"
#include "theme.h"
#include "ui_common.h"
#include <stdint.h>
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
static lv_obj_t *jb_list;  // the rows, one under the other, in a list that scrolls
static lv_obj_t *jb_empty; // in the rows' place: "No jobs right now" while none
static count_pair_t pair_wait, pair_work, pair_done;
static job_row_t rows[JOBS_MAX]; // made as jobs come (row is NULL till then), kept hidden when they go
static int made;                 // rows made so far
static int shown;                // rows shown now: rows[0 .. shown-1]
static lv_timer_t *top_timer;    // the list back to the top, TOP_AFTER_MS after the last touch on it

#define TOP_AFTER_MS 30000
#define KEY_MAX 160 // a job's id kept (with its 0), as job_watch.c (ID_MAX there): one this long or longer isn't tappable

// What each shown row is about, kept so its "how long" moves on without a new message, and which job it is.
static job_state_t row_state[JOBS_MAX];
static time_t row_since[JOBS_MAX];
static char row_key[JOBS_MAX][KEY_MAX]; // its id, or name@machine without one (as job_watch.c)
static char row_id[JOBS_MAX][KEY_MAX];  // its id ("" without one: the row can't be tapped)

// A tap on a row asks the PC to bring its job up (app/pc_open.h). The id is taken when the finger lands, so a
// jobs message that moves the rows before it lifts can't change which job opens.
#define RESULT_MS 4000 // how long "Opened" or why not stays on the row
#define NOTE_W    200  // the right side's width while it says that (the name gives way)
#define NAME_W    280
static card_jobs_open_cb_t open_cb;
static char pressed_id[KEY_MAX];
// What a row says in place of its state for a while, one at a time (one open at a time): "Opening...", then the
// answer for RESULT_MS. Shown on whichever shown row has note_id, so on none once that job is gone.
static char note_id[KEY_MAX];
static char note_text[96];
static uint32_t note_color;
static lv_timer_t *note_timer;

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

// Spec: 432 x 48, one under the other in the list, no background, padding 0, 1 px divider at the bottom.
static void row_pressed(lv_event_t *e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    snprintf(pressed_id, sizeof pressed_id, "%s", row_id[i]);
    lv_timer_reset(top_timer); // (a touch on a row is a touch on the list: back to the top 30 s after it)
    lv_timer_resume(top_timer);
}

// LVGL sends no click after a scroll: a swipe over the rows opens nothing.
static void row_clicked(lv_event_t *e) {
    LV_UNUSED(e);
    if (pressed_id[0] && open_cb) open_cb(pressed_id);
    pressed_id[0] = 0;
}

static job_row_t make_job_row(lv_obj_t *parent, int i) {
    job_row_t r;
    r.row = lv_obj_create(parent);
    lv_obj_set_size(r.row, lv_pct(100), 48);
    lv_obj_set_style_bg_opa(r.row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(r.row, 0, 0);
    lv_obj_set_style_pad_all(r.row, 0, 0);
    lv_obj_set_style_border_side(r.row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(r.row, lv_color_hex(COLOR_LINE), 0);
    lv_obj_set_style_border_width(r.row, 1, 0);
    // Spec: pressed, the tile colour. Tappable (it asks the PC to bring its job up); a drag on it scrolls the list
    // (not scrollable itself, it passes the scroll on).
    lv_obj_set_style_bg_color(r.row, lv_color_hex(COLOR_TILE), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(r.row, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_remove_flag(r.row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(r.row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(r.row, row_pressed, LV_EVENT_PRESSED, (void *)(intptr_t)i);
    lv_obj_add_event_cb(r.row, row_clicked, LV_EVENT_CLICKED, NULL);

    r.dot = make_blob(r.row, 0, 18, 12, 12, LV_RADIUS_CIRCLE, COLOR_WORKING);

    // Fixed one-line height: LONG_DOT only adds "..." when the text overflows the height.
    r.name = make_label(r.row, &lv_font_montserrat_24, COLOR_TEXT, "");
    lv_obj_set_size(r.name, NAME_W, lv_font_get_line_height(&lv_font_montserrat_24));
    lv_label_set_long_mode(r.name, LV_LABEL_LONG_DOT);
    lv_obj_align(r.name, LV_ALIGN_TOP_LEFT, 24, 10);

    // Spec: where it runs, small under the name (the name moves up to y 4 then). Hidden without one.
    r.machine = make_label(r.row, &lv_font_montserrat_14, COLOR_MUTED, "");
    lv_obj_set_size(r.machine, 280, lv_font_get_line_height(&lv_font_montserrat_14));
    lv_label_set_long_mode(r.machine, LV_LABEL_LONG_DOT);
    lv_obj_align(r.machine, LV_ALIGN_TOP_LEFT, 24, 29);
    lv_obj_add_flag(r.machine, LV_OBJ_FLAG_HIDDEN);

    r.state = make_label(r.row, &lv_font_montserrat_14, COLOR_WORKING, "");
    lv_label_set_long_mode(r.state, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(r.state, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(r.state, LV_ALIGN_RIGHT_MID, 0, 0);
    return r;
}

// Divider under every shown row except the last one.
static void update_dividers(void) {
    for (int i = 0; i < shown; i++)
        lv_obj_set_style_border_width(rows[i].row, i + 1 < shown ? 1 : 0, 0);
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
    char text[96];
    time_t now = time(NULL);
    bool note = note_id[0] && strcmp(row_id[i], note_id) == 0;
    // The right side: content-wide for the state, NOTE_W (dotted) for a note, the name giving way to it.
    int32_t state_w = note ? NOTE_W : LV_SIZE_CONTENT, name_w = note ? 432 - 24 - NOTE_W - 8 : NAME_W;
    if (lv_obj_get_style_width(rows[i].state, 0) != state_w) {
        lv_obj_set_width(rows[i].state, state_w);
        lv_obj_set_height(rows[i].state, lv_font_get_line_height(&lv_font_montserrat_14));
        lv_obj_set_width(rows[i].name, name_w);
        lv_obj_align(rows[i].state, LV_ALIGN_RIGHT_MID, 0, 0);
    }
    lv_color_t c = lv_color_hex(note ? note_color : state_color(row_state[i]));
    if (!lv_color_eq(lv_obj_get_style_text_color(rows[i].state, 0), c))
        lv_obj_set_style_text_color(rows[i].state, c, 0);
    if (note) {
        snprintf(text, sizeof text, "%s", note_text);
    } else if (row_since[i] > 0 && now >= CLOCK_SET) {
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
    for (int i = 0; i < shown; i++)
        update_state_text(i);
}

// Row i: coloured dot, name with where it runs under it, and "<state> • <how long>" on the right.
// A job's key, as job_watch.c keys it: its id, or name@machine without one.
static void job_key(const job_info_t *j, char *out, size_t size) {
    if (j->id && j->id[0])
        snprintf(out, size, "%s", j->id);
    else
        snprintf(out, size, "%s@%s", j->name ? j->name : "", j->machine ? j->machine : "");
}

static void show_row(int i, const job_info_t *job) {
    if (i >= made) {
        rows[made] = make_job_row(jb_list, made); // (in order: rows[i] only after rows[i-1])
        made++;
    }
    job_row_t *r = &rows[i];
    job_key(job, row_key[i], KEY_MAX);
    // (an id that doesn't fit whole isn't kept: cut short it would name another chat, or none; the row isn't tappable)
    bool fits = job->id && strlen(job->id) < KEY_MAX - 1;
    snprintf(row_id[i], KEY_MAX, "%s", fits ? job->id : "");
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
    update_state_text(i); // (its text and colour, a note's while one shows for this job)
    lv_obj_remove_flag(r->row, LV_OBJ_FLAG_HIDDEN);
}

void card_jobs_counts(const job_update_t *u, int *waiting, int *working, int *done) {
    *waiting = *working = *done = 0;
    int n = u && u->jobs ? u->count : 0;
    for (int i = 0; i < n; i++) {
        switch (u->jobs[i].state) {
        case JOB_WORKING: (*working)++; break;
        case JOB_WAITING: (*waiting)++; break;
        case JOB_DONE:    (*done)++;    break;
        }
    }
}

// Whether this key was a waiting row's before the rows are filled again (rows[0 .. shown-1] are still the last
// message's then).
static bool was_waiting(const char *key) {
    for (int i = 0; i < shown; i++)
        if (row_state[i] == JOB_WAITING && strcmp(row_key[i], key) == 0) return true;
    return false;
}

void card_jobs_set(const job_update_t *u) {
    if (!u || !jb_list) return; // no data, or card not created yet
    int n = u->jobs ? u->count : 0;
    if (n > JOBS_MAX) n = JOBS_MAX; // (sync.py sends no more)

    // The banner counts every job.
    int working, waiting, done;
    card_jobs_counts(u, &waiting, &working, &done);
    set_banner(true, waiting, working, done);
    char stamp[32];
    snprintf(stamp, sizeof stamp, "updated %s", u->updated ? u->updated : "--:--:--");
    set_text_if_changed(jb_updated, stamp);

    // A job that started waiting since the last message: the list back to its top, where it is.
    bool new_waiting = false;
    char key[KEY_MAX];
    for (int i = 0; i < n && !new_waiting; i++) {
        if (u->jobs[i].state != JOB_WAITING) continue;
        job_key(&u->jobs[i], key, sizeof key);
        new_waiting = !was_waiting(key);
    }

    // Fill the rows from the top, in job_order's order: waiting jobs first, then the newest started. The list
    // keeps where it was scrolled (only a job starting to wait takes it to the top).
    static int order[JOBS_MAX];
    int ordered = job_order(u->jobs, n, order);
    for (int k = 0; k < ordered; k++)
        show_row(k, &u->jobs[order[k]]);
    for (int i = ordered; i < made; i++)
        lv_obj_add_flag(rows[i].row, LV_OBJ_FLAG_HIDDEN);
    shown = ordered;
    update_dividers();
    if (new_waiting) card_jobs_scroll_top();

    if (shown == 0) {
        lv_obj_remove_flag(jb_empty, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(jb_empty, LV_OBJ_FLAG_HIDDEN);
    }
}

void card_jobs_on_open(card_jobs_open_cb_t cb) {
    open_cb = cb;
}

// The shown rows of note_id say their note, or their state again.
static void refresh_note_rows(const char *id) {
    for (int i = 0; i < shown; i++)
        if (strcmp(row_id[i], id) == 0) update_state_text(i);
}

static void note_timeout(lv_timer_t *timer) {
    LV_UNUSED(timer);
    note_timer = NULL; // (a one-shot timer: LVGL deletes it after this)
    char id[KEY_MAX];
    snprintf(id, sizeof id, "%s", note_id);
    note_id[0] = 0;
    refresh_note_rows(id);
}

static void set_note(const char *id, const char *text, uint32_t color, bool lasts) {
    if (note_timer) {
        lv_timer_delete(note_timer);
        note_timer = NULL;
    }
    if (note_id[0] && strcmp(note_id, id) != 0) { // (another job's note goes)
        char old[KEY_MAX];
        snprintf(old, sizeof old, "%s", note_id);
        note_id[0] = 0;
        refresh_note_rows(old);
    }
    snprintf(note_id, sizeof note_id, "%s", id);
    snprintf(note_text, sizeof note_text, "%s", text);
    note_color = color;
    refresh_note_rows(note_id);
    if (!lasts) {
        note_timer = lv_timer_create(note_timeout, RESULT_MS, NULL);
        lv_timer_set_repeat_count(note_timer, 1);
    }
}

void card_jobs_opening(const char *id) {
    if (id && id[0]) set_note(id, "Opening...", COLOR_MUTED, true);
}

void card_jobs_open_result(const char *id, bool ok, const char *message) {
    if (id && id[0]) set_note(id, ok ? "Opened" : message ? message : "", ok ? COLOR_DONE : COLOR_MUTED, false);
}

void card_jobs_scroll_top(void) {
    if (!jb_list) return;
    lv_obj_scroll_to_y(jb_list, 0, LV_ANIM_ON);
    if (top_timer) lv_timer_pause(top_timer);
}

static void top_timeout(lv_timer_t *timer) {
    LV_UNUSED(timer);
    card_jobs_scroll_top();
}

// A touch on the list, or a scroll that ends: back to the top TOP_AFTER_MS later, unless touched again.
static void list_touched(lv_event_t *e) {
    LV_UNUSED(e);
    lv_timer_reset(top_timer);
    lv_timer_resume(top_timer);
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
    lv_obj_set_size(banner_headline_label, 432 - 16 - TALPY_SET.w - 16,
                    lv_font_get_line_height(&lv_font_montserrat_24));
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

    // Spec: jb_list, the rows' place under the banner, 432 x 212 at (0, 124): transparent, no padding, the rows
    // one under the other (a hidden row takes no room), scrolling up and down only, its scrollbar showing only
    // while it scrolls. Clickable, or LVGL wouldn't let a finger scroll it: so a tap there doesn't reach the
    // screen's double tap (the clock face), as on the weather card. Rows are made as jobs come (show_row).
    jb_list = lv_obj_create(parent);
    lv_obj_set_pos(jb_list, 0, 124);
    lv_obj_set_size(jb_list, 432, 212);
    lv_obj_set_style_bg_opa(jb_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(jb_list, 0, 0);
    lv_obj_set_style_radius(jb_list, 0, 0);
    lv_obj_set_style_pad_all(jb_list, 0, 0);
    lv_obj_set_style_pad_row(jb_list, 0, 0);
    lv_obj_set_flex_flow(jb_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(jb_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(jb_list, LV_SCROLLBAR_MODE_ACTIVE);
    lv_obj_add_event_cb(jb_list, list_touched, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(jb_list, list_touched, LV_EVENT_SCROLL_END, NULL);
    top_timer = lv_timer_create(top_timeout, TOP_AFTER_MS, NULL);
    lv_timer_pause(top_timer);

    lv_timer_create(age_tick, 10000, NULL);

    // Spec: jb_empty, in the middle of the rows' place; shown when data came with no jobs. Until the PC's
    // first jobs message nothing is made up (no rows, no "No jobs"): the banner says it waits.
    jb_empty = make_label(parent, &lv_font_montserrat_14, COLOR_MUTED, "No jobs right now");
    lv_obj_align(jb_empty, LV_ALIGN_TOP_MID, 0, 212);
    lv_obj_add_flag(jb_empty, LV_OBJ_FLAG_HIDDEN);

    set_banner(false, 0, 0, 0);
}
