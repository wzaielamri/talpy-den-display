#include "screen_main.h"
#include "card_hourly.h"
#include "card_jobs.h"
#include "card_usage.h"
#include "card_weather.h"
#include "clock_face.h"
#include "header_clock.h"
#include "header_pc.h"
#include "lvgl.h"
#include "shade.h"
#include "theme.h"
#include "ui_common.h"

static void weather_clicked(lv_event_t *e) {
    LV_UNUSED(e);
    card_hourly_show();
}

void screen_main_create(void) {
    styles_init();
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(COLOR_BG), 0);
    lv_obj_t *card_wx   = make_card(screen, 16, 88, 280, 220);
    lv_obj_t *card_use  = make_card(screen, 16, 324, 280, 140);
    lv_obj_t *card_jobs = make_card(screen, 312, 88, 472, 376);

    header_clock_create(screen);
    header_pc_create(screen);
    card_weather_create(card_wx);
    card_usage_create(card_use);
    card_jobs_create(card_jobs);
    card_hourly_create(screen); // after the three cards: it covers them while it shows
    clock_face_create(screen);  // last: it covers everything (the header too) while it shows

    // A tap on the weather card opens the hourly forecast; it shows it's pressed (tile colour).
    lv_obj_add_event_cb(card_wx, weather_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(card_wx, lv_color_hex(COLOR_TILE), LV_STATE_PRESSED);

    lv_screen_load(screen);
    shade_create(); // the brightness, pulled down from the top: over whatever screen shows
}
