#pragma once
#include "ui/job_info.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// The alerts come from the jobs themselves: each jobs message is compared with the one before, job by
// job (by id), and a change alerts.
// - A job starting to wait for you (also a new one that waits at once, or one that already waits in
//   the first message after the board starts): the waiting alert, till OK.
// - A job finishing (to done from working or waiting): the finished alert, which goes by itself.
// Nothing changed, nothing new: a job that keeps waiting doesn't bring back the alert you closed.
// A job that stops waiting (you answered it) or goes takes its waiting alert away.
void job_watch_update(const job_info_t *jobs, int n);

// The laptop locked (true) or unlocked. Locked, no alert shows (an alert shows a chat's name): the one
// showing goes, and the jobs go on being followed. A waiting alert not closed with OK is owed: the one the
// lock hid, and one for each job that starts waiting meanwhile. Unlocked, the owed job asking longest gets
// its waiting alert, if it still waits; finished alerts from meanwhile are dropped.
void job_watch_set_locked(bool locked);

#ifdef __cplusplus
} /*extern "C"*/
#endif
