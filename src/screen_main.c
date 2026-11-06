#include "screen_main.h"
#include "backlight.h"
#include "theme.h"

static lv_style_t style_card;

lv_obj_t *ui_Slider = NULL;

static void styles_init(void) {
    lv_style_init(&style_card); // empty the bag (once!)
    lv_style_set_bg_color(&style_card, lv_color_hex(COLOR_CARD));
    lv_style_set_radius(&style_card, CARD_RADIUS);
    lv_style_set_pad_all(&style_card, CARD_PAD);
    lv_style_set_border_width(&style_card, 0);
}

static lv_obj_t *make_card(lv_obj_t *parent, int x, int y, int w, int h) {
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, w, h);
    lv_obj_set_pos(card, x, y);
    lv_obj_add_style(card, &style_card, 0);
    lv_obj_set_style_pad_all(card, CARD_PAD, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    return card;
}

void ui_event_Slider(lv_event_t *e) {
    lv_event_code_t event_code = lv_event_get_code(e);
    if (event_code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *slider = lv_event_get_target_obj(e);
        int32_t value = lv_slider_get_value(slider);
        backlight_set_percent(value);
    }
}

void screen_main_create(void) {
    styles_init();
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(COLOR_BG), 0);
    lv_obj_t *card_weather = make_card(screen, 16, 88, 280, 256);
    lv_obj_t *card_brightness = make_card(screen, 16, 360, 280, 104);
    lv_obj_t *card_jobs = make_card(screen, 312, 88, 472, 376);

    // create the UI elements inside the cards
    ui_Slider = lv_slider_create(card_brightness);
    lv_obj_set_width(ui_Slider, 240);
    lv_obj_set_height(ui_Slider, 10);
    lv_obj_set_x(ui_Slider, 0);
    lv_obj_set_y(ui_Slider, 38);
    lv_obj_set_align(ui_Slider, LV_ALIGN_TOP_LEFT);
    lv_obj_add_flag(ui_Slider, LV_OBJ_FLAG_SCROLL_ON_FOCUS); /// Flags
    lv_obj_remove_flag(ui_Slider, LV_OBJ_FLAG_SCROLLABLE);   /// Flags
    lv_obj_set_style_bg_color(ui_Slider, lv_color_hex(COLOR_TEXT), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Slider, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_slider_set_range(ui_Slider, 0, 100);
    lv_slider_set_value(ui_Slider, 100, LV_ANIM_OFF);
    lv_obj_add_event_cb(ui_Slider, ui_event_Slider, LV_EVENT_ALL, NULL);

    lv_screen_load(screen);
}