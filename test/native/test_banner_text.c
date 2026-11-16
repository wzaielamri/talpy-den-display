#include "check.h"
#include "ui/banner_text.h"
#include <string.h>

static void test_headline_no_data(void) {
    char b[64];
    banner_headline(b, sizeof b, false, 3, 2, 1);
    CHECK(strcmp(b, "Waiting for the PC") == 0);
    CHECK(banner_mood(false, 3, 2, 1) == TALPY_LOADING);
}

static void test_headline_waiting(void) {
    char b[64];
    banner_headline(b, sizeof b, true, 1, 5, 5);
    CHECK(strcmp(b, "1 chat needs you") == 0);
    banner_headline(b, sizeof b, true, 2, 0, 0);
    CHECK(strcmp(b, "2 chats need you") == 0);
    CHECK(banner_mood(true, 1, 5, 5) == TALPY_NEEDS);
}

static void test_headline_working(void) {
    char b[64];
    banner_headline(b, sizeof b, true, 0, 1, 3);
    CHECK(strcmp(b, "1 chat working") == 0);
    banner_headline(b, sizeof b, true, 0, 3, 0);
    CHECK(strcmp(b, "3 chats working") == 0);
    CHECK(banner_mood(true, 0, 1, 3) == TALPY_WORKING);
}

static void test_headline_done(void) {
    char b[64];
    banner_headline(b, sizeof b, true, 0, 0, 1);
    CHECK(strcmp(b, "1 finished chat") == 0);
    banner_headline(b, sizeof b, true, 0, 0, 12);
    CHECK(strcmp(b, "12 finished chats") == 0);
    CHECK(banner_mood(true, 0, 0, 1) == TALPY_DONE);
}

static void test_headline_all_quiet(void) {
    char b[64];
    banner_headline(b, sizeof b, true, 0, 0, 0);
    CHECK(strcmp(b, "All quiet") == 0);
    CHECK(banner_mood(true, 0, 0, 0) == TALPY_DONE);
}

static void test_headline_fits_small_buffer(void) {
    char b[8];
    memset(b, 'x', sizeof b);
    banner_headline(b, sizeof b, true, 2, 0, 0);
    CHECK(strlen(b) == 7);
    CHECK(strcmp(b, "2 chats") == 0);
}

int main(void) {
    test_headline_no_data();
    test_headline_waiting();
    test_headline_working();
    test_headline_done();
    test_headline_all_quiet();
    test_headline_fits_small_buffer();
    return check_failures() ? 1 : 0;
}
