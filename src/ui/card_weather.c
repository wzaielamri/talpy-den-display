#include "card_brightness.h"
#include "theme.h"
#include <stdio.h>

static lv_obj_t *ui_Slider = NULL;
static lv_obj_t *wx_sun;
static lv_obj_t *wx_cloud_base;
static lv_obj_t *wx_cloud_a;
static lv_obj_t *wx_cloud_b;
static lv_obj_t *wx_title;
static lv_obj_t *wx_temp;
static lv_obj_t *wx_cond;
static lv_obj_t *wx_hilo;
static lv_obj_t *wx_note;

static void ui_set_weather_value_text(int32_t value) {
    char buf[8];
    snprintf(buf, sizeof buf, "%ld°", (long)value);
    lv_label_set_text(wx_temp, buf);
    lv_obj_t *card_brightness = lv_obj_get_parent(wx_temp);
    lv_obj_align_to(wx_temp, card_brightness, LV_ALIGN_TOP_RIGHT, 0, 0);
}

void card_weather_create(lv_obj_t *parent){

    wx_title = lv_label_create(parent);
    lv_obj_set_style_text_font(wx_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(wx_title, lv_color_hex(COLOR_MUTED), 0);
    lv_obj_align_to(wx_title, parent, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_label_set_text(wx_title, "WEATHER");


    wx_temp = lv_label_create(parent);
    lv_obj_set_style_text_font(wx_temp, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(wx_temp, lv_color_hex(COLOR_TEXT), 0);
    lv_obj_align_to(wx_temp, parent, LV_ALIGN_TOP_LEFT, 0, 32);
    lv_label_set_text(wx_temp, "2°");

    wx_cond = lv_label_create(parent);
    lv_obj_set_style_text_font(wx_cond, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(wx_cond, lv_color_hex(COLOR_TEXT), 0);
    lv_obj_align_to(wx_cond, parent, LV_ALIGN_TOP_LEFT, 0, 92);
    lv_label_set_text(wx_cond, "Partly Cloudy");


    wx_note = lv_label_create(parent);
    lv_obj_set_style_text_font(wx_note, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(wx_note, lv_color_hex(COLOR_DIM), 0);
    lv_obj_align_to(wx_note, parent, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_label_set_text(wx_note, "Open-Meteo . --:--");

    wx_sun = lv_obj_create(parent);
    lv_obj_set_size(wx_sun, 52, 52);
    lv_obj_set_style_bg_color(wx_sun, lv_color_hex(COLOR_SUN), 0);
    lv_obj_align_to(wx_sun, parent, LV_ALIGN_TOP_LEFT, 176, 18);
    lv_obj_set_style_radius(wx_sun, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(wx_sun, 0, 0);
    lv_obj_remove_flag(wx_sun, LV_OBJ_FLAG_SCROLLABLE);



    wx_cloud_a = lv_obj_create(parent);
    lv_obj_set_size(wx_cloud_a, 40, 40);
    lv_obj_set_style_bg_color(wx_cloud_a, lv_color_hex(COLOR_CLOUD), 0);
    lv_obj_align_to(wx_cloud_a, parent, LV_ALIGN_TOP_LEFT, 162, 46);
    lv_obj_set_style_radius(wx_cloud_a, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(wx_cloud_a, 0, 0);
    lv_obj_remove_flag(wx_cloud_a, LV_OBJ_FLAG_SCROLLABLE);



    wx_cloud_b = lv_obj_create(parent);
    lv_obj_set_size(wx_cloud_b, 48, 48);
    lv_obj_set_style_bg_color(wx_cloud_b, lv_color_hex(COLOR_CLOUD), 0);
    lv_obj_align_to(wx_cloud_b, parent, LV_ALIGN_TOP_LEFT, 190, 36);
    lv_obj_set_style_radius(wx_cloud_b, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(wx_cloud_b, 0, 0);
    lv_obj_remove_flag(wx_cloud_b, LV_OBJ_FLAG_SCROLLABLE);
    
    wx_cloud_base = lv_obj_create(parent);
    lv_obj_set_size(wx_cloud_base, 88, 34);
    lv_obj_set_style_bg_color(wx_cloud_base, lv_color_hex(COLOR_CLOUD), 0);
    lv_obj_align_to(wx_cloud_base, parent, LV_ALIGN_TOP_LEFT, 150, 62);
    lv_obj_set_style_radius(wx_cloud_base, 17, 0);
    lv_obj_set_style_border_width(wx_cloud_base, 0, 0);
    lv_obj_remove_flag(wx_cloud_base, LV_OBJ_FLAG_SCROLLABLE);
    

}