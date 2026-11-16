#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

// The alert: a box on a screen of its own, for a job. Two kinds:
// - ALERT_WAITING: a job needs you (amber, Talpy waving for you). It stays until OK is tapped or alert_hide().
// - ALERT_DONE: a job finished (green, Talpy finished). It goes by itself after 15 s, or with OK.
// A waiting one comes first: a done one doesn't take its place while it shows.
// kicker: the line over the name ("" for the kind's own: "WAITING FOR YOU" / "FINISHED"); detail: the
// line under it, e.g. "Claude asked a question • 14:52". Shown again, it shows the new texts.
// Copies the texts. Call from the LVGL loop (loop()/serial handler), never from another task.
typedef enum { ALERT_WAITING, ALERT_DONE } alert_kind_t;

// True if it shows it (false: a done one while a waiting one shows).
bool alert_show(alert_kind_t kind, const char *kicker, const char *name, const char *detail);
void alert_hide(void);
bool alert_is_showing(void); // (OK closes it without telling anyone: ask)

#ifdef __cplusplus
} /*extern "C"*/
#endif
