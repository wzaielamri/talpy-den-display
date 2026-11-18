#include "screen_light.h"

// The locked clock face's brightness, in the slider's %: low enough that the backlight is silent. Its controller
// (the STC8 at I2C 0x30) dims by PWM at a rate set in its own firmware, and the board hums from 99% down to 12%;
// only 100% and the lowest levels are quiet. 10% is backlight value 242 (244 is dimmest).
#define CLOCK_LOCKED_PERCENT 10

int32_t screen_light_percent(int32_t slider, bool clock_shown, bool locked) {
    if (clock_shown && locked && slider > CLOCK_LOCKED_PERCENT) return CLOCK_LOCKED_PERCENT;
    return slider;
}
