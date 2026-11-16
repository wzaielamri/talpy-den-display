// Building blocks shared by the weather icons and the live sky (see weather_anim.h).

#include "weather_anim.h"

lv_obj_t *wx_group(lv_obj_t *parent, int x, int y, int w, int h) {
    lv_obj_t *g = lv_obj_create(parent);
    lv_obj_remove_style_all(g);
    lv_obj_set_size(g, w, h);
    lv_obj_set_pos(g, x, y);
    lv_obj_remove_flag(g, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return g;
}

void wx_anim_x(void *obj, int32_t v) { lv_obj_set_x((lv_obj_t *)obj, v); }
void wx_anim_y(void *obj, int32_t v) { lv_obj_set_y((lv_obj_t *)obj, v); }

void wx_anim_bounce(lv_anim_t *a, void *var, lv_anim_exec_xcb_t cb, int32_t from, int32_t to, uint32_t ms,
                    uint32_t delay) {
    lv_anim_init(a);
    lv_anim_set_var(a, var);
    lv_anim_set_exec_cb(a, cb);
    lv_anim_set_values(a, from, to);
    lv_anim_set_duration(a, ms);
    lv_anim_set_playback_duration(a, ms);
    lv_anim_set_delay(a, delay);
    lv_anim_set_path_cb(a, lv_anim_path_ease_in_out);
    lv_anim_set_repeat_count(a, LV_ANIM_REPEAT_INFINITE);
}

void wx_anim_fall(lv_anim_t *a, void *var, lv_anim_exec_xcb_t cb, int32_t from, int32_t to, uint32_t ms,
                  uint32_t delay, lv_anim_path_cb_t path) {
    lv_anim_init(a);
    lv_anim_set_var(a, var);
    lv_anim_set_exec_cb(a, cb);
    lv_anim_set_values(a, from, to);
    lv_anim_set_duration(a, ms);
    lv_anim_set_delay(a, delay);
    lv_anim_set_path_cb(a, path);
    lv_anim_set_repeat_count(a, LV_ANIM_REPEAT_INFINITE);
}
