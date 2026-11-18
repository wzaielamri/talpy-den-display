#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h> // (no LVGL type in what it offers: app/job_watch.c builds natively for its tests)

// The alert: a box on a screen of its own, for a job. Two kinds:
// - ALERT_WAITING: a job needs you (amber, Talpy asking for you: alert, sign, wave). It stays until OK is tapped or alert_hide().
// - ALERT_DONE: a job finished (green, Talpy finished). It goes by itself after 15 s, or with OK.
// A waiting one comes first: a done one doesn't take its place while it shows.
// kicker: the line over the name ("" for the kind's own: "WAITING FOR YOU" / "FINISHED"); detail: the
// line under it, e.g. "Claude asked a question • 14:52". Shown again, it shows the new texts.
// Copies the texts. Call from the LVGL loop (loop()/serial handler), never from another task.
typedef enum { ALERT_WAITING, ALERT_DONE } alert_kind_t;

// True if it shows it (false: a done one while a waiting one shows).
bool alert_show(alert_kind_t kind, const char *kicker, const char *name, const char *detail);
void alert_hide(void);
// Only for motion.c: the pull-down shade is out (true) or gone. The alert's Talpy then holds still.
void alert_motion_hold(bool shade_out);
bool alert_is_showing(void); // (OK closes it without telling anyone: ask)

// "Open on PC", next to OK on both kinds: calls cb (job_watch.c asks the PC to bring the alert's job to the
// front); the alert goes if cb says the PC was asked (true), else it stays (an open already under way).
typedef bool (*alert_open_cb_t)(void);
void alert_on_open(alert_open_cb_t cb);

// "Open on PC" shown (true) or not, for the alert showing now (job_watch.c: not for a job without a usable id).
void alert_show_open(bool on);

#ifdef __cplusplus
} /*extern "C"*/
#endif
