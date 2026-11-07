#include "card_weather.h"
#include "theme.h"
#include "ui_common.h"

static lv_obj_t *wx_sun;
static lv_obj_t *wx_cloud_base;
static lv_obj_t *wx_cloud_a;
static lv_obj_t *wx_cloud_b;
static lv_obj_t *wx_title;
static lv_obj_t *wx_temp;
static lv_obj_t *wx_cond;
static lv_obj_t *wx_hilo;
static lv_obj_t *wx_note;

void card_weather_set(int temp, int hi, int lo, const char *condition, const char *updated) {
    if (!wx_temp) return; // card not created yet
    lv_label_set_text_fmt(wx_temp, "%d°", temp);
    lv_label_set_text(wx_cond, condition ? condition : "--");
    lv_label_set_text_fmt(wx_hilo, LV_SYMBOL_UP " %d°   " LV_SYMBOL_DOWN " %d°", hi, lo);
    lv_label_set_text_fmt(wx_note, "Open-Meteo " LV_SYMBOL_BULLET " %s", updated ? updated : "--:--");
}

void card_weather_create(lv_obj_t *parent){

    wx_title = make_card_title(parent, "WEATHER");

    wx_temp = make_label(parent, &lv_font_montserrat_48, COLOR_TEXT, "");
    lv_obj_align(wx_temp, LV_ALIGN_TOP_LEFT, 0, 32);

    wx_cond = make_label(parent, &lv_font_montserrat_24, COLOR_TEXT, "");
    lv_obj_align(wx_cond, LV_ALIGN_TOP_LEFT, 0, 92);

    wx_hilo = make_label(parent, &lv_font_montserrat_24, COLOR_MUTED, "");
    lv_obj_align(wx_hilo, LV_ALIGN_TOP_LEFT, 0, 128);

    wx_note = make_label(parent, &lv_font_montserrat_14, COLOR_DIM, "");
    lv_obj_align(wx_note, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    // Partly-cloudy icon. Order matters: later shapes are drawn on top,
    // so the sun goes first and the clouds cover its lower half.
    //                            x    y   w   h   radius            color
    wx_sun        = make_blob(parent, 176, 18, 52, 52, LV_RADIUS_CIRCLE, COLOR_SUN);
    wx_cloud_base = make_blob(parent, 150, 62, 88, 34, 17,               COLOR_CLOUD);
    wx_cloud_a    = make_blob(parent, 162, 46, 40, 40, LV_RADIUS_CIRCLE, COLOR_CLOUD);
    wx_cloud_b    = make_blob(parent, 190, 36, 48, 48, LV_RADIUS_CIRCLE, COLOR_CLOUD);

    // Made-up values from the design page until the PC sends real ones (plan step 8).
    card_weather_set(12, 14, 7, "Partly cloudy", "14:45");
}
