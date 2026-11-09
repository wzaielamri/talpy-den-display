#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// The settings kept across restarts (a power cut, a flash): in the ESP32's flash (NVS, namespace
// "desk"), written only when one changes.
int32_t settings_brightness(void); // the brightness slider's last value; 100 at first
bool settings_stay_on(void);       // "Stay on"; off at first
void settings_tz(char *out, size_t size); // the time zone the PC sent last (a POSIX TZ string); "UTC0" at first

void settings_save_brightness(int32_t percent);
void settings_save_stay_on(bool on);
void settings_save_tz(const char *tz);

#ifdef __cplusplus
} /*extern "C"*/
#endif
