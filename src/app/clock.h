#pragma once

// A time before this (14 Nov 2023) is a clock that was never set: nothing shows a time or an age from it.
#define CLOCK_SET 1700000000

#ifdef __cplusplus
extern "C" {
#endif

void clock_start(void);
// epoch: the time now (Unix seconds); tz: its POSIX time zone ("CET-1CEST,M3.5.0,M10.5.0/3"), "" to keep the
// one in use. The zone is kept across restarts (UTC till the PC has sent one).
void clock_sync(long long epoch, const char *tz);
void clock_init(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif