#include "motion.h"
#include "alert.h"
#include "card_jobs.h"
#include "card_weather.h"

void motion_hold(unsigned who, bool on) {
    static unsigned holders;
    unsigned before = holders;
    holders = on ? holders | who : holders & ~who;
    if ((holders != 0) != (before != 0)) { // (not if another cover still holds it, or none did)
        card_weather_freeze(holders != 0);
        card_jobs_freeze(holders != 0);
    }
    if ((holders & MOTION_SHADE) != (before & MOTION_SHADE)) alert_motion_hold((holders & MOTION_SHADE) != 0);
}
