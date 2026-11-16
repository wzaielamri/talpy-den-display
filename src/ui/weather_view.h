#pragma once
#include "ui/talpy_set.h"
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// What the weather card and the clock face show, from the last weather message, the last hourly message, whether the
// PC is connected and the board's clock. Pure C (host tests build it).
// PC connected: the live weather with Talpy out (no weather yet: nothing). PC away: the forecast's current hour
// (at <= now < at + 3600) with Talpy hidden in his burrow; no clock or no such hour: nothing, Talpy still hidden.

typedef struct {
    time_t at;
    int temp;
    int code;
    bool is_day;
} wv_hour_t;

typedef struct {
    bool pc_online;
    bool have_weather;
    int code;
    bool is_day;
    int temp; // the last weather message
    const wv_hour_t *hours;
    int hour_count; // the last hourly message (any order); NULL or <= 0: none
    bool clock_known;
    time_t now; // the board's clock
} wv_input_t;

typedef enum { WV_LIVE, WV_FORECAST, WV_NONE } wv_source_t;

typedef struct {
    tw_mood_t talpy; // TW_PLAIN, TW_HIDDEN or a weather
    tw_mood_t sky;   // a weather, or TW_PLAIN: no sky
    wv_source_t source;
    bool has_temp;
    int temp;
    int code;           // WV_LIVE / WV_FORECAST: for the condition text; WV_NONE: 0
    time_t forecast_at; // WV_FORECAST: the hour shown; otherwise 0
} wv_view_t;

// The picture for a WMO code; day or night matters only for clear and partly cloudy. Unknown codes: overcast.
tw_mood_t weather_mood(int wmo_code, bool is_day);

wv_view_t weather_view(const wv_input_t *in);

#ifdef __cplusplus
} /*extern "C"*/
#endif
