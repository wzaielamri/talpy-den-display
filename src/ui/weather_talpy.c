#include "weather_talpy.h"
#include "ui/talpy.h"
#include "ui/weather_sky.h"
#include <stdlib.h>

#define BOX_W 104
#define BOX_H 96
#define CALM_PACE 3

typedef struct {
    lv_obj_t *talpy;
    weather_sky_t *sky;
    bool calm;
    bool have;       // something has been set
    tw_mood_t talpy_mood;
} wt_t;

static void on_delete(lv_event_t *e) { free(lv_event_get_user_data(e)); }

static lv_obj_t *layer(lv_obj_t *parent) {
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, BOX_W, BOX_H);
    lv_obj_set_pos(o, 0, 0);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

static void on_flash(void *user) {
    wt_t *w = (wt_t *)user;
    if (w->talpy_mood == TW_THUNDER) talpy_play_once(w->talpy, TALPY_WEATHER_SET.react_clip);
}

lv_obj_t *weather_talpy_create(lv_obj_t *parent, bool calm) {
    lv_obj_t *box = layer(parent);
    wt_t *w = (wt_t *)calloc(1, sizeof(wt_t));
    if (!w) return box; // no memory: an empty box
    lv_obj_t *back = layer(box);
    w->talpy = talpy_create_set(box, &TALPY_WEATHER_SET);
    lv_obj_set_pos(w->talpy, TALPY_WEATHER_SET.x, TALPY_WEATHER_SET.y);
    lv_obj_t *front = layer(box);
    w->calm = calm;
    w->sky = weather_sky_create(back, front);
    if (calm) talpy_set_pace(w->talpy, CALM_PACE);
    if (w->sky) weather_sky_on_flash(w->sky, on_flash, w);
    lv_obj_set_user_data(box, w);
    lv_obj_add_event_cb(box, on_delete, LV_EVENT_DELETE, w);
    weather_talpy_set(box, TW_PLAIN, TW_PLAIN); // start as plain Talpy, no sky (not the set's first mood)
    return box;
}

void weather_talpy_set(lv_obj_t *box, tw_mood_t talpy, tw_mood_t sky) {
    wt_t *w = box ? (wt_t *)lv_obj_get_user_data(box) : NULL;
    if (!w) return;
    if (!w->have || talpy != w->talpy_mood) talpy_set_mood(w->talpy, talpy);
    // (the sky keeps running if it already shows this: the same weather, and Talpy out or in his burrow as before)
    if (w->sky) weather_sky_set(w->sky, sky, w->calm, talpy != TW_HIDDEN);
    w->have = true;
    w->talpy_mood = talpy;
}

void weather_talpy_freeze(lv_obj_t *box, bool on) {
    wt_t *w = box ? (wt_t *)lv_obj_get_user_data(box) : NULL;
    if (!w) return;
    talpy_freeze(w->talpy, on);
    if (w->sky) weather_sky_freeze(w->sky, on);
}
