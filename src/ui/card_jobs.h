#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "ui/job_info.h"
#include <time.h>

// Rows at most: every job the PC sends gets one, in a list that scrolls (about 4 1/2 show). The PC sends
// at most this many (sync.py's MAX_JOBS), and job_watch.c follows as many (SEEN_MAX).
#define JOBS_MAX 64

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
// - the rows show the jobs in job_order's order (ui/job_order.h): waiting first, then the newest started,
//   each job a row in the list (up to JOBS_MAX), which keeps where it was scrolled; a job that starts waiting
//   takes the list back to its top;
// - each row says how long, from since and the board's clock, and keeps counting by itself:
//   "waiting • 3m", "working • 2h", "done • 5m ago" (only the state word while either isn't known).
// The card copies the texts, so u and its strings may be freed after the call.
// Call from the LVGL loop (loop()/serial handler), never from another task.
void card_jobs_set(const job_update_t *u);

// A tap on a job's row (one with an id): cb gets the id of the row the finger landed on (main.cpp asks the
// PC to bring it up, app/pc_open.h). A swipe opens nothing.
typedef void (*card_jobs_open_cb_t)(const char *id);
void card_jobs_on_open(card_jobs_open_cb_t cb);

// The row of the job with this id says "Opening..." in place of its state, till card_jobs_open_result.
void card_jobs_opening(const char *id);

// Then "Opened" (green) or message (muted, dotted to the row) for 4 s, then its state again. Shown on the row
// that has this id at the time: on none if the job is gone.
void card_jobs_open_result(const char *id, bool ok, const char *message);

// The list back to its top (the jobs that need you): also by itself 30 s after the last touch on it, and
// when the clock face shows (screen_power.c), so the cards always come back at the top.
void card_jobs_scroll_top(void);

// How many of u's jobs wait for you, work and are done (what the banner and the clock face's chat line count).
void card_jobs_counts(const job_update_t *u, int *waiting, int *working, int *done);

#ifdef __cplusplus
} /*extern "C"*/
#endif
