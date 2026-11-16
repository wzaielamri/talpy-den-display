#include "clock_face.h"
#include "app/clock.h"
#include "banner_text.h"
#include "theme.h"
#include "ui_common.h"
#include "weather_talpy.h"
#include <stdio.h>
#include <time.h>

LV_FONT_DECLARE(font_clock_128); // tools/make_clock_font.py: Montserrat Medium 128 px, digits and ":" only

#define SHIFTS 8

// Spec: where the face's content sits, one spot a minute around the middle (within 6 px), so no pixel
// shows the same thing for hours (some LCDs keep a faint ghost of a picture held that long).
static const int8_t SHIFT[SHIFTS][2] = {{0, 0}, {4, 2}, {6, -2}, {2, -4}, {-2, -2}, {-6, 2}, {-4, 4}, {0, 4}};

static lv_obj_t *face;     // the whole screen, in the screen's colour
static lv_obj_t *content;  // time, date, weather, chats and "PC offline", one under the other, centred
static lv_obj_t *time_label, *date_label, *weather_row, *temp_label, *chat_label, *pc_label;
static lv_obj_t *wx_talpy;  // the calm weather Talpy: runs only while the face shows
static bool pc_online;  // (false till the first message: "PC offline" shows, as before)
static clock_face_touch_cb_t on_touch;
static int shown_minute = -1;
static int shift;

void clock_face_on_touch(clock_face_touch_cb_t cb) { on_touch = cb; }

static void face_pressed(lv_event_t *e) {
    LV_UNUSED(e);
    if (on_touch) on_touch(); // (the touch stays the face's till it lifts: it doesn't land on a card)
}

// The time and the date, again when the minute changed (or now: force), and the content a spot further.
static void show_time(bool force) {
    time_t now = time(NULL);
    if (now < CLOCK_SET) {
        lv_label_set_text(time_label, "--:--");
        lv_label_set_text(date_label, "");
        return;
    }
    struct tm tm;
    localtime_r(&now, &tm);
    if (!force && tm.tm_min == shown_minute) return;
    shown_minute = tm.tm_min;

    char time_str[8], weekday[12], month[12];
    strftime(time_str, sizeof time_str, "%H:%M", &tm);
    strftime(weekday, sizeof weekday, "%A", &tm);
    strftime(month, sizeof month, "%B", &tm);
    lv_label_set_text(time_label, time_str);
    lv_label_set_text_fmt(date_label, "%s, %d %s", weekday, tm.tm_mday, month);

    shift = (shift + 1) % SHIFTS;
    lv_obj_align(content, LV_ALIGN_CENTER, SHIFT[shift][0], SHIFT[shift][1]);
}

static void tick(lv_timer_t *timer) {
    LV_UNUSED(timer);
    if (!lv_obj_has_flag(face, LV_OBJ_FLAG_HIDDEN)) show_time(false);
}

void clock_face_show(void) {
    if (!face) return;
    show_time(true);
    weather_talpy_freeze(wx_talpy, false);
    lv_obj_remove_flag(face, LV_OBJ_FLAG_HIDDEN);
}

void clock_face_hide(void) {
    if (!face) return;
    lv_obj_add_flag(face, LV_OBJ_FLAG_HIDDEN);
    weather_talpy_freeze(wx_talpy, true);
}

void clock_face_set_view(const wv_view_t *v) {
    if (!face || !v) return;
    weather_talpy_set(wx_talpy, v->talpy, v->sky);
    if (v->has_temp) {
        lv_label_set_text_fmt(temp_label, "%d°", v->temp);
        lv_obj_remove_flag(temp_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(temp_label, LV_OBJ_FLAG_HIDDEN);
    }
}

void clock_face_set_chats(bool have_data, int waiting, int working, int done) {
    if (!face) return;
    char text[48];
    banner_headline(text, sizeof text, have_data, waiting, working, done);
    lv_label_set_text(chat_label, text);
}

void clock_face_set_pc(bool online) {
    if (!face) return;
    pc_online = online;
    if (online) {
        lv_obj_add_flag(pc_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(chat_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(pc_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(chat_label, LV_OBJ_FLAG_HIDDEN);
    }
}

// A box that only lays out its children (no background, border or padding of its own).
static lv_obj_t *make_box(lv_obj_t *parent, lv_flex_flow_t flow, int gap) {
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_flex_flow(box, flow);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(box, gap, 0);
    lv_obj_set_style_pad_column(box, gap, 0);
    return box;
}

void clock_face_create(lv_obj_t *screen) {
    // Spec: clock_face, the whole screen in the screen's colour, over the cards (made after them).
    // Clickable: a touch brings the cards back and doesn't reach them.
    face = lv_obj_create(screen);
    lv_obj_remove_style_all(face);
    lv_obj_set_size(face, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(face, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(face, LV_OPA_COVER, 0);
    lv_obj_add_flag(face, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(face, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(face, face_pressed, LV_EVENT_PRESSED, NULL);

    // Spec: one column, centred, 16 px apart: the time (font_clock_128), the date (montserrat_24,
    // muted), the weather (the calm weather Talpy, 104 x 96, and the temperature in montserrat_40), the chat
    // line (montserrat_14, muted; hidden while the PC is away) and "PC offline" (montserrat_14, dim) while it is.
    // (the face moves a few px each minute, so the weather Talpy's own motion stays small and slow)
    content = make_box(face, LV_FLEX_FLOW_COLUMN, 16);
    lv_obj_align(content, LV_ALIGN_CENTER, 0, 0);

    time_label = make_label(content, &font_clock_128, COLOR_TEXT, "--:--");
    date_label = make_label(content, &lv_font_montserrat_24, COLOR_MUTED, "");

    weather_row = make_box(content, LV_FLEX_FLOW_ROW, 12);
    wx_talpy = weather_talpy_create(weather_row, true);
    weather_talpy_freeze(wx_talpy, true); // (clock_face_show lets him go on)
    temp_label = make_label(weather_row, &lv_font_montserrat_40, COLOR_TEXT, "");
    lv_obj_add_flag(temp_label, LV_OBJ_FLAG_HIDDEN); // till the weather is known

    chat_label = make_label(content, &lv_font_montserrat_14, COLOR_MUTED, "");
    clock_face_set_chats(false, 0, 0, 0);

    pc_label = make_label(content, &lv_font_montserrat_14, COLOR_DIM, "PC offline");
    clock_face_set_pc(pc_online);

    lv_timer_create(tick, 1000, NULL);
}
