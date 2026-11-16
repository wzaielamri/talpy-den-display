#pragma once
#include "ui/card_weather.h"
#include "ui/card_hourly.h"

#ifdef __cplusplus
extern "C" {
#endif

// What the weather card shows, from what serial_link hears. It keeps the last weather and hourly messages and whether
// the PC is there, and on every input (and once a minute, so the forecast moves on to the next hour) works out the
// picture and texts (weather_view.h) and gives them to the card (card_weather_view).
// The minute timer is an lv_timer made on the first call of any of these. Call from the LVGL loop, like the cards.
// Before any message the PC is assumed there (as serial_link does): plain Talpy, "Waiting for PC".

void weather_state_weather(const weather_update_t *w); // a weather message (copied)
void weather_state_hourly(const hourly_update_t *u);   // an hourly message (copied; the first HOURLY_COLS hours)
void weather_state_pc(bool online);                    // the PC came or went
void weather_state_tick(void);                         // recompute now (the timer calls it each minute)

#ifdef __cplusplus
} /*extern "C"*/
#endif
