#pragma once
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
// The board's lock: on while the laptop's screen is locked (sync.py's lock message) or the PC is away (no message
// for a while, or it said it suspends: unplugged, asleep, off, sync.py stopped). Either way nobody is at the laptop
// to see what the board shows, and a board left on the desk mustn't show a chat's name to whoever comes by; while
// the PC is away its chats can't be known either. On: the clock face, and it stays (a touch doesn't bring the
// cards back), no alert (job_watch.h: a waiting alert not closed with OK is owed); off: the cards, then the owed
// alert. It goes off only once the laptop is unlocked and the PC there both. The board starts unlocked, the PC
// taken for there (serial_link.cpp: it powers the board).
// The laptop's own lock (true: locked), from its lock message.
void board_lock_laptop(bool locked);
// The PC there (true: a message came in) or away.
void board_lock_pc(bool present);
bool board_lock_on(void);
#ifdef __cplusplus
} /*extern "C"*/
#endif
