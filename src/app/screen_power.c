#include "screen_power.h"
#include "app/settings.h"
#include "drivers/backlight.h"
#include "lvgl.h"
#include "ui/alert.h"
#include "ui/card_brightness.h"
#include "ui/card_hourly.h"
#include "ui/clock_face.h"
#include "ui/double_tap.h"
#include "ui/motion.h"
#include "ui/shade.h"

#define CLOCK_AFTER_MS 120000 // no touch for this long (no job working or waiting, no "Stay on"): the clock
#define CLOCK_DIM      50     // the clock face's brightness: this % of the slider's
#define SAVE_AFTER_MS  2000   // the slider still for this long: its value is saved (not every step of a drag)

static int32_t brightness = 100; // the slider's value
static bool stay_on;
static bool busy;                // a job works or waits for you: the cards stay
static bool pc_away;
static bool clock_shown;
static lv_timer_t *save_timer;

static void light(void) {
    backlight_set_percent(clock_shown ? brightness * CLOCK_DIM / 100 : brightness);
}

static void show_clock(bool on) {
    if (on == clock_shown) return;
    clock_shown = on;
    // Back to the overview for when the cards come back: the pull-down panel up (and not to be pulled
    // over the clock, whose every touch brings the cards back), the hourly view closed.
    shade_allow(!on);
    if (on) {
        card_hourly_hide();
        clock_face_show();
    } else {
        clock_face_hide();
    }
    motion_hold(MOTION_CLOCK, on); // (under the clock nothing on the cards needs to move)
    light();
}

// A double tap on the cards (ui/double_tap.c): the clock face now, as if 2 min had gone by. It stays
// till a touch or something to see (a job starting, an alert), also with a job busy or "Stay on".
static void clock_now(void) {
    show_clock(true);
}

static void save_brightness(lv_timer_t *timer) {
    LV_UNUSED(timer);
    save_timer = NULL; // (a one-shot timer: LVGL deletes it after this)
    settings_save_brightness(brightness);
}

void screen_power_set_brightness(int32_t percent) {
    brightness = percent;
    light();
    if (save_timer) {
        lv_timer_reset(save_timer); // still moving: wait till it's still
    } else {
        save_timer = lv_timer_create(save_brightness, SAVE_AFTER_MS, NULL);
        lv_timer_set_repeat_count(save_timer, 1);
    }
}

void screen_power_set_stay_on(bool on) {
    stay_on = on;
    settings_save_stay_on(on);
}

void screen_power_wake(void) {
    lv_display_trigger_activity(NULL); // counts as a touch: the cards for CLOCK_AFTER_MS at least
    show_clock(false);
}

void screen_power_set_busy(bool on) {
    if (on == busy) return;
    busy = on;
    if (on) {
        if (!pc_away) screen_power_wake(); // a job started (or waits): the cards, to show it
    } else {
        lv_display_trigger_activity(NULL); // the last one finished: the cards 2 min more, to see it done
    }
}

void screen_power_pc_present(bool present) {
    if (present == !pc_away) return;
    pc_away = !present;
    if (pc_away) {
        alert_hide(); // (its job's PC is gone: an alert left up would sit dimmed on the screen all night)
        show_clock(true);
    } else {
        screen_power_wake();
    }
}

static void tick(lv_timer_t *timer) {
    LV_UNUSED(timer);
    // (the PC away: its jobs can't be known, and "Stay on" doesn't keep the cards for a laptop that's gone)
    if (!clock_shown && (pc_away || (!stay_on && !busy)) && lv_display_get_inactive_time(NULL) > CLOCK_AFTER_MS)
        show_clock(true);
}

void screen_power_init(void) {
    brightness = settings_brightness();
    stay_on = settings_stay_on();
    card_brightness_set_value(brightness);
    shade_set_stay_on(stay_on);
    light();
    clock_face_on_touch(screen_power_wake);
    double_tap_set_callback(clock_now);
    lv_timer_create(tick, 500, NULL);
}
