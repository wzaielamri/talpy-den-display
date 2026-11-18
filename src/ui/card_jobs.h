#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "ui/job_info.h"
#include <time.h>

// Rows on the screen. With more jobs than this, a "+N more in Talpy Den" line shows under them.
#define JOBS_ROWS 4

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
// - the rows show the jobs in job_order's order (ui/job_order.h): waiting first, then the newest started;
// - jobs that don't fit are counted in "+N more in Talpy Den" under the rows;
// - each row says how long, from since and the board's clock, and keeps counting by itself:
//   "waiting • 3m", "working • 2h", "done • 5m ago" (only the state word while either isn't known).
// The card copies the texts, so u and its strings may be freed after the call.
// Call from the LVGL loop (loop()/serial handler), never from another task.
void card_jobs_set(const job_update_t *u);

// How many of u's jobs wait for you, work and are done (what the banner and the clock face's chat line count).
void card_jobs_counts(const job_update_t *u, int *waiting, int *working, int *done);

#ifdef __cplusplus
} /*extern "C"*/
#endif
