#include "card_weather.h"
#include "theme.h"
#include "ui_common.h"
#include "weather_icon.h"

static lv_obj_t *wx_title;
static lv_obj_t *wx_temp;
static lv_obj_t *wx_cond;
static lv_obj_t *wx_hilo;
static lv_obj_t *wx_note;

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

void card_weather_set(const weather_update_t *w) {
    if (!w || !wx_temp) return; // no data, or card not created yet
    lv_label_set_text_fmt(wx_temp, "%d°", w->temp);
    lv_label_set_text(wx_cond, wmo_text(w->code));
    lv_label_set_text_fmt(wx_hilo, LV_SYMBOL_UP " %d°   " LV_SYMBOL_DOWN " %d°", w->hi, w->lo);
    lv_label_set_text_fmt(wx_note, "Open-Meteo " LV_SYMBOL_BULLET " %s", w->updated ? w->updated : "--:--");
    weather_icon_set(w->code, w->is_day);
}

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

    // Top-right corner, above the condition text. Its partly-cloudy icon puts the sun and
    // cloud exactly where the design page has them.
    weather_icon_create(parent, 140, 0);

    // Made-up values until the PC sends real ones (plan step 8).
    weather_update_t w = {
        .temp = 22,
        .hi = 25,
        .lo = 18,
        .code = 2,     // partly cloudy
        .is_day = true,
        .updated = "14:45",
    };
    card_weather_set(&w);
}

// ---------- demo: every icon in turn, to check them on the board ----------

typedef struct {
    int code;
    bool day;
} wx_demo_t;

static const wx_demo_t demo[] = {
    {0, true},  {0, false}, {2, true},  {2, false}, {3, true},
    {45, true}, {53, true}, {63, true}, {73, true}, {95, true},
};

static void demo_tick(lv_timer_t *timer) {
    LV_UNUSED(timer);
    static unsigned i = 0;
    weather_icon_set(demo[i].code, demo[i].day);
    lv_label_set_text(wx_cond, wmo_text(demo[i].code));
    i = (i + 1) % (sizeof demo / sizeof demo[0]);
}

void card_weather_demo_start(void) {
    lv_timer_create(demo_tick, 4000, NULL);
    demo_tick(NULL);
}
