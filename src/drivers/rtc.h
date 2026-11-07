#pragma once
#include <stdbool.h>
#include <time.h>


#ifdef __cplusplus
extern "C" {
#endif

bool rtc_read(time_t *utc);
void rtc_write(time_t utc);

#ifdef __cplusplus
} /*extern "C"*/
#endif