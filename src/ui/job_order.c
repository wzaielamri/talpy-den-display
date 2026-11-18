#include "ui/job_order.h"
#include <stdbool.h>

// Whether job a goes above job b.
static bool before(const job_info_t *a, const job_info_t *b) {
    bool a_waits = a->state == JOB_WAITING, b_waits = b->state == JOB_WAITING;
    if (a_waits != b_waits) return a_waits;
    if (a_waits) return a->since < b->since; // asking longest first
    time_t a_start = a->started ? a->started : a->since;
    time_t b_start = b->started ? b->started : b->since;
    return a_start > b_start; // the newest first
}

int job_order(const job_info_t *jobs, int n, int *out) {
    if (!jobs || n <= 0) return 0;
    // An insertion sort (n is at most 64): stable, so equal keys keep the PC's order.
    for (int i = 0; i < n; i++) {
        int k = i;
        while (k > 0 && before(&jobs[i], &jobs[out[k - 1]])) {
            out[k] = out[k - 1];
            k--;
        }
        out[k] = i;
    }
    return n;
}
