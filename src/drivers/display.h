#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void display_init();

// Whether a finger is on the screen now, read straight from the touch chip (GT911, over I2C), for
// when LVGL isn't running: the board napping while the PC is away.
bool display_touched(void);

// Start the LCD's scan-out again after light sleep stopped it (the board napped).
void display_restart(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif
