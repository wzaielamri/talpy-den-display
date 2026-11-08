#include "serial_link.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include "app/clock.h"
#include "ui/card_weather.h"

static void handle_time(JsonDocument &doc) {
    if (doc["epoch"].is<long long>()) {
        long long epoch = doc["epoch"].as<long long>();
        const char *tz = doc["tz"] | "";
        clock_sync(epoch, tz);
        Serial.printf("clock_sync(%lld, \"%s\")\n", epoch, tz);
    }
    else {
        Serial.println("bad line: no epoch");
    }
}

static void handle_weather(JsonDocument &doc) {
    // Without these two the card would show "0°" or a made-up icon as if they were real.
    if (!doc["temp"].is<int>() || !doc["code"].is<int>()) {
        Serial.println("bad weather: needs temp and code");
        return;
    }
    weather_update_t w = {
        .temp = doc["temp"], .hi = doc["hi"] | 0, .lo = doc["lo"] | 0,
        .code = doc["code"], .is_day = doc["day"] | true,
        .updated = doc["updated"].as<const char *>(),
    };
    card_weather_set(&w);
    Serial.printf("card_weather_set(temp %d, hi %d, lo %d, code %d, day %d, \"%s\")\n",
            w.temp, w.hi, w.lo, w.code, w.is_day, w.updated ? w.updated : "(none)");
}

static void handle_line(const char *line) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, line);
    if (err) {
        Serial.printf("bad line: %s\n", err.c_str());
        return;
    }
    const char *type = doc["type"] | "";
    if      (strcmp(type, "time") == 0)    handle_time(doc);
    else if (strcmp(type, "weather") == 0) handle_weather(doc);
    //else if (strcmp(type, "jobs") == 0)    handle_jobs(doc);
    //else if (strcmp(type, "notify") == 0)  handle_notify(doc);
    else Serial.printf("unknown type: \"%s\"\n", type);
}

void serial_poll(void) {
    static char line[512];
    static size_t len = 0;
    while (Serial.available() > 0) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            line[len] = 0;
            if (len > 0) {
                Serial.printf("Serial command: %s\n", line);
                len = 0;
                handle_line(line);
            }
        } else if (len < sizeof(line) - 1) {
            if (c >= 0x20 && c < 0x7F) // Only accept printable ASCII characters
            {
                line[len++] = c;
            }
        }
    }
}
