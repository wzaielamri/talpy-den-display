#pragma once
#include "lvgl.h"
#include "ui/weather_view.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// The clock face: the time big in the middle, the date under it, the weather Talpy (calm: slow, with a calm sky)
// and the temperature under that, then one line saying how the chats stand, over everything else on the main screen.
// screen_power.c shows it after 2 min without a touch (no job working or waiting, no "Stay on") and while the PC is
// away, dimmed; a touch on it calls the touch callback (back to the cards). It's what the screen shows 24/7 when
// nobody looks, so little on it moves, and slowly: the calm weather Talpy, the time drawn again once a minute, and the
// whole face moving a few pixels each minute, so no picture stays in one place on the LCD for hours.
void clock_face_create(lv_obj_t *screen);

typedef void (*clock_face_touch_cb_t)(void);
void clock_face_on_touch(clock_face_touch_cb_t cb);

void clock_face_show(void);
void clock_face_hide(void);

// The weather Talpy and temperature (the weather card's view, from weather_state), under the date. The temperature
// shows only while v->has_temp; Talpy always shows (plain, hidden or in a weather).
void clock_face_set_view(const wv_view_t *v);

// The chat line under the weather ("1 chat needs you", ...; banner_headline's words), hidden while the PC is away.
// Till the first jobs message it says "Waiting for the PC".
void clock_face_set_chats(bool have_data, int waiting, int working, int done);

// "PC offline" at its bottom while the PC isn't there (the chat line is hidden meanwhile).
void clock_face_set_pc(bool online);

#ifdef __cplusplus
} /*extern "C"*/
#endif
