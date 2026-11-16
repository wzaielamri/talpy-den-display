#include "talpy.h"
#include "ui/talpy_codec.h"
#include "ui/talpy_set.h"
#include "esp_heap_caps.h"
#include "esp_random.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TICK_MS 20 // the player works in steps of 100 ms; this is how often we look

typedef struct {
    lv_obj_t *img;
    lv_timer_t *timer; // NULL for a still
    const talpy_set_t *set;
    talpy_player_t player;
    lv_image_dsc_t dsc;
    uint8_t *buf;
} talpy_t;

// Decode one frame into the widget's buffer and redraw. A frame that doesn't decode leaves the last one.
static void show_frame(talpy_t *t, uint16_t i) {
    const talpy_frame_t *f = &t->set->frames[i];
    if (!talpy_decode(t->set->data + f->offset, f->size, f->w, f->h, t->buf)) {
        static bool told; // once for all widgets: a frame that keeps failing must not flood the UART
        if (!told) printf("talpy: frame %u doesn't decode\n", (unsigned)i);
        told = true;
        return;
    }
    lv_image_cache_drop(&t->dsc);
    lv_obj_invalidate(t->img);
}

static void on_timer(lv_timer_t *timer) {
    talpy_t *t = (talpy_t *)lv_timer_get_user_data(timer);
    if (talpy_player_tick(&t->player, lv_tick_get())) show_frame(t, talpy_player_frame(&t->player));
}

static void on_delete(lv_event_t *e) {
    talpy_t *t = (talpy_t *)lv_event_get_user_data(e);
    if (t->timer) lv_timer_delete(t->timer);
    heap_caps_free(t->buf);
    free(t);
}

static uint8_t *alloc_buf(size_t size) {
    uint8_t *b = (uint8_t *)heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!b) b = (uint8_t *)heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (b) memset(b, 0, size); // transparent, so a first frame that doesn't decode never shows leftover RAM
    return b;
}

// The state for a w x h picture on img; NULL when there is no memory (the image then stays empty, nothing to free).
static talpy_t *make(lv_obj_t *img, const talpy_set_t *set, uint16_t w, uint16_t h) {
    size_t size = (size_t)w * h * 3;
    talpy_t *t = (talpy_t *)calloc(1, sizeof(talpy_t));
    uint8_t *buf = t ? alloc_buf(size) : NULL;
    if (!buf) {
        static bool told;
        if (!told) printf("talpy: no memory for a %ux%u picture\n", (unsigned)w, (unsigned)h);
        told = true;
        free(t);
        return NULL;
    }
    t->img = img;
    t->set = set;
    t->buf = buf;
    t->dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    t->dsc.header.cf = LV_COLOR_FORMAT_RGB565A8;
    t->dsc.header.w = w;
    t->dsc.header.h = h;
    t->dsc.header.stride = (uint32_t)w * 2;
    t->dsc.data_size = (uint32_t)size;
    t->dsc.data = buf;
    lv_obj_set_user_data(img, t);
    lv_obj_add_event_cb(img, on_delete, LV_EVENT_DELETE, t);
    return t;
}

static lv_obj_t *new_image(lv_obj_t *parent) {
    lv_obj_t *img = lv_image_create(parent);
    lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
    return img;
}

lv_obj_t *talpy_create_set(lv_obj_t *parent, const talpy_set_t *set) {
    lv_obj_t *img = new_image(parent);
    talpy_t *t = make(img, set, set->w, set->h);
    if (!t) return img;
    talpy_player_init(&t->player, set, esp_random(), lv_tick_get());
    show_frame(t, talpy_player_frame(&t->player));
    lv_image_set_src(img, &t->dsc);
    t->timer = lv_timer_create(on_timer, TICK_MS, t);
    return img;
}

lv_obj_t *talpy_create(lv_obj_t *parent) { return talpy_create_set(parent, &TALPY_SET); }

lv_obj_t *talpy_create_still(lv_obj_t *parent, uint16_t frame) {
    lv_obj_t *img = new_image(parent);
    if (frame >= TALPY_SET.frame_count) frame = 0;
    const talpy_frame_t *f = &TALPY_SET.frames[frame];
    talpy_t *t = make(img, &TALPY_SET, f->w, f->h);
    if (!t) return img;
    show_frame(t, frame);
    lv_image_set_src(img, &t->dsc);
    return img;
}

// The state of a moving Talpy, or NULL (no object, no memory, or a still).
static talpy_t *moving(lv_obj_t *obj) {
    talpy_t *t = obj ? (talpy_t *)lv_obj_get_user_data(obj) : NULL;
    return t && t->timer ? t : NULL;
}

void talpy_set_mood(lv_obj_t *obj, uint16_t mood) {
    talpy_t *t = moving(obj);
    if (!t) return;
    uint16_t before = talpy_player_frame(&t->player);
    talpy_player_set_mood(&t->player, mood, lv_tick_get());
    if (talpy_player_frame(&t->player) != before) show_frame(t, talpy_player_frame(&t->player));
}

void talpy_set_pace(lv_obj_t *obj, uint8_t pace) {
    talpy_t *t = moving(obj);
    if (t) talpy_player_set_pace(&t->player, pace);
}

void talpy_play_once(lv_obj_t *obj, uint16_t clip) {
    talpy_t *t = moving(obj);
    if (t && talpy_player_play_once(&t->player, clip, lv_tick_get())) show_frame(t, talpy_player_frame(&t->player));
}

void talpy_react_finished(lv_obj_t *obj) {
    talpy_t *t = moving(obj);
    if (t && talpy_player_react(&t->player, lv_tick_get())) show_frame(t, talpy_player_frame(&t->player));
}

void talpy_freeze(lv_obj_t *obj, bool on) {
    talpy_t *t = moving(obj);
    if (!t) return;
    if (on) lv_timer_pause(t->timer);
    else lv_timer_resume(t->timer);
}
