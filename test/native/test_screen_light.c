#include "check.h"
#include "app/screen_light.h"

static void test_the_cards_at_the_sliders_brightness(void) {
    CHECK(screen_light_percent(100, false, false) == 100);
    CHECK(screen_light_percent(40, false, false) == 40);
}

static void test_the_clock_while_you_are_there_at_the_sliders_brightness(void) {
    // (unlocked and the PC there: you're at the laptop, the clock as bright as the cards)
    CHECK(screen_light_percent(100, true, false) == 100);
    CHECK(screen_light_percent(60, true, false) == 60);
}

static void test_the_locked_clock_dimmed_to_10_percent(void) {
    // (the laptop locked or the PC away: nobody looks, and 10% doesn't hum)
    CHECK(screen_light_percent(100, true, true) == 10);
    CHECK(screen_light_percent(60, true, true) == 10);
}

static void test_the_locked_clock_never_brighter_than_the_slider(void) {
    CHECK(screen_light_percent(5, true, true) == 5);
    CHECK(screen_light_percent(0, true, true) == 0);
}

int main(void) {
    test_the_cards_at_the_sliders_brightness();
    test_the_clock_while_you_are_there_at_the_sliders_brightness();
    test_the_locked_clock_dimmed_to_10_percent();
    test_the_locked_clock_never_brighter_than_the_slider();
    return check_failures() ? 1 : 0;
}
