#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// A tap on a chat (its row, or "Open on PC" on an alert) asks the PC to bring it to the front: the board prints
// {"type":"open","id":"<id>"} on its serial line, sync.py runs Talpy Den's jump.py --job with the id, and answers
// {"type":"opened","id":"<id>","ok":true|false,"message":"..."} (serial_link.cpp). One at a time.
// The row says "Opening..." till the answer, then "Opened" or the message for 4 s (ui/card_jobs.c); no answer in
// OPEN_WAIT_MS: "No answer from the PC". Nothing while the laptop is locked (screen_power_locked).

// Asks the PC to bring up the job of this id. False (and nothing sent): no id, locked, or one under way.
bool pc_open_request(const char *id);

// The PC's answer for id (serial_link.cpp's "opened"): ignored unless it's the one under way (a late one, after
// "No answer from the PC", goes); frees the board for the next tap.
void pc_open_answer(const char *id, bool ok, const char *message);

#ifdef __cplusplus
} /*extern "C"*/
#endif
