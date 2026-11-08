#include "serial_link.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include "app/clock.h"
#include "ui/card_weather.h"
#include "ui/card_jobs.h"

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

// "working" / "waiting" / "done" -> job_state_t. Returns false for anything else.
static bool parse_state(const char *s, job_state_t *out) {
    if (!s) return false;
    if (strcmp(s, "working") == 0) { *out = JOB_WORKING; return true; }
    if (strcmp(s, "waiting") == 0) { *out = JOB_WAITING; return true; }
    if (strcmp(s, "done") == 0)    { *out = JOB_DONE;    return true; }
    return false;
}

// {"type":"jobs","updated":"14:52:05","rows":[{"name":"...","state":"waiting","detail":"3m"},...]}
// rows holds every job, not only 4: the card counts them all and picks what fits.
// An empty list [] means no jobs.
static void handle_jobs(JsonDocument &doc) {
    if (!doc["rows"].is<JsonArray>()) { Serial.println("bad jobs: needs rows"); return; }

    // The strings stay inside doc, which lives until handle_line returns. card_jobs_set copies them.
    std::vector<job_info_t> jobs;
    for (JsonObject row : doc["rows"].as<JsonArray>()) {
        const char *name = row["name"].as<const char *>();
        job_state_t state;
        if (!name || !name[0] || !parse_state(row["state"].as<const char *>(), &state)) {
            Serial.println("bad job: needs a name and state working, waiting or done");
            continue; // skip this one, keep the others
        }
        jobs.push_back({name, state, row["detail"].as<const char *>()});
    }

    job_update_t u = {
        .jobs = jobs.data(), .count = (int)jobs.size(),
        .updated = doc["updated"].as<const char *>(),
    };
    card_jobs_set(&u);
    Serial.printf("card_jobs_set(%d jobs)\n", u.count);
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
    else if (strcmp(type, "jobs") == 0)    handle_jobs(doc);
    //else if (strcmp(type, "notify") == 0)  handle_notify(doc);
    else Serial.printf("unknown type: \"%s\"\n", type);
}

// A jobs message is about 60 characters per job, so 4096 holds 60+ jobs.
#define LINE_MAX 4096

void serial_poll(void) {
    static char line[LINE_MAX];
    static size_t len = 0;
    static bool too_long = false; // this line didn't fit: drop all of it, don't parse half
    while (Serial.available() > 0) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (too_long) {
                Serial.println("bad line: too long, dropped");
            } else if (len > 0) {
                line[len] = 0;
                // Echo only the start: at 115200 baud a 2 KB echo blocks loop() for ~0.2 s.
                Serial.printf("Serial command: %.80s%s\n", line, len > 80 ? "..." : "");
                handle_line(line);
            }
            len = 0;
            too_long = false;
        } else if (c >= 0x20 && c < 0x7F) { // only printable ASCII
            if (len < sizeof(line) - 1)
                line[len++] = c;
            else
                too_long = true;
        }
    }
}
