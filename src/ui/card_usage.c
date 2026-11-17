#include "card_usage.h"
#include "app/clock.h"
#include "theme.h"
#include "ui_common.h"
#include <stdio.h>
#include <string.h>

// As in Talpy Den: a window's bar is green, orange from USAGE_HIGH, red from USAGE_FULL (its % too).
#define USAGE_HIGH 70
#define USAGE_FULL 90
#define RECHECK_MS 30000 // how often the shown windows are looked at again: a reset passed, "Tue" -> "18:00"
#define NOT_SHOWN  (-2)  // no % on screen yet (-1 is "--")

// The card is a grid: a row per window (the 5-hour, the week), named in a column of their own on the left, and a
// column per account, headed by its name in the title line. Two accounts: two columns side by side; one: its
// column across both, its bars longer, its % bigger and its plan after its name.
//
// Spec: the card's inside is INNER_W wide ("USAGE" takes its first 54 px of the title line); the rows' names fit
// NAMES_W ("Week" is 42 px); the two accounts' columns are COL_W wide at COL_X, 14 px apart (so each % sits nearer
// its own bar than the next column's), one account's from COL_X[0] to the right edge. Window c's row at y ROW_Y(c).
#define INNER_W  240 // the card's 280 less its padding
#define NAMES_W  58
#define COL_W    84
#define ROW_Y(c) (24 + 40 * (c))
static const int COL_X[2] = {NAMES_W, INNER_W - COL_W};
static const char *const ROW_NAME[2] = {"5h", "Week"};

// One account's column: its head, then for each window (5-hour, week) a bar, its % and when it resets, and that
// window's numbers as the PC sent them, kept so the card can move on with the clock by itself.
typedef struct {
    lv_obj_t *head;
    lv_obj_t *bar[2];
    lv_obj_t *pct[2];
    lv_obj_t *resets[2];
    int sent_pct[2];      // the % the PC sent; -1: not known
    time_t sent_reset[2]; // when it starts again, Unix seconds; 0: not known
    int shown_pct[2];     // the % on screen now (NOT_SHOWN: none yet)
} usage_col_t;

static lv_obj_t *use_title;
static lv_obj_t *use_empty;       // "Waiting for the PC", then "No accounts" while there are none
static lv_obj_t *row_names[2];    // "5h", "Week"
static usage_col_t wide;          // one account: its column across both
static usage_col_t cols[USAGE_ROWS]; // two accounts: a column each
static int accounts_shown;        // 0, 1 (wide) or 2 (cols)

// Spec: a thin meter, tile-coloured track, its fill coloured by how full it is (usage_color).
static lv_obj_t *make_meter(lv_obj_t *parent, int x, int y, int w, int h) {
    lv_obj_t *bar = lv_bar_create(parent);
    lv_obj_remove_style_all(bar); // no padding: the fill takes the whole height
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_CLICKABLE); // (a tap goes through to the screen: double tap, screen_main.c)
    lv_obj_set_size(bar, w, h);
    lv_obj_set_pos(bar, x, y);
    lv_bar_set_range(bar, 0, 100);
    lv_obj_set_style_bg_color(bar, lv_color_hex(COLOR_TILE), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, h / 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, lv_color_hex(COLOR_DONE), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, h / 2, LV_PART_INDICATOR);
    return bar;
}

static void set_shown(lv_obj_t *obj, bool shown) {
    if (shown)
        lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

static lv_obj_t *make_line_label(lv_obj_t *parent, uint32_t color, int x, int y, int w) {
    lv_obj_t *l = make_label(parent, &lv_font_montserrat_14, color, "");
    lv_obj_set_size(l, w, lv_font_get_line_height(&lv_font_montserrat_14));
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(l, x, y);
    return l;
}

// Spec: an account's column at x, w wide, its head right-aligned over its %s (clear of "USAGE"). In each window's
// row: the bar, centred on the row's first line, then the % right-aligned at the column's right edge (big:
// montserrat_24, 64 wide, "100%" is 61 px, its digits level with the bar; else montserrat_14, 38 wide, "100%" is
// 35 px), and the reset time under the bar (as wide as the column: "↻ 20:25" is 54 px).
static usage_col_t make_column(lv_obj_t *parent, int x, int w, bool big) {
    usage_col_t col;
    const int pct_w = big ? 64 : 38, bar_h = big ? 10 : 6;
    col.head = make_line_label(parent, COLOR_MUTED, x, 0, w);
    lv_obj_set_style_text_align(col.head, LV_TEXT_ALIGN_RIGHT, 0);
    for (int c = 0; c < 2; c++) {
        const int y = ROW_Y(c);
        col.bar[c] = make_meter(parent, x, y + 8 - bar_h / 2, w - pct_w - 6, bar_h);
        col.pct[c] = make_label(parent, big ? &lv_font_montserrat_24 : &lv_font_montserrat_14, COLOR_TEXT, "");
        lv_obj_set_width(col.pct[c], pct_w);
        lv_obj_set_style_text_align(col.pct[c], LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_pos(col.pct[c], x + w - pct_w, big ? y - 6 : y);
        col.resets[c] = make_line_label(parent, COLOR_DIM, x, y + 18, w);
        col.sent_pct[c] = -1;
        col.sent_reset[c] = 0;
        col.shown_pct[c] = NOT_SHOWN;
    }
    return col;
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

// A window's colour: green, orange (the waiting colour) from USAGE_HIGH, red from USAGE_FULL.
static uint32_t usage_color(int pct) {
    return pct >= USAGE_FULL ? COLOR_LIMIT : pct >= USAGE_HIGH ? COLOR_WAIT : COLOR_DONE;
}

// One window of an account, from what the PC sent and the clock now: the bar, the % and their colour
// ("--" while it isn't known), and its reset. The % is in the text colour while the bar is green, else
// in the bar's colour. Once its reset time has passed the window started again: 0% and no reset time,
// till the PC sends the next reading (which has the new reset time). Changes only what
// differs from what's on screen: called again every RECHECK_MS, it mostly draws nothing.
static void show_window(usage_col_t *col, int c) {
    int pct = col->sent_pct[c];
    time_t at = col->sent_reset[c], now = time(NULL);
    if (pct > 0 && at > 0 && now >= CLOCK_SET && now >= at) pct = 0;
    if (pct > 100) pct = 100;

    char text[24];
    format_reset(text, sizeof text, at); // ("" once it has passed)
    if (strcmp(lv_label_get_text(col->resets[c]), text) != 0) lv_label_set_text(col->resets[c], text);

    if (pct == col->shown_pct[c]) return;
    col->shown_pct[c] = pct;
    if (pct < 0) {
        lv_bar_set_value(col->bar[c], 0, LV_ANIM_OFF);
        lv_obj_set_style_text_color(col->pct[c], lv_color_hex(COLOR_MUTED), 0);
        lv_label_set_text(col->pct[c], "--");
        return;
    }
    uint32_t color = usage_color(pct);
    lv_bar_set_value(col->bar[c], pct, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(col->bar[c], lv_color_hex(color), LV_PART_INDICATOR);
    lv_obj_set_style_text_color(col->pct[c], lv_color_hex(pct >= USAGE_HIGH ? color : COLOR_TEXT), 0);
    lv_label_set_text_fmt(col->pct[c], "%d%%", pct);
}

// An account in col (a NULL: col hidden), headed by its name, and its plan after it ("work • Pro") if plan: the
// title line has no room for two accounts' plans.
static void show_column(usage_col_t *col, const usage_account_t *a, bool plan) {
    lv_obj_t *parts[] = {col->head,   col->bar[0], col->pct[0], col->resets[0],
                         col->bar[1], col->pct[1], col->resets[1]};
    for (unsigned k = 0; k < sizeof parts / sizeof parts[0]; k++)
        set_shown(parts[k], a != NULL);
    if (!a) return;
    const char *name = a->name && a->name[0] ? a->name : "--";
    if (plan && a->plan && a->plan[0])
        lv_label_set_text_fmt(col->head, "%s " LV_SYMBOL_BULLET " %s", name, a->plan);
    else
        lv_label_set_text(col->head, name);
    col->sent_pct[0] = a->five_hour;
    col->sent_reset[0] = a->five_hour_resets;
    col->sent_pct[1] = a->seven_day;
    col->sent_reset[1] = a->seven_day_resets;
    show_window(col, 0);
    show_window(col, 1);
}

void card_usage_set(const usage_account_t *accounts, int n) {
    if (!use_title) return; // card not created yet
    if (!accounts || n < 0) n = 0;
    if (n > USAGE_ROWS) n = USAGE_ROWS;
    for (int c = 0; c < 2; c++)
        set_shown(row_names[c], n > 0);
    if (n > 0) {
        lv_obj_add_flag(use_empty, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_label_set_text(use_empty, "No accounts");
        lv_obj_remove_flag(use_empty, LV_OBJ_FLAG_HIDDEN);
    }
    show_column(&wide, n == 1 ? &accounts[0] : NULL, true);
    for (int i = 0; i < USAGE_ROWS; i++)
        show_column(&cols[i], n >= 2 ? &accounts[i] : NULL, false);
    accounts_shown = n;
}

// Every RECHECK_MS: the shown windows again, with the clock moved on (a reset passed, a day's name
// that becomes a time), between the PC's messages, which come only when a reading changes.
static void recheck(lv_timer_t *timer) {
    LV_UNUSED(timer);
    for (int c = 0; c < 2; c++) {
        if (accounts_shown == 1)
            show_window(&wide, c);
        else
            for (int i = 0; i < accounts_shown; i++)
                show_window(&cols[i], c);
    }
}

void card_usage_create(lv_obj_t *parent) {
    use_title = make_card_title(parent, "USAGE");

    for (int c = 0; c < 2; c++) {
        row_names[c] = make_label(parent, &lv_font_montserrat_14, COLOR_TEXT, ROW_NAME[c]);
        lv_obj_set_pos(row_names[c], 0, ROW_Y(c));
    }
    wide = make_column(parent, COL_X[0], INNER_W - COL_X[0], true);
    for (int i = 0; i < USAGE_ROWS; i++)
        cols[i] = make_column(parent, COL_X[i], COL_W, false);

    // Spec: use_empty, where the first row goes. Until the PC's first usage message: nothing made up, the card
    // says it waits.
    use_empty = make_label(parent, &lv_font_montserrat_14, COLOR_MUTED, "");
    lv_obj_set_pos(use_empty, 0, ROW_Y(0));
    card_usage_set(NULL, 0);
    lv_label_set_text(use_empty, "Waiting for the PC");
    lv_timer_create(recheck, RECHECK_MS, NULL);
}
