#include "header_pc.h"
#include "theme.h"
#include "ui_common.h"

static lv_obj_t *pc_pill;
static lv_obj_t *pc_text;

void header_pc_set_online(bool online) {
    if (!pc_pill) return; // header not created yet
    lv_obj_set_style_bg_color(pc_pill, lv_color_hex(online ? COLOR_PC : COLOR_TILE), 0);
    lv_obj_set_style_text_color(pc_text, lv_color_hex(online ? COLOR_DONE : COLOR_MUTED), 0);
    lv_label_set_text(pc_text, online ? LV_SYMBOL_USB " PC connected" : LV_SYMBOL_USB " PC offline");
}

void header_pc_create(lv_obj_t *parent) {
    pc_pill = make_blob(parent, 0, 0, 168, 32, LV_RADIUS_CIRCLE, COLOR_PC);
    lv_obj_align(pc_pill, LV_ALIGN_TOP_RIGHT, -16, 24);
    lv_obj_set_style_pad_all(pc_pill, 0, 0);

    pc_text = make_label(pc_pill, &lv_font_montserrat_14, COLOR_DONE, "");
    lv_obj_align(pc_text, LV_ALIGN_CENTER, 0, 0);

    // Made-up state from the design page until your step-10 logic decides it.
    header_pc_set_online(true);
}
