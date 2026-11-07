#include "card_weather.h"
#include "theme.h"
#include "ui_common.h"
#include "weather_icon.h"

static lv_obj_t *wx_title;
static lv_obj_t *wx_temp;
static lv_obj_t *wx_cond;
static lv_obj_t *wx_hilo;
static lv_obj_t *wx_note;

void card_weather_set(int temp, int hi, int lo, const char *condition, const char *updated) {
    if (!wx_temp) return; // card not created yet
    lv_label_set_text_fmt(wx_temp, "%d°", temp);
    lv_label_set_text(wx_cond, condition ? condition : "--");
    lv_label_set_text_fmt(wx_hilo, LV_SYMBOL_UP " %d°   " LV_SYMBOL_DOWN " %d°", hi, lo);
    lv_label_set_text_fmt(wx_note, "Open-Meteo " LV_SYMBOL_BULLET " %s", updated ? updated : "--:--");
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

    // Top-right corner, above the condition text. Its partly-cloudy icon puts the sun and
    // cloud exactly where the design page has them.
    weather_icon_create(parent, 140, 0);

    // Made-up values from the design page until the PC sends real ones (plan step 8).
    card_weather_set(12, 14, 7, "Partly cloudy", "14:45");
    weather_icon_set(2, true);
}

// ---------- demo: every icon in turn, to check them on the board ----------

typedef struct {
    int code;
    bool day;
    const char *name;
} wx_demo_t;

static const wx_demo_t demo[] = {
    {0, true, "Clear"},          {0, false, "Clear night"}, {2, true, "Partly cloudy"},
    {2, false, "Cloudy night"},  {3, true, "Overcast"},     {45, true, "Fog"},
    {53, true, "Drizzle"},       {63, true, "Rain"},        {73, true, "Snow"},
    {95, true, "Thunderstorm"},
};

static void demo_tick(lv_timer_t *timer) {
    LV_UNUSED(timer);
    static unsigned i = 0;
    weather_icon_set(demo[i].code, demo[i].day);
    lv_label_set_text(wx_cond, demo[i].name);
    i = (i + 1) % (sizeof demo / sizeof demo[0]);
}

void card_weather_demo_start(void) {
    lv_timer_create(demo_tick, 4000, NULL);
    demo_tick(NULL);
}
