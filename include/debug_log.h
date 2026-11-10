#pragma once

// DESK_DEBUG 1 (platformio.ini, build_flags): the board also prints every message it gets, what it made
// of it, and each backlight and RTC command. sync.py passes all the board prints to its log (journalctl),
// so with a ping every 10 s that's thousands of lines a day: off (0) by default. Off, it still prints
// what matters: "Setup done" (sync.py sends everything again on it), "PC back"/"PC away", and whatever
// went wrong ("bad line: ...", I2C errors).
#ifndef DESK_DEBUG
#define DESK_DEBUG 0
#endif

// Serial.printf with DESK_DEBUG 1, nothing without (the compiler still checks the arguments).
// For .cpp files that include <Arduino.h>.
#define debug_printf(...)                                                                                    \
    do {                                                                                                     \
        if (DESK_DEBUG) Serial.printf(__VA_ARGS__);                                                          \
    } while (0)
