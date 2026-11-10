#include "double_tap.h"

#define DOUBLE_TAP_MS 500 // the second tap lifts at most this long after the first
#define TAP_MOVE      16  // px: a finger that moves this far between down and up swiped (a pull-down), didn't tap
#define TAP_NEAR      48  // px: the second tap lands at most this far from the first

static double_tap_cb_t on_double;
static lv_point_t press_at;   // where the finger went down
static lv_point_t first_at;   // where the first tap was
static uint32_t first_time;   // when it lifted
static bool have_first;       // a first tap waits for its second

void double_tap_set_callback(double_tap_cb_t cb) { on_double = cb; }

static bool within(lv_point_t a, lv_point_t b, int32_t d) {
    return LV_ABS(a.x - b.x) < d && LV_ABS(a.y - b.y) < d;
}

static void tap_event(lv_event_t *e) {
    lv_point_t p;
    lv_indev_get_point(lv_indev_active(), &p);
    if (lv_event_get_code(e) == LV_EVENT_PRESSED) {
        press_at = p;
        return;
    }
    // LV_EVENT_SHORT_CLICKED: the finger lifted before a long press.
    if (!within(p, press_at, TAP_MOVE)) { // a swipe: no tap, and none waits any more
        have_first = false;
        return;
    }
    if (have_first && lv_tick_elaps(first_time) <= DOUBLE_TAP_MS && within(p, first_at, TAP_NEAR)) {
        have_first = false;
        if (on_double) on_double(); // safe if nobody registered
        return;
    }
    have_first = true; // the first tap (or a new first one: the last was too long ago or too far)
    first_at = p;
    first_time = lv_tick_get();
}

void double_tap_watch(lv_obj_t *obj) {
    lv_obj_add_event_cb(obj, tap_event, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(obj, tap_event, LV_EVENT_SHORT_CLICKED, NULL);
}
