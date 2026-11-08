#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include <stdbool.h>
#include <time.h>

#define HOURLY_COLS 8 // the next 8 hours, now on the left

// One hour, as the PC sends it (Open-Meteo hourly).
typedef struct {
    time_t at;   // the hour's start, Unix seconds; the first column says "Now" whatever it is
    int temp;    // °C, rounded (temperature_2m)
    int code;    // WMO weather code (weather_code)
    bool is_day; // is_day
    int rain;    // chance of rain, 0..100 (precipitation_probability); -1: not known
} hour_t;

typedef struct {
    const hour_t *hours;  // from now on; past HOURLY_COLS left out
    int count;
    const char *summary;  // one line about them, "Rain likely from 16:00 to 18:00"; NULL or "": none
    const char *updated;  // fetch time "14:45"; NULL shows "--:--"
} hourly_update_t;

// The hourly forecast: a card over the screen's three cards (the header stays live above it), hidden
// until card_hourly_show(). Create it after the other cards, on the screen, so it covers them.
void card_hourly_create(lv_obj_t *screen);

// Fill it: a column per hour (hour, icon, temperature, chance of rain), the temperature line over them.
// Copies what it needs. Call from the LVGL loop (loop()/serial handler), never from another task.
void card_hourly_set(const hourly_update_t *u);

void card_hourly_show(void); // the weather card's tap
void card_hourly_hide(void); // Back, or 60 s without a touch

#ifdef __cplusplus
} /*extern "C"*/
#endif
