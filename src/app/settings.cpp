#include "settings.h"
#include <Preferences.h>
#include <stdio.h>

#define NAMESPACE "desk"

int32_t settings_brightness(void) {
    Preferences prefs;
    prefs.begin(NAMESPACE, false); // (read-write: makes the namespace the first time, so no error then)
    int32_t percent = prefs.getInt("bright", 100);
    prefs.end();
    return percent < 0 || percent > 100 ? 100 : percent;
}

bool settings_stay_on(void) {
    Preferences prefs;
    prefs.begin(NAMESPACE, false);
    bool on = prefs.getBool("stay_on", false);
    prefs.end();
    return on;
}

void settings_tz(char *out, size_t size) {
    Preferences prefs;
    prefs.begin(NAMESPACE, false);
    String tz = prefs.getString("tz", "UTC0");
    prefs.end();
    snprintf(out, size, "%s", tz.c_str());
}

void settings_save_brightness(int32_t percent) {
    Preferences prefs;
    prefs.begin(NAMESPACE, false);
    if (prefs.getInt("bright", -1) != percent) prefs.putInt("bright", percent); // (no flash write for the same)
    prefs.end();
}

void settings_save_stay_on(bool on) {
    Preferences prefs;
    prefs.begin(NAMESPACE, false);
    if (!prefs.isKey("stay_on") || prefs.getBool("stay_on", false) != on) prefs.putBool("stay_on", on);
    prefs.end();
}

void settings_save_tz(const char *tz) {
    Preferences prefs;
    prefs.begin(NAMESPACE, false);
    if (prefs.getString("tz", "") != tz) prefs.putString("tz", tz); // (no flash write for the same)
    prefs.end();
}
