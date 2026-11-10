#include "rtc.h"

#include <Arduino.h>
#include <Wire.h>
#include "debug_log.h"

#define RTC_ADDR 0x51

static uint8_t bcd_to_dec(uint8_t b) { return (b >> 4) * 10 + (b & 0x0F); }
static uint8_t dec_to_bcd(uint8_t d) { return ((d / 10) << 4) | (d % 10); }

// only valid while TZ is UTC
bool rtc_read(time_t *utc) {
    uint8_t err = 0;
    for (int attempt = 0; attempt < 2; ++attempt) {
        Wire.beginTransmission(RTC_ADDR);
        Wire.write(0x02);
        err = Wire.endTransmission(false);
        if (err == 0)
            break;
    }
    if (err != 0) {
        Serial.printf("RTC: no answer (error %u)\n", err);
        return false;
    }
    uint8_t r[7];
    int n = Wire.requestFrom(RTC_ADDR, 7);
    if (n != 7) {
        Serial.printf("RTC read error: expected 7 bytes, got %d\n", n);
        return false;
    }
    for (int i = 0; i < 7; ++i) {
        r[i] = Wire.read();
        debug_printf("%02X ", r[i]);
    }
    debug_printf("\n");
    if (r[0] & 0x80) {
        Serial.println("the time can't be trusted");
        return false;
    }
    struct tm t = {0};
    t.tm_sec = bcd_to_dec(r[0] & 0x7F);
    t.tm_min = bcd_to_dec(r[1] & 0x7F);
    t.tm_hour = bcd_to_dec(r[2] & 0x3F);
    t.tm_mday = bcd_to_dec(r[3] & 0x3F);
    t.tm_mon = bcd_to_dec(r[5] & 0x1F) - 1;
    t.tm_year = bcd_to_dec(r[6]) + 100;
    t.tm_isdst = 0;
    *utc = mktime(&t);
    return true;
}

void rtc_write(time_t utc) {
    struct tm t;
    gmtime_r(&utc, &t);
    uint8_t err = 0;
    for (int attempt = 0; attempt < 2; ++attempt) {
        Wire.beginTransmission(RTC_ADDR);
        Wire.write(0x02);
        Wire.write(dec_to_bcd(t.tm_sec));
        Wire.write(dec_to_bcd(t.tm_min));
        Wire.write(dec_to_bcd(t.tm_hour));
        Wire.write(dec_to_bcd(t.tm_mday));
        Wire.write(dec_to_bcd(t.tm_wday));
        Wire.write(dec_to_bcd(t.tm_mon + 1));
        Wire.write(dec_to_bcd(t.tm_year - 100));
        err = Wire.endTransmission();
        if (err == 0)
            break;
    }
    if (err != 0) {
        Serial.printf("RTC write error: %u\n", err);
    }
}