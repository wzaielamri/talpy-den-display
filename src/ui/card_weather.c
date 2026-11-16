#include "card_weather.h"
#include "theme.h"
#include "ui_common.h"
#include "app/clock.h"
#include "weather_talpy.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static lv_obj_t *wx_title;
static lv_obj_t *wx_temp;
static lv_obj_t *wx_cond;
static lv_obj_t *wx_hilo;
static lv_obj_t *wx_note;
static lv_obj_t *wx_talpy; // the weather Talpy, top right (104 x 96, not clickable: the card's tap opens the hourly view)

// The last weather message, kept to bring its texts back after a forecast or "PC offline" showed.
static bool have_live;
static weather_update_t live;
static char live_updated[16];
static int live_day = -1; // the day it came (year * 366 + yday); -1: the board's clock was not set

// WMO code -> condition text, as Open-Meteo lists the codes.
// Every text fits the card's 240 px in montserrat_24 (the longest, "Thunder and hail", is 208 px).
static const char *wmo_text(int code) {
    switch (code) {
    case 0:  return "Clear";
    case 1:  return "Mainly clear";
    case 2:  return "Partly cloudy";
    case 3:  return "Overcast";
    case 45: return "Fog";
    case 48: return "Rime fog";
    case 51: return "Light drizzle";
    case 53: return "Drizzle";
    case 55: return "Heavy drizzle";
    case 56:
    case 57: return "Freezing drizzle";
    case 61: return "Light rain";
    case 63: return "Rain";
    case 65: return "Heavy rain";
    case 66:
    case 67: return "Freezing rain";
    case 71: return "Light snow";
    case 73: return "Snow";
    case 75: return "Heavy snow";
    case 77: return "Snow grains";
    case 80: return "Light showers";
    case 81: return "Showers";
    case 82: return "Heavy showers";
    case 85:
    case 86: return "Snow showers";
    case 95: return "Thunderstorm";
    case 96:
    case 99: return "Thunder and hail";
    default: return "--";
    }
}

static int day_key(time_t t) {
    struct tm tm;
    localtime_r(&t, &tm);
    return tm.tm_year * 366 + tm.tm_yday;
}

// Only touches the label when its text differs (the view is pushed every minute).
static void set_text(lv_obj_t *l, const char *s) {
    if (strcmp(lv_label_get_text(l), s) != 0) lv_label_set_text(l, s);
}

static void show_hilo(void) {
    char b[48];
    bool stale = have_live && live_day >= 0 && time(NULL) >= CLOCK_SET && day_key(time(NULL)) != live_day;
    if (!have_live || stale) snprintf(b, sizeof b, LV_SYMBOL_UP " --°   " LV_SYMBOL_DOWN " --°");
    else snprintf(b, sizeof b, LV_SYMBOL_UP " %d°   " LV_SYMBOL_DOWN " %d°", live.hi, live.lo);
    set_text(wx_hilo, b);
}

static void show_note_live(void) {
    char b[48];
    snprintf(b, sizeof b, "Open-Meteo " LV_SYMBOL_BULLET " %s", have_live ? live_updated : "--:--");
    set_text(wx_note, b);
}

void card_weather_set(const weather_update_t *w) {
    if (!w || !wx_temp) return; // no data, or card not created yet
    have_live = true;
    live = *w;
    snprintf(live_updated, sizeof live_updated, "%s", w->updated ? w->updated : "--:--");
    live.updated = live_updated;
    time_t now = time(NULL);
    live_day = now >= CLOCK_SET ? day_key(now) : -1;
    char b[24];
    snprintf(b, sizeof b, "%d°", w->temp);
    set_text(wx_temp, b);
    set_text(wx_cond, wmo_text(w->code));
    show_hilo();
    show_note_live();
}

void card_weather_view(const wv_view_t *v) {
    if (!v || !wx_temp) return;
    weather_talpy_set(wx_talpy, v->talpy, v->sky);
    char b[48];
    switch (v->source) {
    case WV_LIVE:
        if (!have_live) break; // (the live texts come with the weather message)
        snprintf(b, sizeof b, "%d°", live.temp);
        set_text(wx_temp, b);
        set_text(wx_cond, wmo_text(live.code));
        show_hilo();
        show_note_live();
        break;
    case WV_FORECAST: {
        struct tm tm;
        time_t at = v->forecast_at;
        localtime_r(&at, &tm);
        snprintf(b, sizeof b, "%d°", v->temp);
        set_text(wx_temp, b);
        set_text(wx_cond, wmo_text(v->code));
        show_hilo();
        snprintf(b, sizeof b, "Forecast " LV_SYMBOL_BULLET " %02d:%02d", tm.tm_hour, tm.tm_min);
        set_text(wx_note, b);
        break;
    }
    case WV_NONE:
        set_text(wx_temp, "--°");
        // Talpy hidden: the PC is away; otherwise it is there and its weather not yet.
        set_text(wx_cond, v->talpy == TW_HIDDEN ? "PC offline" : "Waiting for PC");
        show_hilo();
        show_note_live();
        break;
    }
}

void card_weather_freeze(bool on) { weather_talpy_freeze(wx_talpy, on); }

void card_weather_create(lv_obj_t *parent){

    wx_title = make_card_title(parent, "WEATHER");

    wx_temp = make_label(parent, &lv_font_montserrat_48, COLOR_TEXT, "");
    lv_obj_align(wx_temp, LV_ALIGN_TOP_LEFT, 0, 32);

    wx_cond = make_label(parent, &lv_font_montserrat_24, COLOR_TEXT, "");
    lv_obj_align(wx_cond, LV_ALIGN_TOP_LEFT, 0, 92);

    wx_hilo = make_label(parent, &lv_font_montserrat_24, COLOR_MUTED, "");
    lv_obj_align(wx_hilo, LV_ALIGN_TOP_LEFT, 0, 128);

    wx_note = make_label(parent, &lv_font_montserrat_14, COLOR_DIM, "");
    lv_obj_align(wx_note, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    // Says the card can be tapped: it opens the hourly forecast (screen_main.c).
    lv_obj_t *wx_more = make_label(parent, &lv_font_montserrat_14, COLOR_MUTED, "Hourly " LV_SYMBOL_RIGHT);
    lv_obj_align(wx_more, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    // Top-right corner, above the condition text: Talpy in front of his sky (it starts as plain Talpy, no sky).
    wx_talpy = weather_talpy_create(parent, false);
    lv_obj_set_pos(wx_talpy, 140, 0);

    // Until the PC's first weather message: nothing made up, the card says it waits.
    lv_label_set_text(wx_temp, "--°");
    lv_label_set_text(wx_cond, "Waiting for PC");
    lv_label_set_text(wx_hilo, LV_SYMBOL_UP " --°   " LV_SYMBOL_DOWN " --°");
    lv_label_set_text(wx_note, "Open-Meteo " LV_SYMBOL_BULLET " --:--");
}
