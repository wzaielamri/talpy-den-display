#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// The settings kept across restarts (a power cut, a flash): in the ESP32's flash (NVS, namespace
// "desk"), written only when one changes.
int32_t settings_brightness(void); // the brightness slider's last value; 100 at first
bool settings_stay_on(void);       // "Stay on"; off at first

void settings_save_brightness(int32_t percent);
void settings_save_stay_on(bool on);

#ifdef __cplusplus
} /*extern "C"*/
#endif
