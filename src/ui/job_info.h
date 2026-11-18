#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include <time.h>

// One job (a chat), as the PC sends it, with no LVGL: the jobs card, the alerts (app/job_watch.c) and the order
// (job_order.c) share it, and the native tests build it.

// The three states the screen can show (one colour each).
// Mapping Talpy Den's states (blocked, idle, stopped, failed, ...) onto these is up to the caller.
typedef enum { JOB_WORKING, JOB_WAITING, JOB_DONE } job_state_t;

typedef struct {
    const char *name;
    job_state_t state;
    time_t since;         // when it got into this state, Unix seconds (UTC); 0: not known
    const char *machine;  // where it runs, a small line under the name ("local", "lab (ssh)");
                          // NULL or "": the name alone, on one line
    const char *id;       // stable while the job is listed (jobs.json's "id"): job_watch.c tells a job's
                          // state change by it, and a tap asks the PC to bring up the job of this id; NULL or "":
                          // name and machine stand in (and the row can't be tapped)
    time_t started;       // when the chat started, Unix seconds (jobs.json's "startedAt"); 0: not known (its
                          // since stands in for the order)
} job_info_t;

#ifdef __cplusplus
} /*extern "C"*/
#endif
