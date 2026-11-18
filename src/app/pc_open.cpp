#include "app/pc_open.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include "app/screen_power.h"
#include "lvgl.h"
#include "ui/card_jobs.h"

#define OPEN_WAIT_MS 20000 // the PC's answer at the latest (sync.py gives jump.py 15 s)
#define ID_MAX       96    // as job_watch.c and card_jobs.c keep an id

static char pending[ID_MAX]; // the id under way; "" while none
static lv_timer_t *wait_timer;

static void give_up(lv_timer_t *timer) {
    LV_UNUSED(timer);
    wait_timer = NULL; // (a one-shot timer: LVGL deletes it after this)
    if (!pending[0]) return;
    char id[ID_MAX];
    snprintf(id, sizeof id, "%s", pending);
    pending[0] = 0; // (free for the next tap; a late answer for it is ignored)
    card_jobs_open_result(id, false, "No answer from the PC");
}

bool pc_open_request(const char *id) {
    if (!id || !id[0] || strlen(id) >= ID_MAX || pending[0] || screen_power_locked()) return false;
    // ArduinoJson writes it, so an id with a quote or a backslash still makes one valid JSON line.
    JsonDocument doc;
    doc["type"] = "open";
    doc["id"] = id;
    serializeJson(doc, Serial);
    Serial.print('\n');
    snprintf(pending, sizeof pending, "%s", id);
    card_jobs_opening(id);
    wait_timer = lv_timer_create(give_up, OPEN_WAIT_MS, NULL);
    lv_timer_set_repeat_count(wait_timer, 1);
    return true;
}

void pc_open_answer(const char *id, bool ok, const char *message) {
    if (!id || !pending[0] || strcmp(id, pending) != 0) return;
    pending[0] = 0;
    if (wait_timer) {
        lv_timer_delete(wait_timer);
        wait_timer = NULL;
    }
    card_jobs_open_result(id, ok, ok ? "Opened" : message && message[0] ? message : "Couldn't open it");
}
