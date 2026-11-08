#include "job_watch.h"
#include "app/screen_power.h"
#include "ui/alert.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#define SEEN_MAX 48 // jobs remembered from one message to the next (more are left out: no alerts)
#define ID_MAX   96

typedef struct {
    char id[ID_MAX];
    job_state_t state;
} seen_t;

static seen_t seen[SEEN_MAX];
static int seen_n;
static bool started;             // a first message came already (before: no "finished" alerts)
static char alert_id[ID_MAX];    // the job of the alert job_watch showed last
static alert_kind_t alert_kind;

// The job's id, or name and machine standing in for it.
static void job_key(const job_info_t *j, char *out, size_t size) {
    if (j->id && j->id[0])
        snprintf(out, size, "%s", j->id);
    else
        snprintf(out, size, "%s@%s", j->name ? j->name : "", j->machine ? j->machine : "");
}

static const seen_t *find(const char *id) {
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
    char id[ID_MAX];

    for (int i = 0; i < n; i++) {
        const job_info_t *j = &jobs[i];
        job_key(j, id, sizeof id);
        const seen_t *before = find(id);
        if (j->state == JOB_WAITING && (!before || before->state != JOB_WAITING) && !now_waiting) {
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

    if (now_waiting)
        show(ALERT_WAITING, now_waiting, waiting_id);
    else if (now_done)
        show(ALERT_DONE, now_done, done_id);

    // Remember this message's states for the next one.
    seen_n = 0;
    for (int i = 0; i < n && seen_n < SEEN_MAX; i++) {
        job_key(&jobs[i], seen[seen_n].id, ID_MAX);
        seen[seen_n].state = jobs[i].state;
        seen_n++;
    }
    started = true;
}
