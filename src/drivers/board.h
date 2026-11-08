#pragma once
#include <stdbool.h>
#include <stdint.h>

void board_init(void);

// Light sleep for up to ms (the CPU stops, memory and the serial port's settings stay): it wakes early
// when data comes in on the serial port. True if that's what woke it; the bytes that woke it are
// lost, so a message arriving then comes in cut (the next one comes in whole).
bool board_nap(uint32_t ms);
