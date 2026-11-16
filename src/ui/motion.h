#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// What moves on the main screen (the weather Talpy, the banner's Talpy) holds still while something
// covers it: drawn again and again under a see-through cover it showed through (the pull-down's
// dimming), and under the clock face or the alert's screen nobody sees it anyway. Each cover holds
// and lets go on its own; it moves again once none holds it.
// The alert's own Talpy is not on the main screen: it holds still only while the shade is out (the
// shade can be pulled over the alert too), told by alert_motion_hold(), which the alert combines with
// whether it shows. The alert itself holds the main screen's animations with MOTION_ALERT.
enum { MOTION_SHADE = 1, MOTION_CLOCK = 2, MOTION_ALERT = 4 };

void motion_hold(unsigned who, bool on);

#ifdef __cplusplus
} /*extern "C"*/
#endif
