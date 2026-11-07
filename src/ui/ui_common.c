#include "ui_common.h"
#include "theme.h"


static lv_style_t style_card;


void styles_init(void) {
    lv_style_init(&style_card); // empty the bag (once!)
    lv_style_set_bg_color(&style_card, lv_color_hex(COLOR_CARD));
    lv_style_set_radius(&style_card, CARD_RADIUS);
    lv_style_set_pad_all(&style_card, CARD_PAD);
    lv_style_set_border_width(&style_card, 0);
}

lv_obj_t *make_card(lv_obj_t *parent, int x, int y, int w, int h) {
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, w, h);
    lv_obj_set_pos(card, x, y);
    lv_obj_add_style(card, &style_card, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    return card;
}
