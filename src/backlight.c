#include "backlight.h"

void sendI2CCommand(uint8_t command);

static void set_backlight(int32_t x) {
    if (x > DIMMEST) {
        x = DIMMEST;
    } else if (x < 0) {
        x = 0;
    }
    sendI2CCommand(x);
}
void backlight_set_percent(int32_t percent) {
    if (percent < 0) {
        percent = 0;
    } else if (percent > 100) {
        percent = 100;
    }
    int32_t x = DIMMEST - percent * percent * DIMMEST / 10000;
    set_backlight(x);
}
