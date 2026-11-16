#include "weather_state.h"
#include "clock.h"
#include "lvgl.h"
#include "ui/clock_face.h"
#include "ui/weather_view.h"
#include <time.h>

static bool pc_online = true; // as serial_link's: the PC is assumed there at start
static bool have_weather;
static int w_code, w_temp;
static bool w_day;
static wv_hour_t hours[HOURLY_COLS];
static int hour_count;
static lv_timer_t *minute;

static void push(void) {
    time_t now = time(NULL);
    wv_input_t in = {
        .pc_online = pc_online,
        .have_weather = have_weather,
        .code = w_code,
        .is_day = w_day,
        .temp = w_temp,
        .hours = hours,
        .hour_count = hour_count,
        .clock_known = now >= CLOCK_SET,
        .now = now,
    };
    wv_view_t v = weather_view(&in);
    card_weather_view(&v); // (the card ignores what has not changed)
    clock_face_set_view(&v);
}

static void on_minute(lv_timer_t *t) {
    (void)t;
    push();
}

static void ensure_timer(void) {
    if (!minute) minute = lv_timer_create(on_minute, 60000, NULL);
}

void weather_state_weather(const weather_update_t *w) {
    if (!w) return;
    ensure_timer();
    have_weather = true;
    w_code = w->code;
    w_temp = w->temp;
    w_day = w->is_day;
    push();
}

void weather_state_hourly(const hourly_update_t *u) {
    if (!u) return;
    ensure_timer();
    hour_count = 0;
    for (int i = 0; i < u->count && i < HOURLY_COLS; i++)
        hours[hour_count++] = (wv_hour_t){u->hours[i].at, u->hours[i].temp, u->hours[i].code, u->hours[i].is_day};
    push();
}

void weather_state_pc(bool online) {
    ensure_timer();
    pc_online = online;
    push();
}

void weather_state_tick(void) { push(); }
