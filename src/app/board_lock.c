#include "board_lock.h"
#include "app/job_watch.h"
#include "app/screen_power.h"

static bool laptop_locked;
static bool pc_away;
static bool on; // what the alerts and the screen were told last

static void follow(void) {
    bool now = laptop_locked || pc_away;
    if (now == on) return;
    on = now;
    if (on) {
        job_watch_set_locked(true); // (first: a waiting alert showing is owed, then it goes)
        screen_power_set_locked(true);
    } else {
        screen_power_set_locked(false); // (first the cards, then the owed alert over them)
        job_watch_set_locked(false);
    }
}

void board_lock_laptop(bool locked) {
    laptop_locked = locked;
    follow();
}

void board_lock_pc(bool present) {
    pc_away = !present;
    follow();
}

bool board_lock_on(void) {
    return on;
}
