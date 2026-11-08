#include "card_hourly.h"
#include "theme.h"
#include "ui_common.h"
#include "weather_icon.h"

// Spec: the card is 768 x 376 at (16, 88), inner area 728 x 336. The columns are 91 x 272 from
// (0, 64), each next one 91 to the right; the temperature line runs over them, its points on
// their centres.
#define COL_W        91
#define COL_H        272
#define COLS_Y       64
#define RAIN_LIKELY  50       // from here the chance of rain shows in the rain colour
#define AUTO_HIDE_MS 60000    // no touch for this long: back to the jobs
#define CLOCK_SET    1700000000 // before this the board's clock was never set (as clock.c)

// One hour's column.
typedef struct {
    lv_obj_t *col;
    lv_obj_t *hour;
    lv_obj_t *icon;
    lv_obj_t *temp;
    lv_obj_t *rain;
    lv_obj_t *rain_bar;
} hour_col_t;

// Spec: the temperature line's box, 637 x 56 at (45, 208): its points on the column centres, 91 apart.
// Drawn 4 px inside a bigger lv_line, so the line's width and the dots aren't cut at its edges.
// Straight from hour to hour: a curve looked worse, as LVGL here keeps line points in whole pixels
// (LV_USE_FLOAT 0), so its many short pieces came out stepped and wobbly.
#define LINE_X    45
#define LINE_Y    208
#define LINE_H    56
#define LINE_PAD  4

static lv_obj_t *card_hourly;
static lv_obj_t *hr_summary;
static lv_obj_t *hr_note;
static hour_col_t cols[HOURLY_COLS];
static lv_obj_t *hr_line;
static lv_obj_t *hr_dots[HOURLY_COLS];
static lv_point_precise_t points[HOURLY_COLS]; // must stay alive: lv_line keeps the pointer

void card_hourly_show(void) {
    if (card_hourly) lv_obj_remove_flag(card_hourly, LV_OBJ_FLAG_HIDDEN);
}

void card_hourly_hide(void) {
    if (card_hourly) lv_obj_add_flag(card_hourly, LV_OBJ_FLAG_HIDDEN);
}

static void back_clicked(lv_event_t *e) {
    LV_UNUSED(e);
    card_hourly_hide();
}

static void auto_hide(lv_timer_t *timer) {
    LV_UNUSED(timer);
    if (!lv_obj_has_flag(card_hourly, LV_OBJ_FLAG_HIDDEN) && lv_display_get_inactive_time(NULL) > AUTO_HIDE_MS)
        card_hourly_hide();
}

// Spec: hr_col and what's in it, all centred on the column (TOP_MID): the hour at y 18, the icon at 44,
// the temperature at 100, the chance of rain at 216 and its bar at 242. Only the first (now) has a
// background.
static hour_col_t make_col(lv_obj_t *parent, int i) {
    hour_col_t c;
    c.col = lv_obj_create(parent);
    lv_obj_remove_style_all(c.col);
    lv_obj_set_size(c.col, COL_W, COL_H);
    lv_obj_set_pos(c.col, COL_W * i, COLS_Y);
    lv_obj_remove_flag(c.col, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    if (i == 0) {
        lv_obj_set_style_bg_color(c.col, lv_color_hex(COLOR_TILE), 0);
        lv_obj_set_style_bg_opa(c.col, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(c.col, 12, 0);
    }

    c.hour = make_label(c.col, &lv_font_montserrat_14, i == 0 ? COLOR_TEXT : COLOR_MUTED, "");
    lv_obj_align(c.hour, LV_ALIGN_TOP_MID, 0, 18);

    c.icon = weather_icon_small_create(c.col);
    lv_obj_align(c.icon, LV_ALIGN_TOP_MID, 0, 44);

    c.temp = make_label(c.col, &lv_font_montserrat_24, COLOR_TEXT, "");
    lv_obj_align(c.temp, LV_ALIGN_TOP_MID, 0, 100);

    c.rain = make_label(c.col, &lv_font_montserrat_14, COLOR_MUTED, "");
    lv_obj_align(c.rain, LV_ALIGN_TOP_MID, 0, 216);

    c.rain_bar = lv_bar_create(c.col);
    lv_obj_remove_style_all(c.rain_bar); // no padding: the fill takes the whole height
    lv_obj_set_size(c.rain_bar, 56, 6);
    lv_obj_align(c.rain_bar, LV_ALIGN_TOP_MID, 0, 242);
    lv_bar_set_range(c.rain_bar, 0, 100);
    lv_obj_set_style_bg_color(c.rain_bar, lv_color_hex(COLOR_LINE), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(c.rain_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(c.rain_bar, 3, LV_PART_MAIN);
    lv_obj_set_style_bg_color(c.rain_bar, lv_color_hex(COLOR_RAIN), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(c.rain_bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(c.rain_bar, 3, LV_PART_INDICATOR);
    return c;
}

// Spec: hr_line and hr_dot, the temperature over the columns. An lv_line from hour to hour
// (rounded, 3 px, text colour) and a dot (8 px) on each hour.
static void make_line(lv_obj_t *parent) {
    hr_line = lv_line_create(parent);
    lv_obj_set_pos(hr_line, LINE_X - LINE_PAD, LINE_Y - LINE_PAD);
    lv_obj_set_style_line_width(hr_line, 3, 0);
    lv_obj_set_style_line_color(hr_line, lv_color_hex(COLOR_TEXT), 0);
    lv_obj_set_style_line_rounded(hr_line, true, 0);
    for (int i = 0; i < HOURLY_COLS; i++)
        hr_dots[i] = make_blob(parent, 0, 0, 8, 8, LV_RADIUS_CIRCLE, COLOR_TEXT);
}

// The line and dots for the first n hours' temperatures, from coldest - 2 (bottom) to warmest + 2 (top).
static void show_temps(const hour_t *hours, int n) {
    if (n < 1) {
        lv_obj_add_flag(hr_line, LV_OBJ_FLAG_HIDDEN);
        for (int i = 0; i < HOURLY_COLS; i++) lv_obj_add_flag(hr_dots[i], LV_OBJ_FLAG_HIDDEN);
        return;
    }
    int lo = hours[0].temp, hi = hours[0].temp;
    for (int i = 1; i < n; i++) {
        if (hours[i].temp < lo) lo = hours[i].temp;
        if (hours[i].temp > hi) hi = hours[i].temp;
    }
    int top = hi + 2, range = (hi + 2) - (lo - 2);
    for (int i = 0; i < HOURLY_COLS; i++) {
        if (i >= n) {
            lv_obj_add_flag(hr_dots[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        int y = ((top - hours[i].temp) * LINE_H + range / 2) / range; // (rounded)
        points[i].x = LINE_PAD + i * COL_W;
        points[i].y = LINE_PAD + y;
        lv_obj_set_pos(hr_dots[i], LINE_X + i * COL_W - 4, LINE_Y + y - 4);
        lv_obj_remove_flag(hr_dots[i], LV_OBJ_FLAG_HIDDEN);
    }
    if (n < 2) { // one hour: a dot, no line
        lv_obj_add_flag(hr_line, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_line_set_points(hr_line, points, n);
    lv_obj_remove_flag(hr_line, LV_OBJ_FLAG_HIDDEN);
}

void card_hourly_set(const hourly_update_t *u) {
    if (!u || !card_hourly) return; // no data, or card not created yet
    lv_label_set_text(hr_summary, u->summary ? u->summary : "");
    lv_label_set_text_fmt(hr_note, "Open-Meteo " LV_SYMBOL_BULLET " %s", u->updated ? u->updated : "--:--");

    int n = u->hours ? (u->count < HOURLY_COLS ? u->count : HOURLY_COLS) : 0;

    for (int i = 0; i < HOURLY_COLS; i++) {
        hour_col_t *c = &cols[i];
        if (i >= n) { // fewer hours than columns: the rest empty
            lv_obj_add_flag(c->col, LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        const hour_t *h = &u->hours[i];
        lv_obj_remove_flag(c->col, LV_OBJ_FLAG_HIDDEN);

        if (i == 0) {
            lv_label_set_text(c->hour, "Now");
        } else if (h->at >= CLOCK_SET) {
            struct tm tm;
            localtime_r(&h->at, &tm);
            lv_label_set_text_fmt(c->hour, "%02d:00", tm.tm_hour);
        } else {
            lv_label_set_text(c->hour, "--");
        }
        // (the moon's cut-out disc takes the column's background: tile in the first, card in the rest)
        weather_icon_small_set(c->icon, h->code, h->is_day, i == 0 ? COLOR_TILE : COLOR_CARD);
        lv_label_set_text_fmt(c->temp, "%d°", h->temp);
        if (h->rain < 0) {
            lv_label_set_text(c->rain, "--");
            lv_obj_set_style_text_color(c->rain, lv_color_hex(COLOR_MUTED), 0);
            lv_bar_set_value(c->rain_bar, 0, LV_ANIM_OFF);
        } else {
            int rain = h->rain > 100 ? 100 : h->rain;
            lv_label_set_text_fmt(c->rain, "%d%%", rain);
            lv_obj_set_style_text_color(c->rain, lv_color_hex(rain >= RAIN_LIKELY ? COLOR_RAIN : COLOR_MUTED), 0);
            lv_bar_set_value(c->rain_bar, rain, LV_ANIM_OFF);
        }
    }
    show_temps(u->hours, n);
}

void card_hourly_create(lv_obj_t *screen) {
    // Spec: card style; left clickable (the default) so taps don't reach the cards under it.
    card_hourly = make_card(screen, 16, 88, 768, 376);

    // Spec: hr_back, 120 x 44 at (0, 0), tile background, radius 12; "< Back" in font 24.
    lv_obj_t *back = lv_button_create(card_hourly);
    lv_obj_set_size(back, 120, 44);
    lv_obj_set_pos(back, 0, 0);
    lv_obj_set_style_bg_color(back, lv_color_hex(COLOR_TILE), 0);
    lv_obj_set_style_bg_color(back, lv_color_hex(COLOR_LINE), LV_STATE_PRESSED);
    lv_obj_set_style_radius(back, 12, 0);
    lv_obj_set_style_shadow_width(back, 0, 0);
    lv_obj_set_style_border_width(back, 0, 0);
    lv_obj_add_event_cb(back, back_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_text = make_label(back, &lv_font_montserrat_24, COLOR_TEXT, LV_SYMBOL_LEFT " Back");
    lv_obj_center(back_text);

    lv_obj_t *title = make_label(card_hourly, &lv_font_montserrat_14, COLOR_MUTED, "NEXT 8 HOURS");
    lv_obj_set_style_text_letter_space(title, 1, 0);
    lv_obj_set_pos(title, 140, 0);

    hr_summary = make_label(card_hourly, &lv_font_montserrat_24, COLOR_TEXT, "");
    lv_obj_set_size(hr_summary, 588, lv_font_get_line_height(&lv_font_montserrat_24));
    lv_label_set_long_mode(hr_summary, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(hr_summary, 140, 18);

    hr_note = make_label(card_hourly, &lv_font_montserrat_14, COLOR_DIM, "");
    lv_obj_align(hr_note, LV_ALIGN_TOP_RIGHT, 0, 0);

    for (int i = 0; i < HOURLY_COLS; i++)
        cols[i] = make_col(card_hourly, i);
    make_line(card_hourly); // after the columns: it draws over them

    lv_obj_add_flag(card_hourly, LV_OBJ_FLAG_HIDDEN);
    lv_timer_create(auto_hide, 1000, NULL);

    // Until the PC's first hourly message: no columns, no line, the summary says it waits.
    hourly_update_t u = {NULL, 0, "Waiting for the PC", NULL};
    card_hourly_set(&u);
}
