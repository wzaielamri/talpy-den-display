#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include <time.h>

// Accounts the card has room for; more are left out (the PC picks which come first).
#define USAGE_ROWS 2

// One account's plan usage, as the PC sends it.
typedef struct {
    const char *name;        // the account, e.g. "work"
    const char *plan;        // e.g. "Pro", "Team"; NULL or "": none shown
    int five_hour;           // % of its 5-hour window used, 0..100; -1: not known
    int seven_day;           // % of its week used, 0..100; -1: not known
    time_t five_hour_resets; // when that window starts again, Unix seconds; 0: not known
    time_t seven_day_resets;
} usage_account_t;

void card_usage_create(lv_obj_t *parent);

// A row per window (5h, then the week) and a column per account, in the order given, headed by its name
// and plan; in each, the window's bar and %, and under them when it resets ("↻ 18:00", or the day once
// it's more than a day away). Just one account: its column across the card, its bars longer and its %
// bigger.
// A window's bar is green, orange from 70% and red from 90% (its % too), as in Talpy Den.
// Once a window's reset time has passed it shows 0% and no reset time, till the next reading.
// Accounts past n are hidden. Copies the texts.
// Call from the LVGL loop (loop()/serial handler), never from another task.
void card_usage_set(const usage_account_t *accounts, int n);

#ifdef __cplusplus
} /*extern "C"*/
#endif
