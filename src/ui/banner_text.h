#pragma once
#include "ui/talpy_set.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// What the jobs card's banner says and which mood Talpy shows, from the job counts. Pure C (host tests build it).
// Chats that wait for you come first, then working ones, then finished ones; no data yet: "Waiting for the PC".

// The headline ("1 chat needs you", "3 chats working", "12 finished chats", "All quiet"), cut to fit size.
void banner_headline(char *buf, size_t size, bool have_data, int waiting, int working, int done);

talpy_mood_t banner_mood(bool have_data, int waiting, int working, int done);

#ifdef __cplusplus
} /*extern "C"*/
#endif
