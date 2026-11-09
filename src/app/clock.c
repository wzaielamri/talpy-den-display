#include "clock.h"
#include "app/settings.h"
#include "drivers/rtc.h"
#include "lvgl.h"
#include "ui/header_clock.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

static char zone[64]; // the POSIX time zone in use: the PC's last (kept across restarts), UTC till it has sent one

void clock_init(){
    time_t now;
    if (rtc_read(&now)) { // (first: it reads the RTC as UTC, while no zone is set)
        printf("RTC: %lld\n", (long long)now);
        struct timeval tv = {};
        tv.tv_sec = now;
        settimeofday(&tv, NULL);
    } else {
        printf("RTC: not set\n");
    }
    settings_tz(zone, sizeof zone);
    setenv("TZ", zone, 1);
    tzset();

}

void clock_sync(long long epoch, const char *tz) {
    struct timeval tv = {0};
    tv.tv_sec = epoch;
    settimeofday(&tv, NULL);
    rtc_write((time_t)epoch);
    if (tz && tz[0] && strlen(tz) < sizeof zone && strcmp(tz, zone) != 0) { // (a new zone: kept for the next start)
        strcpy(zone, tz);
        setenv("TZ", zone, 1);
        tzset();
        settings_save_tz(zone);
    }
}

static void clock_tick(lv_timer_t *timer) {
    LV_UNUSED(timer);
    static char shown_time[6], shown_date[32];
    char time_str[6];
    char date_str[32];
    char weekday[12], month[12];
    time_t now = time(NULL);
    if (now < CLOCK_SET) { // clock never set: the labels keep their start text "--:--"
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
    header_clock_set_time(time_str, date_str);
}

void clock_start(void) {
    lv_timer_create(clock_tick, 1000, NULL);
    clock_tick(NULL);
}