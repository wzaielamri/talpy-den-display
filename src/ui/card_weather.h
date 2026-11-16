#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "ui/weather_view.h"
#include <stdbool.h>

// Everything the weather card shows, in one message.
typedef struct {
    int temp;              // °C, rounded
    int hi;                // today's high, °C
    int lo;                // today's low, °C
    int code;              // WMO code, Open-Meteo current.weather_code
    bool is_day;           // Open-Meteo current.is_day
    const char *updated;   // fetch time "14:45"; NULL shows "--:--"
} weather_update_t;

void card_weather_create(lv_obj_t *parent);

// Fill the card's live texts (numbers, condition, update time) from w; the picture and which texts show
// come from card_weather_view() (weather_state.c calls it right after).
// The card copies what it needs, so w may be freed after the call.
// Call from the LVGL loop (loop()/serial handler), never from another task.
void card_weather_set(const weather_update_t *w);

// What the card shows now: the weather Talpy and his sky, and per v->source the live texts (WV_LIVE), the forecast
// hour (WV_FORECAST) or "--°" with "PC offline" / "Waiting for PC" (WV_NONE). Cheap when nothing changed.
void card_weather_view(const wv_view_t *v);

// motion_hold: the weather Talpy (Talpy and sky) stand still while true.
void card_weather_freeze(bool on);

#ifdef __cplusplus
} /*extern "C"*/
#endif
