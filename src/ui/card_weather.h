#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
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

// Fill the whole card: numbers, the condition text and the icon, both from w->code.
// The card copies what it needs, so w may be freed after the call.
// Call from the LVGL loop (loop()/serial handler), never from another task.
void card_weather_set(const weather_update_t *w);

// Checking aid: shows every weather icon in turn (4 s each) with its condition text.
// Call once after screen_main_create() in setup(); remove the call when you're done looking.
void card_weather_demo_start(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif
