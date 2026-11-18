#include "check.h"
#include "app/job_watch.h"
#include "app/screen_power.h"
#include "ui/alert.h"
#include <stdio.h>
#include <string.h>

// Fakes for what job_watch.c calls: the alert remembers what it was asked to show.
static int shows, hides;
static bool showing;
static alert_kind_t shown_kind;
static char shown_name[64];

bool alert_show(alert_kind_t kind, const char *kicker, const char *name, const char *detail) {
    (void)kicker;
    (void)detail;
    if (showing && shown_kind == ALERT_WAITING && kind == ALERT_DONE) return false;
    shows++;
    showing = true;
    shown_kind = kind;
    snprintf(shown_name, sizeof shown_name, "%s", name ? name : "");
    return true;
}
void alert_hide(void) {
    hides++;
    showing = false;
}
bool alert_is_showing(void) { return showing; }
void screen_power_wake(void) {}

// The alert's "Open on PC" (what job_watch_init registers) and what it asks the PC for.
static alert_open_cb_t open_cb;
static char opened[256];
static int requests;
static bool pc_free = true; // (false: an open under way, so the PC is asked nothing)
static bool open_shown = true;
void alert_on_open(alert_open_cb_t cb) { open_cb = cb; }
void alert_show_open(bool on) { open_shown = on; }
bool pc_open_request(const char *id) {
    requests++;
    snprintf(opened, sizeof opened, "%s", id ? id : "");
    return pc_free;
}

static void test_open_hidden_without_a_usable_id(void) {
    char long_id[201];
    memset(long_id, 'a', 200);
    long_id[200] = 0;
    memcpy(long_id + 190, "/x/live", 8); // (a container on a host with a long name: more than the board keeps)
    job_info_t no_id = {.name = "named", .state = JOB_WAITING, .since = 100, .machine = "local", .id = "", .started = 0};
    job_info_t too_long = {.name = "long", .state = JOB_WAITING, .since = 100, .machine = "x", .id = long_id, .started = 0};
    job_info_t fine = {.name = "fine", .state = JOB_WAITING, .since = 100, .machine = "x", .id = "lab:.c/a/live", .started = 0};
    const job_info_t *cases[] = {&no_id, &too_long};
    for (int k = 0; k < 2; k++) {
        job_watch_set_locked(false);
        job_watch_update(NULL, 0);
        showing = false;
        requests = 0;
        job_watch_update(cases[k], 1);
        CHECK(showing && !open_shown);
        CHECK(open_cb && !open_cb() && requests == 0); // (nothing cut short or made up goes to the PC)
    }
    job_watch_update(NULL, 0);
    showing = false;
    job_watch_update(&fine, 1);
    CHECK(showing && open_shown);
}

static void test_open_refused_keeps_the_alert(void) {
    job_watch_update(NULL, 0);
    showing = false;
    job_info_t waits = {.name = "w", .state = JOB_WAITING, .since = 100, .machine = "x", .id = "lab:.c/w/live", .started = 0};
    job_watch_update(&waits, 1);
    pc_free = false;
    CHECK(open_cb && !open_cb()); // (the alert stays: alert.c hides it only when the PC was asked)
    pc_free = true;
    CHECK(open_cb && open_cb());
}

static job_info_t J(const char *id, job_state_t state, time_t since) {
    job_info_t j = {.name = id, .state = state, .since = since, .machine = "local", .id = id, .started = 0};
    return j;
}

// A fresh start for each test: unlocked, no jobs known, no alert.
static void reset(void) {
    job_watch_set_locked(false);
    job_watch_update(NULL, 0);
    shows = hides = 0;
    showing = false;
    shown_name[0] = 0;
}

static void test_alert_hidden_by_lock_comes_back(void) {
    reset();
    job_info_t a[] = {J("A", JOB_WAITING, 100)};
    job_watch_update(a, 1);
    CHECK(shows == 1 && strcmp(shown_name, "A") == 0);
    job_watch_set_locked(true);
    CHECK(hides >= 1 && !showing);
    job_watch_set_locked(false);
    CHECK(shows == 2 && showing && shown_kind == ALERT_WAITING && strcmp(shown_name, "A") == 0);
}

static void test_waiting_while_locked_shows_on_unlock(void) {
    reset();
    job_info_t working[] = {J("A", JOB_WORKING, 50)};
    job_watch_update(working, 1);
    job_watch_set_locked(true);
    job_info_t waits[] = {J("A", JOB_WAITING, 100)};
    job_watch_update(waits, 1);
    CHECK(shows == 0);
    job_watch_set_locked(false);
    CHECK(shows == 1 && strcmp(shown_name, "A") == 0);
}

static void test_answered_while_locked_not_shown(void) {
    reset();
    job_info_t working[] = {J("A", JOB_WORKING, 50)};
    job_watch_update(working, 1);
    job_watch_set_locked(true);
    job_info_t waits[] = {J("A", JOB_WAITING, 100)};
    job_watch_update(waits, 1);
    job_info_t again[] = {J("A", JOB_WORKING, 150)};
    job_watch_update(again, 1);
    job_watch_set_locked(false);
    CHECK(shows == 0);
}

static void test_hidden_alert_answered_while_locked(void) {
    reset();
    job_info_t waits[] = {J("A", JOB_WAITING, 100)};
    job_watch_update(waits, 1);
    job_watch_set_locked(true);
    job_info_t working[] = {J("A", JOB_WORKING, 150)};
    job_watch_update(working, 1);
    job_watch_set_locked(false);
    CHECK(shows == 1); // (only the one before the lock)
}

static void test_done_while_locked_dropped(void) {
    reset();
    job_info_t working[] = {J("A", JOB_WORKING, 50)};
    job_watch_update(working, 1);
    job_watch_set_locked(true);
    job_info_t done[] = {J("A", JOB_DONE, 100)};
    job_watch_update(done, 1);
    job_watch_set_locked(false);
    CHECK(shows == 0);
}

static void test_longest_asking_first(void) {
    reset();
    job_info_t working[] = {J("A", JOB_WORKING, 50), J("B", JOB_WORKING, 50)};
    job_watch_update(working, 2);
    job_watch_set_locked(true);
    job_info_t waits[] = {J("B", JOB_WAITING, 200), J("A", JOB_WAITING, 100)};
    job_watch_update(waits, 2);
    job_watch_set_locked(false);
    CHECK(shows == 1 && strcmp(shown_name, "A") == 0);
}

static void test_closed_with_ok_not_owed(void) {
    reset();
    job_info_t waits[] = {J("A", JOB_WAITING, 100)};
    job_watch_update(waits, 1);
    CHECK(shows == 1);
    showing = false; // OK tapped (the alert closes itself, telling no one)
    job_watch_set_locked(true);
    job_watch_set_locked(false);
    CHECK(shows == 1);
}

static void test_unlocked_alerts_as_before(void) {
    reset();
    job_info_t working[] = {J("A", JOB_WORKING, 50)};
    job_watch_update(working, 1);
    job_info_t waits[] = {J("A", JOB_WAITING, 100)};
    job_watch_update(waits, 1);
    CHECK(shows == 1 && shown_kind == ALERT_WAITING);
    job_info_t done[] = {J("A", JOB_DONE, 150)};
    job_watch_update(done, 1);
    CHECK(!showing || shown_kind == ALERT_DONE);
}

static void test_open_on_pc_opens_the_alerts_job(void) {
    reset();
    job_watch_init();
    CHECK(open_cb != NULL);
    job_info_t waits[] = {J("A", JOB_WORKING, 50), J("lab:.claude/b/live", JOB_WAITING, 100)};
    job_watch_update(waits, 2);
    CHECK(shows == 1);
    opened[0] = 0;
    if (open_cb) open_cb();
    CHECK(strcmp(opened, "lab:.claude/b/live") == 0);
}

int main(void) {
    test_open_on_pc_opens_the_alerts_job();
    test_open_hidden_without_a_usable_id();
    test_open_refused_keeps_the_alert();
    test_alert_hidden_by_lock_comes_back();
    test_waiting_while_locked_shows_on_unlock();
    test_answered_while_locked_not_shown();
    test_hidden_alert_answered_while_locked();
    test_done_while_locked_dropped();
    test_longest_asking_first();
    test_closed_with_ok_not_owed();
    test_unlocked_alerts_as_before();
    return check_failures() ? 1 : 0;
}
