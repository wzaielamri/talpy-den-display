#include "check.h"
#include "ui/job_order.h"

#define W JOB_WAITING
#define K JOB_WORKING
#define D JOB_DONE

static job_info_t J(job_state_t state, time_t since, time_t started) {
    job_info_t j = {.name = "x", .state = state, .since = since, .machine = "", .id = "", .started = started};
    return j;
}

// order of jobs as the card shows them, compared with want
static int same_order(const job_info_t *jobs, int n, const int *want) {
    int out[8];
    if (job_order(jobs, n, out) != n) return 0;
    for (int i = 0; i < n; i++)
        if (out[i] != want[i]) return 0;
    return 1;
}

static void test_waiting_first_longest_asking(void) {
    job_info_t jobs[] = {J(W, 300, 0), J(K, 0, 100), J(W, 100, 0)};
    CHECK(same_order(jobs, 3, (int[]){2, 0, 1}));
}

static void test_rest_newest_started_first_whatever_state(void) {
    job_info_t jobs[] = {J(D, 0, 500), J(K, 0, 200), J(K, 0, 900)};
    CHECK(same_order(jobs, 3, (int[]){2, 0, 1}));
}

static void test_started_falls_back_to_since(void) {
    job_info_t jobs[] = {J(D, 0, 600), J(K, 700, 0)};
    CHECK(same_order(jobs, 2, (int[]){1, 0}));
}

static void test_neither_known_last(void) {
    job_info_t jobs[] = {J(K, 0, 0), J(D, 0, 50)};
    CHECK(same_order(jobs, 2, (int[]){1, 0}));
}

static void test_waiting_unknown_since_first(void) {
    job_info_t jobs[] = {J(W, 100, 0), J(W, 0, 0)};
    CHECK(same_order(jobs, 2, (int[]){1, 0}));
}

static void test_ties_keep_pc_order(void) {
    job_info_t jobs[] = {J(K, 0, 5), J(D, 0, 5), J(K, 0, 5)};
    CHECK(same_order(jobs, 3, (int[]){0, 1, 2}));
}

static void test_none(void) {
    int out[1];
    CHECK(job_order(NULL, 0, out) == 0);
}

int main(void) {
    test_waiting_first_longest_asking();
    test_rest_newest_started_first_whatever_state();
    test_started_falls_back_to_since();
    test_neither_known_last();
    test_waiting_unknown_since_first();
    test_ties_keep_pc_order();
    test_none();
    return check_failures() ? 1 : 0;
}
