#include "shade.h"
#include "card_brightness.h"
#include "motion.h"
#include "theme.h"
#include "ui_common.h"

// Spec: the panel is 800 x 160 with radius 20; open, its top 24 px are above the screen,
// so only its bottom corners show round. Shut, all of it is above the screen.
#define PANEL_H       160
#define PANEL_OPEN    (-24)
#define PANEL_SHUT    (-PANEL_H)
#define PANEL_TRAVEL  (PANEL_OPEN - PANEL_SHUT)
#define PULL_ZONE_H   72    // a pull down that starts this high on the screen (the header) opens it
#define DRAG_START    6     // px the finger moves before the panel follows: a tap opens nothing
#define GRAB          16    // the panel's bottom edge stays this far under the finger
#define GLIDE_MS      150   // the whole way; a shorter way is quicker
#define AUTO_CLOSE_MS 10000 // no touch for this long: it goes up again

// Why it's built this way: this board draws everything in software into PSRAM, so it's quick only
// while few pixels change. The panel follows the finger (it moves at once, no waiting for a swipe to
// be recognised) and only its strip of the screen is drawn again each frame. The dimming under it
// changes the whole screen, the one slow redraw, so it comes on once, as the pull starts (the finger
// is moving, the dim reads as part of the pull), and goes once the panel is back up: never faded, and
// never in the way of the panel settling.

static lv_obj_t *zone;  // over the header: takes the pull down
static lv_obj_t *scrim; // dims the screen while the panel is down; a touch on it closes the panel
static lv_obj_t *panel;
static bool is_open;    // where it is, or is gliding to
static bool dragging;   // the finger moves it now
static int32_t press_y; // where the finger went down

static void anim_y(void *obj, int32_t v) { lv_obj_set_y((lv_obj_t *)obj, v); }

static int32_t finger_y(void) {
    lv_point_t p;
    lv_indev_get_point(lv_indev_active(), &p);
    return p.y;
}

// The panel on its way down: the screen dims (once) and the panel shows. What moves on the screen
// (the weather icon, the waiting tile's border) holds still meanwhile: drawn again and again under the
// dimming, it showed through undimmed.
static void come_down(void) {
    motion_hold(MOTION_SHADE, true);
    lv_obj_remove_flag(scrim, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_HIDDEN);
}

// Once it's back up: the dimming goes and the panel hides, so neither takes taps or costs drawing;
// the screen moves again.
static void settled(lv_anim_t *a) {
    LV_UNUSED(a);
    if (!is_open) {
        lv_obj_add_flag(scrim, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
        motion_hold(MOTION_SHADE, false); // (still held while the clock face shows)
    }
}

// From wherever the panel is (where the finger let go, or halfway the other way), the rest of the way.
static void glide(bool open) {
    is_open = open;
    int32_t from = lv_obj_get_y(panel), to = open ? PANEL_OPEN : PANEL_SHUT;
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, panel);
    lv_anim_set_exec_cb(&a, anim_y);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_duration(&a, 40 + (GLIDE_MS - 40) * LV_ABS(to - from) / PANEL_TRAVEL);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_completed_cb(&a, settled); // (starting it replaces one that's running)
    lv_anim_start(&a);
}

void shade_open(void) {
    if (!panel || is_open) return;
    come_down();
    glide(true);
}

void shade_close(void) {
    if (panel && is_open) glide(false);
}

// A pull down from the header: the screen dims as the pull starts, and the panel's bottom edge follows
// the finger; let go past a third of the way and it opens, else it goes back up.
static void zone_event(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED) {
        press_y = finger_y();
        dragging = false;
    } else if (code == LV_EVENT_PRESSING) {
        int32_t y = finger_y();
        if (!dragging) {
            if (is_open || y - press_y < DRAG_START) return; // a tap, or not pulling down yet
            dragging = true;
            lv_anim_delete(panel, anim_y);
            come_down();
        }
        lv_obj_set_y(panel, LV_CLAMP(PANEL_SHUT, y + GRAB - PANEL_H, PANEL_OPEN));
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        if (!dragging) return;
        dragging = false;
        glide(lv_obj_get_y(panel) > PANEL_SHUT + PANEL_TRAVEL / 3);
    }
}

// A push up on the open panel (not on its slider, which takes its own drags): it follows; let go
// past a third of the way and it closes, else it comes back down.
static void panel_event(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED) {
        press_y = finger_y();
        dragging = false;
    } else if (code == LV_EVENT_PRESSING) {
        int32_t dy = finger_y() - press_y;
        if (!dragging) {
            if (!is_open || dy > -DRAG_START) return;
            dragging = true;
            lv_anim_delete(panel, anim_y);
        }
        lv_obj_set_y(panel, LV_CLAMP(PANEL_SHUT, PANEL_OPEN + dy, PANEL_OPEN));
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        if (!dragging) return;
        dragging = false;
        glide(lv_obj_get_y(panel) > PANEL_OPEN - PANEL_TRAVEL / 3);
    }
}

// A touch anywhere outside the open panel closes it as the finger goes down, not when it lifts.
// (That touch stays the scrim's till the finger lifts, so it doesn't land on the card under it.)
static void scrim_pressed(lv_event_t *e) {
    LV_UNUSED(e);
    shade_close();
}

static stay_on_cb_t on_stay = NULL;
static lv_obj_t *stay;

void shade_set_stay_on_callback(stay_on_cb_t cb) { on_stay = cb; }

void shade_set_stay_on(bool on) {
    if (!stay) return;
    if (on)
        lv_obj_add_state(stay, LV_STATE_CHECKED);
    else
        lv_obj_remove_state(stay, LV_STATE_CHECKED);
}

static void stay_changed(lv_event_t *e) {
    bool on = lv_obj_has_state(lv_event_get_target_obj(e), LV_STATE_CHECKED);
    if (on_stay) on_stay(on); // safe if nobody registered
}

static void auto_close(lv_timer_t *timer) {
    LV_UNUSED(timer);
    if (is_open && !dragging && lv_display_get_inactive_time(NULL) > AUTO_CLOSE_MS) shade_close();
}

void shade_create(void) {
    lv_obj_t *top = lv_layer_top();

    // Spec: pull_zone, 800 x 72 over the header, invisible but for a small pill at its top middle
    // that says something pulls down from there. Nothing under it takes taps (clock, date, PC pill).
    zone = lv_obj_create(top);
    lv_obj_remove_style_all(zone);
    lv_obj_set_size(zone, lv_pct(100), PULL_ZONE_H);
    lv_obj_add_flag(zone, LV_OBJ_FLAG_CLICKABLE);
    // (no scrolling, here or passed on: the drag is the panel's)
    lv_obj_remove_flag(zone, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_CHAIN | LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(zone, zone_event, LV_EVENT_ALL, NULL);
    lv_obj_t *hint = make_blob(zone, 0, 0, 40, 4, LV_RADIUS_CIRCLE, COLOR_LINE);
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 6);

    // Spec: shade_scrim, the whole screen, black at 50%, from the start of the pull till the panel is
    // back up. Clickable: a touch on it closes the panel.
    scrim = lv_obj_create(top);
    lv_obj_remove_style_all(scrim);
    lv_obj_set_size(scrim, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(scrim, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scrim, LV_OPA_50, 0);
    lv_obj_add_flag(scrim, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(scrim, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(scrim, scrim_pressed, LV_EVENT_PRESSED, NULL);

    // Spec: shade, tile colour (lighter than the cards it covers) with a 1 px line-colour outline,
    // padding 40 at the sides, 48 on top (24 of them above the screen), 28 below.
    panel = make_card(top, 0, PANEL_SHUT, 800, PANEL_H);
    lv_obj_set_style_bg_color(panel, lv_color_hex(COLOR_TILE), 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(COLOR_LINE), 0);
    lv_obj_set_style_radius(panel, 20, 0);
    lv_obj_set_style_pad_hor(panel, 40, 0);
    lv_obj_set_style_pad_top(panel, 48, 0);
    lv_obj_set_style_pad_bottom(panel, 28, 0);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLL_CHAIN | LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(panel, panel_event, LV_EVENT_ALL, NULL);

    // Spec: the brightness (the card's title, value and slider) in the panel's left 564 px, so the slider
    // ends 16 px before "Stay on". The box reaches 14 px further on each side (padding): the knob at 0%
    // or 100% sticks out of the slider by half its 28 px, and LVGL draws a box's children only where the
    // box itself is, so without that room half the knob wasn't drawn.
    lv_obj_t *bright = lv_obj_create(panel);
    lv_obj_remove_style_all(bright);
    lv_obj_set_size(bright, 564 + 2 * 14, lv_pct(100));
    lv_obj_set_pos(bright, -14, 0);
    lv_obj_set_style_pad_hor(bright, 14, 0);
    lv_obj_remove_flag(bright, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    card_brightness_create(bright);

    // Spec: shade_stay, 140 x 44 at the right middle, 16 px into the panel's right padding (the knob at
    // 100% reaches 14 px past the slider's end: this leaves a gap; 24 px from the screen's edge), a
    // toggle: on, the screen doesn't go dark after a while without a touch. Off: screen colour, muted
    // text; on: text colour, screen-colour text.
    stay = lv_button_create(panel);
    lv_obj_set_size(stay, 140, 44);
    lv_obj_align(stay, LV_ALIGN_RIGHT_MID, 16, 0);
    lv_obj_add_flag(stay, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_set_style_radius(stay, 12, 0);
    lv_obj_set_style_shadow_width(stay, 0, 0);
    lv_obj_set_style_bg_color(stay, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_text_color(stay, lv_color_hex(COLOR_MUTED), 0);
    lv_obj_set_style_bg_color(stay, lv_color_hex(COLOR_TEXT), LV_STATE_CHECKED);
    lv_obj_set_style_text_color(stay, lv_color_hex(COLOR_BG), LV_STATE_CHECKED);
    lv_obj_add_event_cb(stay, stay_changed, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_t *stay_text = lv_label_create(stay); // (its colour from the button's state)
    lv_obj_set_style_text_font(stay_text, &lv_font_montserrat_14, 0);
    lv_label_set_text(stay_text, LV_SYMBOL_EYE_OPEN " Stay on");
    lv_obj_center(stay_text);

    lv_obj_t *handle = make_blob(panel, 0, 0, 40, 4, LV_RADIUS_CIRCLE, COLOR_DIM);
    lv_obj_align(handle, LV_ALIGN_BOTTOM_MID, 0, 16);

    lv_timer_create(auto_close, 1000, NULL);
}
