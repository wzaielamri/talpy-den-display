#include "motion.h"
#include "card_jobs.h"
#include "weather_icon.h"

void motion_hold(unsigned who, bool on) {
    static unsigned holders;
    bool held = holders != 0;
    holders = on ? holders | who : holders & ~who;
    if ((holders != 0) == held) return; // (another cover still holds it, or none did)
    weather_icon_freeze(holders != 0);
    card_jobs_freeze(holders != 0);
}
