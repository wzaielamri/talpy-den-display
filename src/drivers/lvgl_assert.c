#include "lvgl_assert.h"
#include <stdio.h>
#include <stdlib.h>

void lvgl_assert_failed(const char *file, int line) {
    printf("LVGL check failed at %s:%d: restarting\n", file, line);
    fflush(stdout);
    abort(); // prints a backtrace, then the board restarts
}
