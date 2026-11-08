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

lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font, uint32_t color, const char *text) {
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_label_set_text(label, text);
    return label;
}

lv_obj_t *make_card_title(lv_obj_t *parent, const char *text) {
    lv_obj_t *title = make_label(parent, &lv_font_montserrat_14, COLOR_MUTED, text);
    lv_obj_set_style_text_letter_space(title, 1, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);
    return title;
}

lv_obj_t *make_blob(lv_obj_t *parent, int x, int y, int w, int h, int radius, uint32_t color) {
    lv_obj_t *blob = lv_obj_create(parent);
    lv_obj_set_size(blob, w, h);
    lv_obj_set_pos(blob, x, y);
    lv_obj_set_style_bg_color(blob, lv_color_hex(color), 0);
    lv_obj_set_style_radius(blob, radius, 0);
    lv_obj_set_style_border_width(blob, 0, 0);
    // Not clickable: a tap on a shape goes to what's under it (the weather card that opens the hourly view).
    lv_obj_remove_flag(blob, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return blob;
}
