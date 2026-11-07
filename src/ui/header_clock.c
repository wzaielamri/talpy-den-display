#include "header_clock.h"
#include "theme.h"

static lv_obj_t *clock_label;
static lv_obj_t *date_label;


void header_clock_set_time(const char *time, const char *date) {
    lv_label_set_text(clock_label, time);
    lv_label_set_text(date_label, date);
    lv_obj_align_to(date_label, clock_label, LV_ALIGN_OUT_RIGHT_BOTTOM, 16, -6);
}

void header_clock_create(lv_obj_t *parent) {
    // create the clock label
    //  in screen_main_create:
    clock_label = lv_label_create(parent);
    lv_obj_set_style_text_font(clock_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(clock_label, lv_color_hex(COLOR_TEXT), 0);
    lv_obj_set_pos(clock_label, 24, 16);
    lv_label_set_text(clock_label, "--:--");
    // create the date label
    date_label = lv_label_create(parent);
    lv_obj_set_style_text_font(date_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(date_label, lv_color_hex(COLOR_MUTED), 0);
    lv_obj_align_to(date_label, clock_label, LV_ALIGN_OUT_RIGHT_BOTTOM, 16, -6);
    lv_label_set_text(date_label, "---- --");
}