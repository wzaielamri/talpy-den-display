#include "screen_power.h"
#include "drivers/backlight.h"
#include "drivers/display.h"
#include "lvgl.h"
#include "ui/card_hourly.h"
#include "ui/shade.h"

#define SLEEP_AFTER_MS 120000 // no touch for this long (and no job working or waiting): dark, unless "Stay on"
#define LISTEN_MS      15000 // serial data woke the napping board: stay up this long, for a whole message
                             // (the PC sends one at least every 10 s)

static int32_t brightness = 100; // the slider's value: what a wake goes back to (setup() starts at 100)
static bool stay_on;
static bool busy;                // a job works or waits for you: something to watch, so it stays lit
static bool pc_away;
static bool asleep;
static bool napped;              // the board napped since it went dark: the LCD needs starting again
static uint32_t listen_until;    // lv_tick time till which the board stays up, listening
static lv_obj_t *catcher;        // the whole screen, on the system layer (over the alert too)

void screen_power_set_brightness(int32_t percent) {
    brightness = percent;
    if (!asleep) backlight_set_percent(percent);
}

void screen_power_set_stay_on(bool on) {
    stay_on = on;
}

void screen_power_set_busy(bool on) {
    if (on == busy) return;
    busy = on;
    if (on) {
        if (!pc_away) screen_power_wake(); // a job started (or waits): light up to show it
    } else {
        lv_display_trigger_activity(NULL); // the last one finished: lit SLEEP_AFTER_MS more, to see it done
    }
}

void screen_power_sleep(void) {
    if (asleep) return;
    asleep = true;
    // Back to the overview for when it wakes: the pull-down panel up, the hourly view closed.
    shade_close();
    card_hourly_hide();
    // The dimmest the backlight goes (backlight.cpp: 0% is DIMMEST); then nothing is drawn while dark,
    // so animations (the weather icon, the waiting tile) don't keep the CPU busy for no one.
    backlight_set_percent(0);
    lv_display_enable_invalidation(NULL, false);
    lv_obj_add_flag(catcher, LV_OBJ_FLAG_CLICKABLE); // the next touch only wakes
}

// Lit again: the LCD started again if the board napped, everything drawn again (what changed while
// dark too), then the light on at the slider's brightness.
static void light_up(void) {
    lv_display_trigger_activity(NULL); // counts as a touch: lit for SLEEP_AFTER_MS at least
    if (!asleep) return;
    asleep = false;
    if (napped) {
        display_restart();
        napped = false;
    }
    lv_display_enable_invalidation(NULL, true);
    lv_obj_invalidate(lv_screen_active());
    backlight_set_percent(brightness);
}

void screen_power_wake(void) {
    light_up();
    lv_obj_remove_flag(catcher, LV_OBJ_FLAG_CLICKABLE); // (no finger on it: an alert, the PC)
}

void screen_power_wake_by_touch(void) {
    light_up(); // the catcher stays clickable till that finger lifts (catcher_released)
}

void screen_power_pc_present(bool present) {
    if (present == !pc_away) return;
    pc_away = !present;
    if (pc_away)
        screen_power_sleep();
    else
        screen_power_wake();
}

bool screen_power_napping(void) {
    if (!asleep || !pc_away || (int32_t)(lv_tick_get() - listen_until) < 0) return false;
    napped = true;
    return true;
}

void screen_power_heard_serial(void) {
    listen_until = lv_tick_get() + LISTEN_MS;
}

// The touch on the dark screen: it lights it, and stays the catcher's till the finger lifts, so it
// doesn't open the hourly view or press anything under it.
static void catcher_event(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED) {
        screen_power_wake_by_touch();
    } else if ((code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) && !asleep) {
        lv_obj_remove_flag(catcher, LV_OBJ_FLAG_CLICKABLE);
    }
}

static void tick(lv_timer_t *timer) {
    LV_UNUSED(timer);
    // (the PC away: its jobs can't be known, and "Stay on" doesn't keep a desk lit for a laptop that's gone)
    if (!asleep && (pc_away || (!stay_on && !busy)) && lv_display_get_inactive_time(NULL) > SLEEP_AFTER_MS)
        screen_power_sleep();
    // A tap that lit it from a nap but lifted before LVGL saw it: let the next touch through.
    if (!asleep && lv_obj_has_flag(catcher, LV_OBJ_FLAG_CLICKABLE) &&
        lv_indev_get_state(lv_indev_get_next(NULL)) == LV_INDEV_STATE_RELEASED)
        lv_obj_remove_flag(catcher, LV_OBJ_FLAG_CLICKABLE);
}

void screen_power_init(void) {
    // Invisible (no style: draws nothing) and never hidden or shown, so it never makes LVGL draw
    // anything again; clickable only while dark, and till the finger that lit it lifts.
    catcher = lv_obj_create(lv_layer_sys());
    lv_obj_remove_style_all(catcher);
    lv_obj_set_size(catcher, lv_pct(100), lv_pct(100));
    lv_obj_remove_flag(catcher, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(catcher, catcher_event, LV_EVENT_ALL, NULL);

    lv_timer_create(tick, 500, NULL);
}
