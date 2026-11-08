#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

// Rows on the screen. With more jobs than this, a "+N more in Talpy Den" line shows under them.
#define JOBS_ROWS 4

// The three states the screen can show (one colour each).
// Mapping Talpy Den's states (blocked, idle, stopped, failed, ...) onto these is up to the caller.
typedef enum { JOB_WORKING, JOB_WAITING, JOB_DONE } job_state_t;

// One job, as the PC sends it.
typedef struct {
    const char *name;
    job_state_t state;
    const char *detail;   // "3m", "5m ago"; NULL or "" shows the state word alone
} job_info_t;

// Everything the jobs card shows, in one message.
typedef struct {
    const job_info_t *jobs; // every job, in any order (not only the ones that fit)
    int count;              // how many entries jobs has
    const char *updated;    // time of the PC's message, "14:52:05"; NULL shows "--:--:--"
} job_update_t;

void card_jobs_create(lv_obj_t *parent);

// Fill the whole card from the full job list:
// - the three tiles count every job;
// - the rows show waiting jobs first (they need you), then working, then done,
//   keeping the PC's order within each state;
// - jobs that don't fit are counted in "+N more in Talpy Den" under the rows.
// The card copies the texts, so u and its strings may be freed after the call.
// Call from the LVGL loop (loop()/serial handler), never from another task.
void card_jobs_set(const job_update_t *u);

#ifdef __cplusplus
} /*extern "C"*/
#endif
