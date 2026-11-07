#include "card_brightness.h"
#include "theme.h"
#include <stdio.h>

static lv_obj_t *ui_Slider = NULL;
static lv_obj_t *brightness_title;
static lv_obj_t *brightness_value;

static void ui_set_brightness_value_text(int32_t value) {
    char buf[8];
    snprintf(buf, sizeof buf, "%ld%%", (long)value);
    lv_label_set_text(brightness_value, buf);
    lv_obj_t *card_brightness = lv_obj_get_parent(brightness_value);
    lv_obj_align_to(brightness_value, card_brightness, LV_ALIGN_TOP_RIGHT, 0, 0);
}



static brightness_cb_t on_change = NULL;

void card_brightness_set_callback(brightness_cb_t cb) { on_change = cb; }

static void ui_event_Slider(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        int32_t value = lv_slider_get_value(lv_event_get_target_obj(e));
        if (on_change) on_change(value);        // safe if nobody registered
        ui_set_brightness_value_text(value);
    }
}


void card_brightness_create(lv_obj_t *parent){
    // create the UI elements inside the cards
    ui_Slider = lv_slider_create(parent);
    lv_obj_set_width(ui_Slider, 240);
    lv_obj_set_height(ui_Slider, 10);
    lv_obj_set_x(ui_Slider, 0);
    lv_obj_set_y(ui_Slider, 38);
    lv_obj_set_align(ui_Slider, LV_ALIGN_TOP_LEFT);
    lv_obj_add_flag(ui_Slider, LV_OBJ_FLAG_SCROLL_ON_FOCUS); /// Flags
    lv_obj_remove_flag(ui_Slider, LV_OBJ_FLAG_SCROLLABLE);   /// Flags
    lv_obj_set_style_bg_color(ui_Slider, lv_color_hex(COLOR_TILE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Slider, lv_color_hex(COLOR_TEXT), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Slider, lv_color_white(), LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Slider, lv_color_hex(COLOR_WAIT), LV_PART_KNOB | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(ui_Slider, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui_Slider, 9, LV_PART_KNOB);
    lv_slider_set_range(ui_Slider, 0, 100);
    lv_obj_set_ext_click_area(ui_Slider, 20);
    lv_slider_set_value(ui_Slider, 100, LV_ANIM_OFF);
    lv_obj_add_event_cb(ui_Slider, ui_event_Slider, LV_EVENT_ALL, NULL);

    brightness_title = lv_label_create(parent);
    lv_obj_set_style_text_font(brightness_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(brightness_title, lv_color_hex(COLOR_MUTED), 0);
    lv_obj_align_to(brightness_title, parent, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_label_set_text(brightness_title, "BRIGHTNESS");

    brightness_value = lv_label_create(parent);
    lv_obj_set_style_text_font(brightness_value, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(brightness_value, lv_color_hex(COLOR_MUTED), 0);
    lv_obj_align_to(brightness_value, parent, LV_ALIGN_TOP_RIGHT, 0, 0);
    ui_set_brightness_value_text(100);
}