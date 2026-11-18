
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
#include "app/job_watch.h"
#include "app/pc_open.h"
#include "ui/card_jobs.h"

static void open_row(const char *id) {
    pc_open_request(id); // (false: one under way, or locked: the tap does nothing)
}

void setup() {

    board_init();
    clock_init();
    display_init();

    // The slider and "Stay on" (in the pull-down panel) go through screen_power: it keeps the
    // brightness for the clock face's dimming and saves both across restarts.
    card_brightness_set_callback(screen_power_set_brightness);
    shade_set_stay_on_callback(screen_power_set_stay_on);
    screen_main_create();
    screen_power_init(); // the saved brightness and "Stay on" back
    // A tap on a chat (its row, or "Open on PC" on an alert): the PC brings it up (app/pc_open.h).
    card_jobs_on_open(open_row);
    job_watch_init();
    clock_start();

    Serial.println("Setup done");
}

void loop() {
    lv_timer_handler(); /* let the GUI do its work */
    serial_poll();
    delay(1);
}
