#include "job_watch.h"
#include "app/screen_power.h"
#include "ui/alert.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#define SEEN_MAX 64 // jobs remembered from one message to the next (sync.py's MAX_JOBS); more are left out: no alerts
#define ID_MAX   96
#define TEXT_MAX 48 // a name or machine kept for an alert owed till the unlock (sync.py cuts names at 40)

typedef struct {
    char id[ID_MAX];
    job_state_t state;
    time_t since;
    char name[TEXT_MAX];
    char machine[TEXT_MAX];
    bool owed; // waits, and its waiting alert wasn't seen: the lock hid it, or it came while locked
} seen_t;

static seen_t seen[SEEN_MAX];
static int seen_n;
static bool started;             // a first message came already (before: no "finished" alerts)
static char alert_id[ID_MAX];    // the job of the alert job_watch showed last
static alert_kind_t alert_kind;
static bool locked; // the laptop is locked: no alert (they show a chat's name); owed ones show on unlock

// The job's id, or name and machine standing in for it.
static void job_key(const job_info_t *j, char *out, size_t size) {
    if (j->id && j->id[0])
        snprintf(out, size, "%s", j->id);
    else
        snprintf(out, size, "%s@%s", j->name ? j->name : "", j->machine ? j->machine : "");
}

static seen_t *find(const char *id) {
    for (int i = 0; i < seen_n; i++)
        if (strcmp(seen[i].id, id) == 0) return &seen[i];
    return NULL;
}

// The line under the name: where it runs and since when, "lab (ssh) • 14:52".
static void detail_of(const job_info_t *j, char *out, size_t size) {
    char when[8] = "";
    if (j->since > 0) {
        struct tm tm;
        localtime_r(&j->since, &tm);
        strftime(when, sizeof when, "%H:%M", &tm);
    }
    bool has_machine = j->machine && j->machine[0];
    snprintf(out, size, "%s%s%s", has_machine ? j->machine : "", has_machine && when[0] ? " \xE2\x80\xA2 " : "", when);
}

static void show(alert_kind_t kind, const job_info_t *j, const char *id) {
    char detail[96];
    detail_of(j, detail, sizeof detail);
    if (!alert_show(kind, "", j->name, detail)) return; // (a finished one while a waiting one shows)
    snprintf(alert_id, sizeof alert_id, "%s", id);
    alert_kind = kind;
    screen_power_wake(); // a dark screen mustn't hide it
}

void job_watch_update(const job_info_t *jobs, int n) {
    const job_info_t *now_waiting = NULL, *now_done = NULL;
    char waiting_id[ID_MAX] = "", done_id[ID_MAX] = "";
    bool alerted_still_waits = false;
    bool owed[SEEN_MAX] = {false};
    char id[ID_MAX];

    if (!jobs) n = 0;
    if (n > SEEN_MAX) n = SEEN_MAX; // (one past what's remembered would look new in every message)
    for (int i = 0; i < n; i++) {
        const job_info_t *j = &jobs[i];
        job_key(j, id, sizeof id);
        const seen_t *before = find(id);
        bool starts_waiting = j->state == JOB_WAITING && (!before || before->state != JOB_WAITING);
        // Owed: one the lock hid or that came while locked, as long as it still waits.
        owed[i] = j->state == JOB_WAITING && ((before && before->owed) || (locked && starts_waiting));
        if (starts_waiting && !now_waiting) {
            now_waiting = j; // started waiting (or new, or first seen): it needs you
            snprintf(waiting_id, sizeof waiting_id, "%s", id);
        }
        if (j->state == JOB_DONE && started && before && before->state != JOB_DONE && !now_done) {
            now_done = j; // finished since the last message
            snprintf(done_id, sizeof done_id, "%s", id);
        }
        if (j->state == JOB_WAITING && strcmp(id, alert_id) == 0) alerted_still_waits = true;
    }

    // The job of the waiting alert doesn't wait any more (answered, or gone): its alert goes.
    if (alert_is_showing() && alert_kind == ALERT_WAITING && !alerted_still_waits) alert_hide();

    // (locked: none now; a waiting one is owed, a finished one is dropped)
    if (!locked && now_waiting)
        show(ALERT_WAITING, now_waiting, waiting_id);
    else if (!locked && now_done)
        show(ALERT_DONE, now_done, done_id);

    // Remember this message's states for the next one (and, for an alert owed till the unlock, its texts).
    seen_n = 0;
    for (int i = 0; i < n && seen_n < SEEN_MAX; i++) {
        const job_info_t *j = &jobs[i];
        seen_t *s = &seen[seen_n++];
        job_key(j, s->id, ID_MAX);
        s->state = j->state;
        s->since = j->since;
        snprintf(s->name, TEXT_MAX, "%s", j->name ? j->name : "");
        snprintf(s->machine, TEXT_MAX, "%s", j->machine ? j->machine : "");
        s->owed = owed[i];
    }
    started = true;
}

void job_watch_set_locked(bool on) {
    if (on == locked) return;
    locked = on;
    if (on) {
        // A waiting alert the lock hides is still owed (you didn't close it with OK); any alert goes, as it shows
        // a chat's name.
        if (alert_is_showing() && alert_kind == ALERT_WAITING) {
            seen_t *s = find(alert_id);
            if (s && s->state == JOB_WAITING) s->owed = true;
        }
        alert_hide();
        return;
    }
    // Unlocked: the owed job asking longest (if several), its waiting alert now; none owed after that.
    seen_t *first = NULL;
    for (int i = 0; i < seen_n; i++) {
        if (seen[i].owed && seen[i].state == JOB_WAITING && (!first || seen[i].since < first->since))
            first = &seen[i];
        seen[i].owed = false;
    }
    if (first) {
        job_info_t j = {.name = first->name, .state = JOB_WAITING, .since = first->since,
                        .machine = first->machine, .id = first->id, .started = 0};
        show(ALERT_WAITING, &j, first->id);
    }
}
