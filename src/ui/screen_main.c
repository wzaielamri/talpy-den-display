#include "screen_main.h"
#include "card_brightness.h"
#include "card_weather.h"
#include "header_clock.h"
#include "lvgl.h"
#include "theme.h"
#include "ui_common.h"



void screen_main_create(void) {
    styles_init();
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(COLOR_BG), 0);
    lv_obj_t *card_brightness = make_card(screen, 16, 360, 280, 104);
    lv_obj_t *card_weather = make_card(screen, 16, 88, 280, 256);
    lv_obj_t *card_jobs = make_card(screen, 312, 88, 472, 376);

    header_clock_create(screen);
    card_brightness_create(card_brightness);
    card_weather_create(card_weather);

    lv_screen_load(screen);
}