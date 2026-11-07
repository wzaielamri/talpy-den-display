#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

void styles_init(void);
lv_obj_t *make_card(lv_obj_t *parent, int x, int y, int w, int h);

// Label with font, color (0xRRGGBB) and text. Position it yourself afterwards.
lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font, uint32_t color, const char *text);

// Small muted caption in the card's top-left corner ("WEATHER", "BRIGHTNESS", ...).
lv_obj_t *make_card_title(lv_obj_t *parent, const char *text);

// Solid rounded shape with no border, for simple icons (sun, cloud puffs, status dots).
// radius: pixels, or LV_RADIUS_CIRCLE for a full circle (w == h) / pill (w != h).
lv_obj_t *make_blob(lv_obj_t *parent, int x, int y, int w, int h, int radius, uint32_t color);

#ifdef __cplusplus
} /*extern "C"*/
#endif
