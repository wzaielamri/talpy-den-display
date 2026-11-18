#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "ui/job_info.h"

// The order the jobs card shows jobs in, Talpy Den's steady order turned over so the newest shows:
// - waiting jobs first, the one asking longest on top (by since, oldest first; an unknown since counts as 0);
// - then every other job, working or done alike, by when it started, the newest first (started; 0: its since;
//   neither known: last), so a row doesn't move when its chat finishes;
// - equal keys keep the PC's order.
// Fills out[0..n-1] with indexes into jobs, the top row's first; returns n (0 for n <= 0).
int job_order(const job_info_t *jobs, int n, int *out);

#ifdef __cplusplus
} /*extern "C"*/
#endif
