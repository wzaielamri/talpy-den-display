#include "backlight.h"

void sendI2CCommand(uint8_t command);

void set_backlight(int32_t x) {
    if (x > DIMMEST) {
        x = DIMMEST;
    } else if (x < 0) {
        x = 0;
    }
    sendI2CCommand(x);
}

void Screen_dim_curve(lv_event_t *e) {
    lv_obj_t *slider = lv_event_get_target_obj(e); // which object sent the event
    int32_t value = lv_slider_get_value(slider);   // 0 … 100, where the knob is
    int32_t x = DIMMEST - value * value * DIMMEST / 10000;
    set_backlight(x);
}
