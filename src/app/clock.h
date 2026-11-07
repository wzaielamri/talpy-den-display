#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void clock_start(void);
void clock_sync(long long epoch, const char *tz);

#ifdef __cplusplus
} /*extern "C"*/
#endif