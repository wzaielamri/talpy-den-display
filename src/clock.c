#include "clock.h"
#include "lvgl.h"
#include "rtc.h"
#include "screen_main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

void clock_sync(long long epoch, const char *tz) {
    struct timeval tv = {0};
    tv.tv_sec = epoch;
    settimeofday(&tv, NULL);

    rtc_write((time_t)epoch);

    if (tz && tz[0]) {
        setenv("TZ", tz, 1);
        tzset();
    }
}

static void clock_tick(lv_timer_t *timer) {
    LV_UNUSED(timer);
    static char shown_time[6], shown_date[32];
    char time_str[6];
    char date_str[32];
    char weekday[12], month[12];
    time_t now = time(NULL);
    if (now < 1700000000) { // clock never set: the labels keep their start text "--:--"
        return;
    }
    struct tm tm;
    localtime_r(&now, &tm);

    strftime(time_str, sizeof time_str, "%H:%M", &tm);
    strftime(weekday, sizeof weekday, "%A", &tm);
    strftime(month, sizeof month, "%B", &tm);
    snprintf(date_str, sizeof date_str, "%s, %d %s", weekday, tm.tm_mday, month);

    if (strcmp(time_str, shown_time) == 0 && strcmp(date_str, shown_date) == 0)
        return;
    strcpy(shown_time, time_str);
    strcpy(shown_date, date_str);
    ui_set_time(time_str, date_str);
}

void clock_start(void) {
    lv_timer_create(clock_tick, 1000, NULL);
    clock_tick(NULL);
}