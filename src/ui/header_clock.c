#include "header_clock.h"
#include "theme.h"
#include "ui_common.h"

static lv_obj_t *clock_label;
static lv_obj_t *date_label;


void header_clock_set_time(const char *time, const char *date) {
    lv_label_set_text(clock_label, time);
    lv_label_set_text(date_label, date);
    lv_obj_align_to(date_label, clock_label, LV_ALIGN_OUT_RIGHT_BOTTOM, 16, -6);
}

void header_clock_create(lv_obj_t *parent) {
    clock_label = make_label(parent, &lv_font_montserrat_48, COLOR_TEXT, "--:--");
    lv_obj_set_pos(clock_label, 24, 16);

    // Sits next to the clock, so it must be re-aligned whenever the clock text changes.
    date_label = make_label(parent, &lv_font_montserrat_24, COLOR_MUTED, "---- --");
    lv_obj_align_to(date_label, clock_label, LV_ALIGN_OUT_RIGHT_BOTTOM, 16, -6);
}