#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include <time.h>

// Rows on the screen. With more jobs than this, a "+N more in Talpy Den" line shows under them.
#define JOBS_ROWS 4

// The three states the screen can show (one colour each).
// Mapping Talpy Den's states (blocked, idle, stopped, failed, ...) onto these is up to the caller.
typedef enum { JOB_WORKING, JOB_WAITING, JOB_DONE } job_state_t;

// One job, as the PC sends it.
typedef struct {
    const char *name;
    job_state_t state;
    time_t since;         // when it got into this state, Unix seconds (UTC); 0: not known
    const char *machine;  // where it runs, a small line under the name ("local", "lab (ssh)");
                          // NULL or "": the name alone, on one line
    const char *id;       // stable while the job is listed (jobs.json's "id"): job_watch.c tells a job's
                          // state change by it; NULL or "": name and machine stand in
} job_info_t;

// Everything the jobs card shows, in one message.
typedef struct {
    const job_info_t *jobs; // every job, in any order (not only the ones that fit)
    int count;              // how many entries jobs has
    const char *updated;    // time of the PC's message, "14:52:05"; NULL shows "--:--:--"
} job_update_t;

void card_jobs_create(lv_obj_t *parent);

// Freeze Talpy in the banner (true; the current picture stays) or let him go on (false): the pull-down panel
// and the clock face hold it still while they cover the card, so nothing changes under their
// dimming (a change there showed through undimmed).
void card_jobs_freeze(bool on);

// Fill the whole card from the full job list:
// - the banner (headline, counts, Talpy's mood) counts every job;
// - the rows show waiting jobs first (they need you), then working, then done,
//   keeping the PC's order within each state;
// - jobs that don't fit are counted in "+N more in Talpy Den" under the rows;
// - each row says how long, from since and the board's clock, and keeps counting by itself:
//   "waiting • 3m", "working • 2h", "done • 5m ago" (only the state word while either isn't known).
// The card copies the texts, so u and its strings may be freed after the call.
// Call from the LVGL loop (loop()/serial handler), never from another task.
void card_jobs_set(const job_update_t *u);

#ifdef __cplusplus
} /*extern "C"*/
#endif
