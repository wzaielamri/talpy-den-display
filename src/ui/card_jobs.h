#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

#define JOBS_ROWS 4

// The three states the screen can show (one colour each).
// Mapping Talpy Den's states (blocked, idle, stopped, failed, ...) onto these is up to the caller.
typedef enum { JOB_WORKING, JOB_WAITING, JOB_DONE } job_state_t;

void card_jobs_create(lv_obj_t *parent);

// Top-right caption: "updated <time>", e.g. card_jobs_set_updated("14:52:05").
void card_jobs_set_updated(const char *time);

// The three count tiles. The waiting tile gets an amber border while waiting > 0.
void card_jobs_set_counts(int working, int waiting, int done);

// Row i (0 .. JOBS_ROWS-1): coloured dot, name, and "<state> • <detail>" on the right,
// e.g. card_jobs_set_row(0, "desk-display-gadget", JOB_WAITING, "3m") -> "waiting • 3m".
// detail may be NULL or "" to show the state word alone. Shows the row if it was hidden.
void card_jobs_set_row(int i, const char *name, job_state_t state, const char *detail);

// Hide row i, for when there are fewer jobs than rows. Fill rows from the top.
void card_jobs_hide_row(int i);

// All setters: call from the LVGL loop (loop()/serial handler), never from another task.

#ifdef __cplusplus
} /*extern "C"*/
#endif
