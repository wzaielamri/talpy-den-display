#include "alert.h"
#include "motion.h"
#include "talpy.h"
#include "theme.h"
#include "ui_common.h"

#define DONE_HIDE_MS 15000 // a "finished" alert goes by itself after this long

// Why a screen of its own: on lv_layer_top() over a see-through dim, LVGL kept drawing the main screen
// under it, its animations (the weather Talpy, the jobs banner) too, blending the dim over them again on
// every frame: they showed through and flickered. With its own screen loaded, nothing of the main screen
// is drawn while the alert shows (its animations are held meanwhile: MOTION_ALERT), and the main screen
// comes back as it was.
static lv_obj_t *alert_scr; // NULL until the first alert
static lv_obj_t *back_to;   // the screen it took the place of
static lv_obj_t *box, *al_talpy, *al_kicker, *al_name, *al_detail, *ok;
static bool showing;
static bool shade_out; // the pull-down shade is out: it can be over this screen too, so Talpy holds still
static alert_kind_t shown_kind;
static lv_timer_t *done_timer;

static void ok_clicked(lv_event_t *e) {
    LV_UNUSED(e);
    alert_hide();
}

static void done_timeout(lv_timer_t *timer) {
    LV_UNUSED(timer);
    done_timer = NULL; // (a one-shot timer: LVGL deletes it after this)
    alert_hide();
}

// Talpy moves only while the alert shows and nothing dims it.
static void update_motion(void) {
    if (al_talpy) talpy_freeze(al_talpy, shade_out || !showing);
}

void alert_motion_hold(bool out) {
    shade_out = out;
    update_motion();
}

// Made the first time an alert shows, then kept for the next one.
static void create(void) {
    // Spec: alert_scr, a screen of its own in the screen colour.
    alert_scr = lv_obj_create(NULL);
    lv_obj_remove_flag(alert_scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(alert_scr, lv_color_hex(COLOR_BG), 0);

    // Spec: alert_box, 560 x 260 in the middle, card colour, radius 20, padding 28,
    // a 6 px border in the kind's colour on its left side only.
    box = make_card(alert_scr, 0, 0, 560, 260);
    lv_obj_center(box);
    lv_obj_set_style_radius(box, 20, 0);
    lv_obj_set_style_pad_all(box, 28, 0);
    lv_obj_set_style_border_side(box, LV_BORDER_SIDE_LEFT, 0);
    lv_obj_set_style_border_width(box, 6, 0);

    // Spec: Talpy over the box's top right, its burrow on the box's top edge; drawn after the box, so over it.
    // Frozen (no timer running) while no alert shows.
    al_talpy = talpy_create(alert_scr);
    lv_obj_align_to(al_talpy, box, LV_ALIGN_OUT_TOP_RIGHT, -40, 12);
    talpy_freeze(al_talpy, true);

    al_kicker = make_label(box, &lv_font_montserrat_14, COLOR_WAIT, "");
    lv_obj_set_style_text_letter_space(al_kicker, 1, 0);
    lv_obj_set_pos(al_kicker, 0, 12);

    al_name = make_label(box, &lv_font_montserrat_40, COLOR_TEXT, "");
    lv_obj_set_size(al_name, 504, lv_font_get_line_height(&lv_font_montserrat_40));
    lv_label_set_long_mode(al_name, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(al_name, 0, 60);

    al_detail = make_label(box, &lv_font_montserrat_24, COLOR_MUTED, "");
    lv_obj_set_size(al_detail, 504, lv_font_get_line_height(&lv_font_montserrat_24));
    lv_label_set_long_mode(al_detail, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(al_detail, 0, 112);

    // Spec: al_ok, 140 x 52 at the bottom right, in the kind's colour, radius 12; "OK" in the screen colour.
    ok = lv_button_create(box);
    lv_obj_set_size(ok, 140, 52);
    lv_obj_align(ok, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_radius(ok, 12, 0);
    lv_obj_set_style_shadow_width(ok, 0, 0);
    lv_obj_add_event_cb(ok, ok_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *ok_text = make_label(ok, &lv_font_montserrat_24, COLOR_BG, "OK");
    lv_obj_center(ok_text);
}

// The kind's colours: waiting amber, done green.
static void paint(alert_kind_t kind) {
    bool done = kind == ALERT_DONE;
    lv_color_t c = lv_color_hex(done ? COLOR_DONE : COLOR_WAIT);
    lv_obj_set_style_border_color(box, c, 0);
    lv_obj_set_style_text_color(al_kicker, c, 0);
    lv_obj_set_style_bg_color(ok, c, 0);
}

bool alert_is_showing(void) {
    return showing;
}

bool alert_show(alert_kind_t kind, const char *kicker, const char *name, const char *detail) {
    if (showing && shown_kind == ALERT_WAITING && kind == ALERT_DONE) return false; // a job waiting for you first
    if (!alert_scr) create();
    paint(kind);
    lv_label_set_text(al_kicker, kicker && kicker[0] ? kicker : kind == ALERT_DONE ? "FINISHED" : "WAITING FOR YOU");
    lv_label_set_text(al_name, name ? name : "");
    lv_label_set_text(al_detail, detail ? detail : "");
    shown_kind = kind;

    // Done first, then the reaction: it is ignored while the mood is NEEDS, so a done alert after a waiting
    // one plays it.
    if (kind == ALERT_DONE) {
        talpy_set_mood(al_talpy, TALPY_DONE);
        talpy_react_finished(al_talpy);
    } else {
        talpy_set_mood(al_talpy, TALPY_NEEDS);
    }

    // A finished job's alert goes by itself; a waiting one stays.
    if (done_timer) {
        lv_timer_delete(done_timer);
        done_timer = NULL;
    }
    if (kind == ALERT_DONE) {
        done_timer = lv_timer_create(done_timeout, DONE_HIDE_MS, NULL);
        lv_timer_set_repeat_count(done_timer, 1);
    }

    if (!showing) {
        showing = true;
        update_motion();
        motion_hold(MOTION_ALERT, true);
        back_to = lv_screen_active();
        lv_screen_load(alert_scr);
    }
    return true;
}

void alert_hide(void) {
    if (!showing) return;
    showing = false;
    if (done_timer) {
        lv_timer_delete(done_timer);
        done_timer = NULL;
    }
    update_motion();
    motion_hold(MOTION_ALERT, false);
    if (back_to) lv_screen_load(back_to);
}
