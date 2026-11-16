#ifndef CHECK_H
#define CHECK_H

#include <stdio.h>

// A tiny test harness: CHECK reports the failing line and keeps going, so one run shows every failure.
static int check_fail_count;

#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            printf("%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond);                                            \
            check_fail_count++;                                                                                        \
        }                                                                                                              \
    } while (0)

static inline int check_failures(void) {
    return check_fail_count;
}

#endif
