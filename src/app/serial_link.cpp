#include "serial_link.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include "app/clock.h"
#include "ui/card_weather.h"
#include "ui/card_jobs.h"
#include "ui/card_usage.h"
#include "ui/card_hourly.h"
#include "app/job_watch.h"
#include "app/weather_state.h"
#include "ui/header_pc.h"
#include "ui/clock_face.h"
#include "app/screen_power.h"
#include "debug_log.h"

static void handle_time(JsonDocument &doc) {
    if (doc["epoch"].is<long long>()) {
        long long epoch = doc["epoch"].as<long long>();
        const char *tz = doc["tz"] | "";
        clock_sync(epoch, tz);
        debug_printf("clock_sync(%lld, \"%s\")\n", epoch, tz);
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
    weather_state_weather(&w);
    debug_printf("card_weather_set(temp %d, hi %d, lo %d, code %d, day %d, \"%s\")\n",
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

// {"type":"jobs","updated":"14:52:05","rows":[{"name":"...","state":"waiting","since":1794145062},...]}
// rows holds every job, not only 4: the card counts them all and picks what fits.
// since: when the job got into its state, Unix seconds as a whole number (Talpy Den's ms / 1000).
// started (optional): when the chat started, the same way (jobs.json's startedAt): the card's order
// (ui/job_order.h) puts the newest started on top after the waiting ones; without it, since stands in.
// machine (optional): where it runs, shown small under the name, e.g. "local" or "lab (ssh)".
// id: stable while the job is listed (jobs.json's "id"): the alerts come from a job's state changing
// from one jobs message to the next (job_watch.c), so no separate notify message is needed.
// The board counts "3m" from it with its own clock, so the PC only has to send when something changes.
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
        long long since = row["since"] | 0LL;     // missing or not a whole number: 0, not known
        long long started = row["started"] | 0LL; // (the same; its since stands in for the order then)
        jobs.push_back({name, state, (time_t)(since > 0 ? since : 0), row["machine"].as<const char *>(),
                        row["id"].as<const char *>(), (time_t)(started > 0 ? started : 0)});
    }

    job_update_t u = {
        .jobs = jobs.data(), .count = (int)jobs.size(),
        .updated = doc["updated"].as<const char *>(),
    };
    card_jobs_set(&u);
    int waiting, working, done;
    card_jobs_counts(&u, &waiting, &working, &done);
    clock_face_set_chats(true, waiting, working, done); // (the clock face's chat line)
    debug_printf("card_jobs_set(%d jobs)\n", u.count);
    // A row is made for each job (up to JOBS_MAX) and kept: what's left after a new most, to check 64 fit.
    static int most;
    if (u.count > most) {
        most = u.count;
        debug_printf("free heap %u (internal %u), largest block %u\n", (unsigned)ESP.getFreeHeap(),
                     (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL), (unsigned)ESP.getMaxAllocHeap());
    }

    // A job waiting for you: the cards stay. A working one doesn't hold them (one can work for hours, on
    // another machine too): the clock face's chat line shows how many work.
    screen_power_set_busy(waiting > 0);

    // A job that started waiting for you or finished: the alert.
    job_watch_update(jobs.data(), (int)jobs.size());
}

// A percentage as a whole number: 40, and also 40.0 or 39.6 as Talpy Den's jobs.json has them; -1 if missing.
static int percent(JsonVariantConst v) {
    return v.is<double>() ? (int)lround(v.as<double>()) : -1;
}

// Unix seconds as a whole number; 0 (not known) if missing.
static time_t seconds(JsonVariantConst v) {
    long long s = v | 0LL;
    return (time_t)(s > 0 ? s : 0);
}

// {"type":"usage","accounts":[{"name":"work","plan":"Team","five_hour":42,"seven_day":84,
//                              "five_hour_resets":1794158400,"seven_day_resets":1794304800},...]}
// Each account's plan usage: the % of its 5-hour window and of its week, and when each starts again
// (jobs.json's "accounts": their windows "five_hour" and "seven_day", "pct" and "resetsAt" / 1000).
// One missing shows "--" (a reset: nothing). The card shows the first USAGE_ROWS.
static void handle_usage(JsonDocument &doc) {
    if (!doc["accounts"].is<JsonArray>()) { Serial.println("bad usage: needs accounts"); return; }
    std::vector<usage_account_t> accounts;
    for (JsonObject a : doc["accounts"].as<JsonArray>()) {
        const char *name = a["name"].as<const char *>();
        if (!name || !name[0]) {
            Serial.println("bad account: needs a name");
            continue;
        }
        accounts.push_back({name, a["plan"].as<const char *>(), percent(a["five_hour"]), percent(a["seven_day"]),
                            seconds(a["five_hour_resets"]), seconds(a["seven_day_resets"])});
    }
    card_usage_set(accounts.data(), (int)accounts.size());
    debug_printf("card_usage_set(%d accounts)\n", (int)accounts.size());
}

// {"type":"hourly","updated":"14:45","summary":"Rain likely from 16:00 to 18:00",
//  "hours":[{"at":1794150000,"temp":12,"code":2,"day":true,"rain":10},...]}
// The next hours from now (Open-Meteo hourly: temperature_2m, weather_code, is_day,
// precipitation_probability); at: the hour's start in Unix seconds. 12.4 is fine for temp (rounded).
static void handle_hourly(JsonDocument &doc) {
    if (!doc["hours"].is<JsonArray>()) { Serial.println("bad hourly: needs hours"); return; }
    std::vector<hour_t> hours;
    for (JsonObject h : doc["hours"].as<JsonArray>()) {
        if (hours.size() == HOURLY_COLS) break;
        if (!h["temp"].is<double>() || !h["code"].is<int>()) {
            Serial.println("bad hour: needs temp and code");
            continue;
        }
        hours.push_back({seconds(h["at"]), (int)lround(h["temp"].as<double>()), h["code"].as<int>(),
                         h["day"] | true, percent(h["rain"])});
    }
    hourly_update_t u = {hours.data(), (int)hours.size(), doc["summary"].as<const char *>(),
                         doc["updated"].as<const char *>()};
    card_hourly_set(&u);
    weather_state_hourly(&u);
    debug_printf("card_hourly_set(%d hours)\n", u.count);
}

// ---------- is the PC there? ----------

// No whole message for this long: the PC is away (suspended, shut down, unplugged, or sync.py not
// running): "PC offline", and the board goes dark and naps. sync.py sends something at least every
// 10 s (a {"type":"ping"} does) to stay there; 60 s leaves room for a slow check or a restart.
#define PC_AWAY_MS 60000

static bool pc_online = true; // at start: the PC is assumed there (it powers the board) till it's quiet
static uint32_t last_seen;    // millis() of the last whole message

static void set_pc_online(bool online) {
    if (online == pc_online) return;
    pc_online = online;
    header_pc_set_online(online);
    clock_face_set_pc(online);
    weather_state_pc(online);
    screen_power_pc_present(online);
    Serial.printf("PC %s\n", online ? "back" : "away");
}

// {"type":"power","state":"sleep"} when the PC is about to suspend or shut down: dark and napping at
// once, not PC_AWAY_MS later. Its next message (any) brings the board back.
static void handle_power(JsonDocument &doc) {
    const char *state = doc["state"] | "";
    if (strcmp(state, "sleep") == 0) set_pc_online(false);
    else if (strcmp(state, "wake") != 0) Serial.println("bad power: state sleep or wake");
}

// {"type":"lock","locked":true} when the laptop locks, false when it unlocks (sync.py, from logind; and in what
// it sends at its start). Locked: the clock face, no alert, nothing that shows a chat's name; unlocked: the
// cards, then the waiting alert you hadn't seen (job_watch.c). The board starts unlocked.
static void handle_lock(JsonDocument &doc) {
    if (!doc["locked"].is<bool>()) { Serial.println("bad lock: needs locked"); return; }
    bool locked = doc["locked"].as<bool>();
    if (locked) {
        job_watch_set_locked(true); // (first: a waiting alert showing is owed, then it goes)
        screen_power_set_locked(true);
    } else {
        screen_power_set_locked(false); // (first the cards, then the owed alert over them)
        job_watch_set_locked(false);
    }
    debug_printf("lock(%d)\n", locked);
}

static void handle_line(const char *line) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, line);
    if (err) {
        Serial.printf("bad line: %s\n", err.c_str()); // (one cut by the board waking up, too)
        return;
    }
    const char *type = doc["type"] | "";
    last_seen = millis();
    if (strcmp(type, "power") != 0) { // a whole message: the PC is there
        static bool heard;            // (the header starts "PC offline": its first message says connected)
        if (!heard) {
            heard = true;
            header_pc_set_online(true);
            clock_face_set_pc(true);
            weather_state_pc(true);
        }
        set_pc_online(true);
    }
    if      (strcmp(type, "power") == 0)   handle_power(doc);
    else if (strcmp(type, "ping") == 0)    {} // only says the PC is there
    else if (strcmp(type, "time") == 0)    handle_time(doc);
    else if (strcmp(type, "weather") == 0) handle_weather(doc);
    else if (strcmp(type, "jobs") == 0)    handle_jobs(doc);
    else if (strcmp(type, "usage") == 0)   handle_usage(doc);
    else if (strcmp(type, "hourly") == 0)  handle_hourly(doc);
    else if (strcmp(type, "lock") == 0)    handle_lock(doc);
    else Serial.printf("unknown type: \"%s\"\n", type);
}

// A jobs message is 100-180 characters per job. sync.py keeps one under 8000 characters and 64 jobs (MAX_LINE and
// MAX_JOBS there); a longer line is dropped whole.
#define SERIAL_LINE_MAX 8192 // (not LINE_MAX: <limits.h> has one of that name)

void serial_poll(void) {
    static char line[SERIAL_LINE_MAX];
    static size_t len = 0;
    static bool too_long = false; // this line didn't fit: drop all of it, don't parse half
    if (pc_online && millis() - last_seen > PC_AWAY_MS) set_pc_online(false);
    while (Serial.available() > 0) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (too_long) {
                Serial.println("bad line: too long, dropped");
            } else if (len > 0) {
                line[len] = 0;
                // Echo only the start: at 115200 baud a 2 KB echo blocks loop() for ~0.2 s. (DESK_DEBUG only:
                // with a ping every 10 s, a line each in sync.py's log.)
                debug_printf("Serial command: %.80s%s\n", line, len > 80 ? "..." : "");
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
