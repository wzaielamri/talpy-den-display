
#include <Arduino.h>
#include <lvgl.h>

#include "app/clock.h"
#include "app/serial_link.h"
#include "ui/screen_main.h"
#include "ui/card_brightness.h"
#include "drivers/backlight.h"
#include "drivers/display.h"
#include "drivers/board.h"

void setup() {

    board_init();
    clock_init();
    display_init();

    card_brightness_set_callback(backlight_set_percent); 
    screen_main_create();
    clock_start();

    Serial.println("Setup done");
}

void loop() {
    lv_timer_handler(); /* let the GUI do its work */
    serial_poll();
    delay(1);
}
