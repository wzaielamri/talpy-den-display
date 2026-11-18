#include "check.h"
#include "app/board_lock.h"
#include "app/job_watch.h"
#include "app/screen_power.h"
#include <string.h>

// Fakes for what board_lock.c calls: each call is written down, in order.
static char calls[256];

static void note(const char *what) {
    strncat(calls, what, sizeof calls - strlen(calls) - 1);
}
void job_watch_set_locked(bool locked) { note(locked ? "alerts:on " : "alerts:off "); }
void screen_power_set_locked(bool locked) { note(locked ? "screen:on " : "screen:off "); }

static void clear(void) { calls[0] = 0; }

// Back to the start (the laptop unlocked, the PC there), whatever a test left.
static void reset(void) {
    board_lock_laptop(false);
    board_lock_pc(true);
    clear();
}

static void test_starts_unlocked(void) {
    CHECK(!board_lock_on());
}

static void test_pc_away_locks_the_board(void) {
    reset();
    board_lock_pc(false);
    CHECK(board_lock_on());
    // (the alerts first: a waiting alert showing is owed before the clock covers it)
    CHECK(strcmp(calls, "alerts:on screen:on ") == 0);
}

static void test_pc_back_unlocks_it(void) {
    reset();
    board_lock_pc(false);
    clear();
    board_lock_pc(true);
    CHECK(!board_lock_on());
    // (the cards first, then the owed alert over them)
    CHECK(strcmp(calls, "screen:off alerts:off ") == 0);
}

static void test_pc_back_while_the_laptop_is_locked_stays_locked(void) {
    reset();
    board_lock_laptop(true);
    board_lock_pc(false);
    clear();
    board_lock_pc(true);
    CHECK(board_lock_on());
    CHECK(calls[0] == 0);
    board_lock_laptop(false);
    CHECK(!board_lock_on());
    CHECK(strcmp(calls, "screen:off alerts:off ") == 0);
}

static void test_the_laptop_locked_on_its_way_back_never_unlocks(void) {
    // Away unlocked; the first message back says it's locked now (sync.py sends the lock first): no unlock between.
    reset();
    board_lock_pc(false);
    clear();
    board_lock_laptop(true);
    board_lock_pc(true);
    CHECK(board_lock_on());
    CHECK(calls[0] == 0);
}

static void test_the_laptop_unlocked_while_away_stays_locked_till_back(void) {
    reset();
    board_lock_laptop(true);
    board_lock_pc(false);
    clear();
    board_lock_laptop(false);
    CHECK(board_lock_on());
    CHECK(calls[0] == 0);
    board_lock_pc(true);
    CHECK(!board_lock_on());
    CHECK(strcmp(calls, "screen:off alerts:off ") == 0);
}

static void test_the_laptop_lock_alone_still_locks(void) {
    reset();
    board_lock_laptop(true);
    CHECK(board_lock_on());
    CHECK(strcmp(calls, "alerts:on screen:on ") == 0);
    clear();
    board_lock_laptop(false);
    CHECK(!board_lock_on());
    CHECK(strcmp(calls, "screen:off alerts:off ") == 0);
}

static void test_the_same_again_tells_nobody(void) {
    reset();
    board_lock_pc(true);
    board_lock_laptop(false);
    CHECK(calls[0] == 0);
    board_lock_pc(false);
    clear();
    board_lock_pc(false);
    CHECK(calls[0] == 0);
}

int main(void) {
    test_starts_unlocked();
    test_pc_away_locks_the_board();
    test_pc_back_unlocks_it();
    test_pc_back_while_the_laptop_is_locked_stays_locked();
    test_the_laptop_locked_on_its_way_back_never_unlocks();
    test_the_laptop_unlocked_while_away_stays_locked_till_back();
    test_the_laptop_lock_alone_still_locks();
    test_the_same_again_tells_nobody();
    return check_failures() ? 1 : 0;
}
