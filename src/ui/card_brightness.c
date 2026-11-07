#include "card_brightness.h"
#include "theme.h"
#include "ui_common.h"
#include <stdio.h>

static lv_obj_t *br_slider = NULL;
static lv_obj_t *br_title;
static lv_obj_t *br_value;

static void br_set_value_text(int32_t value) {
    char buf[8];
    snprintf(buf, sizeof buf, "%ld%%", (long)value);
    lv_label_set_text(br_value, buf); // stays right-aligned: lv_obj_align() is re-applied on resize
}

static brightness_cb_t on_change = NULL;

void card_brightness_set_callback(brightness_cb_t cb) { on_change = cb; }

static void br_slider_event(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        int32_t value = lv_slider_get_value(lv_event_get_target_obj(e));
        if (on_change) on_change(value);        // safe if nobody registered
        br_set_value_text(value);
    }
}


void card_brightness_create(lv_obj_t *parent){
    br_slider = lv_slider_create(parent);
    lv_obj_set_width(br_slider, 240);
    lv_obj_set_height(br_slider, 10);
    lv_obj_set_x(br_slider, 0);
    lv_obj_set_y(br_slider, 38);
    lv_obj_set_align(br_slider, LV_ALIGN_TOP_LEFT);
    lv_obj_add_flag(br_slider, LV_OBJ_FLAG_SCROLL_ON_FOCUS); /// Flags
    lv_obj_remove_flag(br_slider, LV_OBJ_FLAG_SCROLLABLE);   /// Flags
    lv_obj_set_style_bg_color(br_slider, lv_color_hex(COLOR_TILE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(br_slider, lv_color_hex(COLOR_TEXT), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(br_slider, lv_color_white(), LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(br_slider, lv_color_hex(COLOR_WAIT), LV_PART_KNOB | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(br_slider, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(br_slider, 9, LV_PART_KNOB);
    lv_slider_set_range(br_slider, 0, 100);
    lv_obj_set_ext_click_area(br_slider, 20);
    lv_slider_set_value(br_slider, 100, LV_ANIM_OFF); // setup() starts the backlight at brightest
    lv_obj_add_event_cb(br_slider, br_slider_event, LV_EVENT_ALL, NULL);

    br_title = make_card_title(parent, "BRIGHTNESS");

    br_value = make_label(parent, &lv_font_montserrat_14, COLOR_TEXT, "");
    lv_obj_align(br_value, LV_ALIGN_TOP_RIGHT, 0, 0);
    br_set_value_text(100);
}
