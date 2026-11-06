#include "screen_main.h"
#include "theme.h"

static lv_style_t style_card;

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
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_add_style(card, &style_card, 0);
    lv_obj_set_style_pad_all(card, CARD_PAD, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    return card;
}

void screen_main_create(void) {
    styles_init();
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(COLOR_BG), 0);
    lv_obj_t *card_weather = make_card(screen, 16, 88, 280, 256);
    lv_obj_t *card_brightness = make_card(screen, 16, 360, 280, 104);
    lv_obj_t *card_jobs = make_card(screen, 312, 88, 472, 376);
    lv_screen_load(screen);
}
