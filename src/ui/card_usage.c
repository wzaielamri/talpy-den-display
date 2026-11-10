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

// One account: its name and plan, then for each window (5-hour, week) a bar, its % and when it resets,
// and that window's numbers as the PC sent them, kept so the card can move on with the clock by itself.
typedef struct {
    lv_obj_t *name;
    lv_obj_t *plan;
    lv_obj_t *bar[2];
    lv_obj_t *pct[2];
    lv_obj_t *resets[2];
    int sent_pct[2];      // the % the PC sent; -1: not known
    time_t sent_reset[2]; // when it starts again, Unix seconds; 0: not known
    int shown_pct[2];     // the % on screen now (NOT_SHOWN: none yet)
} usage_row_t;

static lv_obj_t *use_title;
static lv_obj_t *use_heads[2]; // "5h", "7d"
static lv_obj_t *use_empty;    // "Waiting for the PC", then "No accounts" while there are none
static usage_row_t rows[USAGE_ROWS];
static int rows_shown;         // accounts on the card now

// Spec: each window's column starts at this x: its bar (36 x 6), then its % (42 wide, right-aligned:
// "100%" is ~38 px in montserrat_14, and a narrower label wraps the "%" to a second line); its reset
// time under them.
static const int COL_X[2] = {72, 160};
static const char *const COL_HEAD[2] = {"5h", "7d"};

// Spec: a thin meter, tile-coloured track, its fill coloured by how full it is (usage_color).
static lv_obj_t *make_meter(lv_obj_t *parent, int x, int y) {
    lv_obj_t *bar = lv_bar_create(parent);
    lv_obj_remove_style_all(bar); // no padding: the fill takes the whole height
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_CLICKABLE); // (a tap goes through to the screen: double tap, screen_main.c)
    lv_obj_set_size(bar, 36, 6);
    lv_obj_set_pos(bar, x, y);
    lv_bar_set_range(bar, 0, 100);
    lv_obj_set_style_bg_color(bar, lv_color_hex(COLOR_TILE), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 3, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, lv_color_hex(COLOR_DONE), LV_PART_INDICATOR);
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
        r.sent_pct[c] = -1;
        r.sent_reset[c] = 0;
        r.shown_pct[c] = NOT_SHOWN;
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

// A window's colour: green, orange (the waiting colour) from USAGE_HIGH, red from USAGE_FULL.
static uint32_t usage_color(int pct) {
    return pct >= USAGE_FULL ? COLOR_LIMIT : pct >= USAGE_HIGH ? COLOR_WAIT : COLOR_DONE;
}

// One window of an account, from what the PC sent and the clock now: the bar, the % and their colour
// ("--" while it isn't known), and its reset. The % is in the text colour while the bar is green, else
// in the bar's colour. Once its reset time has passed the window started again: 0% and no reset time,
// till the PC sends the next reading (which has the new reset time). Changes only what
// differs from what's on screen: called again every RECHECK_MS, it mostly draws nothing.
static void show_window(usage_row_t *r, int c) {
    int pct = r->sent_pct[c];
    time_t at = r->sent_reset[c], now = time(NULL);
    if (pct > 0 && at > 0 && now >= CLOCK_SET && now >= at) pct = 0;
    if (pct > 100) pct = 100;

    char text[24];
    format_reset(text, sizeof text, at); // ("" once it has passed)
    if (strcmp(lv_label_get_text(r->resets[c]), text) != 0) lv_label_set_text(r->resets[c], text);

    if (pct == r->shown_pct[c]) return;
    r->shown_pct[c] = pct;
    if (pct < 0) {
        lv_bar_set_value(r->bar[c], 0, LV_ANIM_OFF);
        lv_obj_set_style_text_color(r->pct[c], lv_color_hex(COLOR_MUTED), 0);
        lv_label_set_text(r->pct[c], "--");
        return;
    }
    uint32_t color = usage_color(pct);
    lv_bar_set_value(r->bar[c], pct, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(r->bar[c], lv_color_hex(color), LV_PART_INDICATOR);
    lv_obj_set_style_text_color(r->pct[c], lv_color_hex(pct >= USAGE_HIGH ? color : COLOR_TEXT), 0);
    lv_label_set_text_fmt(r->pct[c], "%d%%", pct);
}

void card_usage_set(const usage_account_t *accounts, int n) {
    if (!use_title) return; // card not created yet
    bool any = accounts && n > 0;
    for (int c = 0; c < 2; c++) {
        if (any)
            lv_obj_remove_flag(use_heads[c], LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(use_heads[c], LV_OBJ_FLAG_HIDDEN);
    }
    if (any) {
        lv_obj_add_flag(use_empty, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_label_set_text(use_empty, "No accounts");
        lv_obj_remove_flag(use_empty, LV_OBJ_FLAG_HIDDEN);
    }
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
        r->sent_pct[0] = a->five_hour;
        r->sent_reset[0] = a->five_hour_resets;
        r->sent_pct[1] = a->seven_day;
        r->sent_reset[1] = a->seven_day_resets;
        show_window(r, 0);
        show_window(r, 1);
    }
    rows_shown = accounts ? (n < USAGE_ROWS ? n : USAGE_ROWS) : 0;
}

// Every RECHECK_MS: the shown windows again, with the clock moved on (a reset passed, a day's name
// that becomes a time), between the PC's messages, which come only when a reading changes.
static void recheck(lv_timer_t *timer) {
    LV_UNUSED(timer);
    for (int i = 0; i < rows_shown; i++)
        for (int c = 0; c < 2; c++)
            show_window(&rows[i], c);
}

void card_usage_create(lv_obj_t *parent) {
    use_title = make_card_title(parent, "USAGE");

    // The windows' names over their columns, in the title's line.
    for (int c = 0; c < 2; c++) {
        use_heads[c] = make_label(parent, &lv_font_montserrat_14, COLOR_DIM, COL_HEAD[c]);
        lv_obj_set_pos(use_heads[c], COL_X[c], 0);
    }

    for (int i = 0; i < USAGE_ROWS; i++)
        rows[i] = make_row(parent, i);

    // Spec: use_empty, where the first account's line goes. Until the PC's first usage message:
    // nothing made up, the card says it waits.
    use_empty = make_label(parent, &lv_font_montserrat_14, COLOR_MUTED, "");
    lv_obj_set_pos(use_empty, 0, 26);
    card_usage_set(NULL, 0);
    lv_label_set_text(use_empty, "Waiting for the PC");
    lv_timer_create(recheck, RECHECK_MS, NULL);
}
