
#include <Arduino.h>
#include <lvgl.h>

#include "app/clock.h"
#include "app/screen_power.h"
#include "app/serial_link.h"
#include "ui/screen_main.h"
#include "ui/card_brightness.h"
#include "ui/shade.h"
#include "drivers/backlight.h"
#include "drivers/display.h"
#include "drivers/board.h"

void setup() {

    board_init();
    clock_init();
    display_init();

    // The slider and "Stay on" (in the pull-down panel) go through screen_power: it keeps the
    // brightness to come back to after the screen went dark.
    card_brightness_set_callback(screen_power_set_brightness);
    shade_set_stay_on_callback(screen_power_set_stay_on);
    screen_main_create();
    screen_power_init();
    clock_start();

    Serial.println("Setup done");
}

void loop() {
    // The PC is away and the screen dark: the board sleeps in short naps (100 ms of light sleep),
    // waking early on serial data. Between naps: a touch lights the screen, data is read.
    if (screen_power_napping()) {
        if (board_nap(100)) screen_power_heard_serial();
        if (display_touched()) screen_power_wake_by_touch();
        serial_poll();
        return;
    }
    lv_timer_handler(); /* let the GUI do its work */
    serial_poll();
    delay(1);
}
