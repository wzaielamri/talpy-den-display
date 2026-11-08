#include "card_usage.h"
#include "theme.h"
#include "ui_common.h"
#include <stdio.h>

#define USAGE_HIGH 80        // from here a window's bar and % turn amber
#define CLOCK_SET 1700000000 // before this the board's clock was never set (as clock.c): no reset times

// One account: its name and plan, then for each window (5-hour, week) a bar, its % and when it resets.
typedef struct {
    lv_obj_t *name;
    lv_obj_t *plan;
    lv_obj_t *bar[2];
    lv_obj_t *pct[2];
    lv_obj_t *resets[2];
} usage_row_t;

static lv_obj_t *use_title;
static usage_row_t rows[USAGE_ROWS];

// Spec: each window's column starts at this x: its bar (36 x 6), then its % (42 wide, right-aligned:
// "100%" is ~38 px in montserrat_14, and a narrower label wraps the "%" to a second line); its reset
// time under them.
static const int COL_X[2] = {72, 160};
static const char *const COL_HEAD[2] = {"5h", "7d"};

// Spec: a thin meter, tile-coloured track, filled in the working colour (amber when high).
static lv_obj_t *make_meter(lv_obj_t *parent, int x, int y) {
    lv_obj_t *bar = lv_bar_create(parent);
    lv_obj_remove_style_all(bar); // no padding: the fill takes the whole height
    lv_obj_set_size(bar, 36, 6);
    lv_obj_set_pos(bar, x, y);
    lv_bar_set_range(bar, 0, 100);
    lv_obj_set_style_bg_color(bar, lv_color_hex(COLOR_TILE), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 3, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, lv_color_hex(COLOR_WORKING), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, 3, LV_PART_INDICATOR);
    return bar;
}

static lv_obj_t *make_line_label(lv_obj_t *parent, uint32_t color, int x, int y, int w) {
    lv_obj_t *l = make_label(parent, &lv_font_montserrat_14, color, "");
    lv_obj_set_size(l, w, lv_font_get_line_height(&lv_font_montserrat_14));
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(l, x, y);
    return l;
}

// Spec: account i's first line at y 26 + 40*i, its second 18 below; each bar 5 px lower than its
// line's text (centred on it).
static usage_row_t make_row(lv_obj_t *parent, int i) {
    usage_row_t r;
    int y = 26 + 40 * i;
    r.name = make_line_label(parent, COLOR_TEXT, 0, y, 64);
    r.plan = make_line_label(parent, COLOR_DIM, 0, y + 18, 64);
    for (int c = 0; c < 2; c++) {
        r.bar[c] = make_meter(parent, COL_X[c], y + 5);
        r.pct[c] = make_label(parent, &lv_font_montserrat_14, COLOR_TEXT, "");
        lv_obj_set_width(r.pct[c], 42);
        lv_obj_set_style_text_align(r.pct[c], LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_pos(r.pct[c], COL_X[c] + 38, y);
        r.resets[c] = make_line_label(parent, COLOR_DIM, COL_X[c], y + 18, 80);
    }
    return r;
}

// "↻ 18:00" for a reset within a day, else its day, "↻ Tue"; "" while it (or the clock) isn't known.
static void format_reset(char *buf, size_t size, time_t at) {
    time_t now = time(NULL);
    buf[0] = 0;
    if (at <= 0 || now < CLOCK_SET || at < now) return;
    struct tm tm;
    localtime_r(&at, &tm);
    char when[12];
    strftime(when, sizeof when, at - now < 24 * 3600 ? "%H:%M" : "%a", &tm);
    snprintf(buf, size, LV_SYMBOL_REFRESH " %s", when);
}

// One window of an account: the bar, the % and their colour ("--" while it isn't known), and its reset.
static void show_window(usage_row_t *r, int c, int pct, time_t resets) {
    char text[24];
    format_reset(text, sizeof text, resets);
    lv_label_set_text(r->resets[c], text);
    if (pct < 0) {
        lv_bar_set_value(r->bar[c], 0, LV_ANIM_OFF);
        lv_obj_set_style_text_color(r->pct[c], lv_color_hex(COLOR_MUTED), 0);
        lv_label_set_text(r->pct[c], "--");
        return;
    }
    if (pct > 100) pct = 100;
    bool high = pct >= USAGE_HIGH;
    lv_bar_set_value(r->bar[c], pct, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(r->bar[c], lv_color_hex(high ? COLOR_WAIT : COLOR_WORKING), LV_PART_INDICATOR);
    lv_obj_set_style_text_color(r->pct[c], lv_color_hex(high ? COLOR_WAIT : COLOR_TEXT), 0);
    lv_label_set_text_fmt(r->pct[c], "%d%%", pct);
}

void card_usage_set(const usage_account_t *accounts, int n) {
    if (!use_title) return; // card not created yet
    for (int i = 0; i < USAGE_ROWS; i++) {
        usage_row_t *r = &rows[i];
        lv_obj_t *parts[] = {r->name, r->plan, r->bar[0], r->pct[0], r->resets[0], r->bar[1], r->pct[1], r->resets[1]};
        bool shown = accounts && i < n;
        for (unsigned k = 0; k < sizeof parts / sizeof parts[0]; k++) {
            if (shown)
                lv_obj_remove_flag(parts[k], LV_OBJ_FLAG_HIDDEN);
            else
                lv_obj_add_flag(parts[k], LV_OBJ_FLAG_HIDDEN);
        }
        if (!shown) continue;
        const usage_account_t *a = &accounts[i];
        lv_label_set_text(r->name, a->name && a->name[0] ? a->name : "--");
        lv_label_set_text(r->plan, a->plan ? a->plan : "");
        show_window(r, 0, a->five_hour, a->five_hour_resets);
        show_window(r, 1, a->seven_day, a->seven_day_resets);
    }
}

void card_usage_create(lv_obj_t *parent) {
    use_title = make_card_title(parent, "USAGE");

    // The windows' names over their columns, in the title's line.
    for (int c = 0; c < 2; c++) {
        lv_obj_t *head = make_label(parent, &lv_font_montserrat_14, COLOR_DIM, COL_HEAD[c]);
        lv_obj_set_pos(head, COL_X[c], 0);
    }

    for (int i = 0; i < USAGE_ROWS; i++)
        rows[i] = make_row(parent, i);

    // Made-up values from the design page until your sync.py sends real ones (no resets while the
    // clock isn't set: they'd be made up too).
    time_t now = time(NULL);
    const usage_account_t example[] = {
        {"personal", "Pro",  40, 84,  now + 3 * 3600, now + 4 * 86400},
        {"work",     "Team", 12, 100, now + 2 * 3600, now + 86400 + 3600},
    };
    card_usage_set(example, 2);
}
