#include "ui/weather_view.h"

#define HOUR_SECONDS 3600

// Mirrors kind_from_wmo() in weather_icon.c, with day/night for clear and partly cloudy.
tw_mood_t weather_mood(int code, bool is_day) {
    if (code <= 1) return is_day ? TW_CLEAR_DAY : TW_CLEAR_NIGHT;
    if (code == 2) return is_day ? TW_PARTLY_DAY : TW_PARTLY_NIGHT;
    if (code == 3) return TW_OVERCAST;
    if (code == 45 || code == 48) return TW_FOG;
    if (code >= 51 && code <= 57) return TW_DRIZZLE;
    if ((code >= 61 && code <= 67) || (code >= 80 && code <= 82)) return TW_RAIN;
    if ((code >= 71 && code <= 77) || code == 85 || code == 86) return TW_SNOW;
    if (code >= 95 && code <= 99) return TW_THUNDER;
    return TW_OVERCAST; // unknown code: plain cloud
}

wv_view_t weather_view(const wv_input_t *in) {
    wv_view_t v = {.talpy = TW_PLAIN, .sky = TW_PLAIN, .source = WV_NONE};

    if (in->pc_online) {
        if (in->have_weather) {
            v.talpy = v.sky = weather_mood(in->code, in->is_day);
            v.source = WV_LIVE;
            v.has_temp = true;
            v.temp = in->temp;
            v.code = in->code;
        }
        return v;
    }

    v.talpy = TW_HIDDEN; // away: Talpy stays in his burrow
    if (!in->clock_known || !in->hours || in->hour_count <= 0) return v;

    const wv_hour_t *best = 0;
    for (int i = 0; i < in->hour_count; i++) {
        const wv_hour_t *h = &in->hours[i];
        if (h->at <= in->now && in->now < h->at + HOUR_SECONDS && (!best || h->at > best->at)) best = h;
    }
    if (!best) return v;

    v.sky = weather_mood(best->code, best->is_day);
    v.source = WV_FORECAST;
    v.has_temp = true;
    v.temp = best->temp;
    v.code = best->code;
    v.forecast_at = best->at;
    return v;
}
