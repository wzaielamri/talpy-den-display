#include "card_jobs.h"
#include "theme.h"
#include "ui_common.h"

// One count tile: big number on top, caption below.
typedef struct {
    lv_obj_t *tile;
    lv_obj_t *num;
    lv_obj_t *cap;
} count_tile_t;

// One job row: dot, name, state text on the right.
typedef struct {
    lv_obj_t *row;
    lv_obj_t *dot;
    lv_obj_t *name;
    lv_obj_t *state;
} job_row_t;

static lv_obj_t *jb_title;
static lv_obj_t *jb_updated;
static count_tile_t tile_work, tile_wait, tile_done;
static job_row_t rows[JOBS_ROWS];

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

// Spec: 432 x 48 at (0, 132 + 48*i), no background, padding 0, 1 px divider at the bottom.
static job_row_t make_job_row(lv_obj_t *parent, int i) {
    job_row_t r;
    r.row = lv_obj_create(parent);
    lv_obj_set_size(r.row, lv_pct(100), 48);
    lv_obj_set_pos(r.row, 0, 132 + 48 * i);
    lv_obj_set_style_bg_opa(r.row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(r.row, 0, 0);
    lv_obj_set_style_pad_all(r.row, 0, 0);
    lv_obj_set_style_border_side(r.row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(r.row, lv_color_hex(COLOR_LINE), 0);
    lv_obj_set_style_border_width(r.row, 1, 0);
    lv_obj_remove_flag(r.row, LV_OBJ_FLAG_SCROLLABLE);

    r.dot = make_blob(r.row, 0, 18, 12, 12, LV_RADIUS_CIRCLE, COLOR_WORKING);

    // Fixed one-line height: LONG_DOT only adds "..." when the text overflows the height.
    r.name = make_label(r.row, &lv_font_montserrat_24, COLOR_TEXT, "");
    lv_obj_set_size(r.name, 280, lv_font_get_line_height(&lv_font_montserrat_24));
    lv_label_set_long_mode(r.name, LV_LABEL_LONG_DOT);
    lv_obj_align(r.name, LV_ALIGN_TOP_LEFT, 24, 10);

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

void card_jobs_set_updated(const char *time) {
    if (!jb_updated) return; // card not created yet
    lv_label_set_text_fmt(jb_updated, "updated %s", time ? time : "--:--:--");
}

void card_jobs_set_counts(int working, int waiting, int done) {
    if (!tile_wait.tile) return;
    lv_label_set_text_fmt(tile_work.num, "%d", working);
    lv_label_set_text_fmt(tile_wait.num, "%d", waiting);
    lv_label_set_text_fmt(tile_done.num, "%d", done);
    lv_obj_set_style_border_opa(tile_wait.tile, waiting > 0 ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
}

void card_jobs_set_row(int i, const char *name, job_state_t state, const char *detail) {
    if (i < 0 || i >= JOBS_ROWS || !rows[i].row) return;
    job_row_t *r = &rows[i];
    lv_color_t c = lv_color_hex(state_color(state));

    lv_obj_set_style_bg_color(r->dot, c, 0);
    lv_label_set_text(r->name, name ? name : "--");
    lv_obj_set_style_text_color(r->state, c, 0);
    if (detail && detail[0])
        lv_label_set_text_fmt(r->state, "%s " LV_SYMBOL_BULLET " %s", state_word(state), detail);
    else
        lv_label_set_text(r->state, state_word(state));

    lv_obj_remove_flag(r->row, LV_OBJ_FLAG_HIDDEN);
    update_dividers();
}

void card_jobs_hide_row(int i) {
    if (i < 0 || i >= JOBS_ROWS || !rows[i].row) return;
    lv_obj_add_flag(rows[i].row, LV_OBJ_FLAG_HIDDEN);
    update_dividers();
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
    lv_obj_set_style_pad_all(tile_wait.tile, 10, 0);

    for (int i = 0; i < JOBS_ROWS; i++)
        rows[i] = make_job_row(parent, i);

    // Made-up values from the design page until your step-7 jobs code sends real ones.
    card_jobs_set_updated("14:52:05");
    card_jobs_set_counts(2, 1, 1);
    card_jobs_set_row(0, "desk-display-gadget", JOB_WAITING, "3m");
    card_jobs_set_row(1, "quick-chat-next",     JOB_WORKING, "12m");
    card_jobs_set_row(2, "fix-serial-parser",   JOB_WORKING, "41m");
    card_jobs_set_row(3, "release-notes",       JOB_DONE,    "5m ago");
}
