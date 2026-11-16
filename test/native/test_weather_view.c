#include "check.h"
#include "ui/weather_view.h"

static void test_mood_mapping(void) {
    CHECK(weather_mood(0, true) == TW_CLEAR_DAY);
    CHECK(weather_mood(0, false) == TW_CLEAR_NIGHT);
    CHECK(weather_mood(1, true) == TW_CLEAR_DAY);
    CHECK(weather_mood(1, false) == TW_CLEAR_NIGHT);
    CHECK(weather_mood(2, true) == TW_PARTLY_DAY);
    CHECK(weather_mood(2, false) == TW_PARTLY_NIGHT);
    CHECK(weather_mood(3, true) == TW_OVERCAST);
    CHECK(weather_mood(3, false) == TW_OVERCAST);
    CHECK(weather_mood(45, true) == TW_FOG);
    CHECK(weather_mood(48, true) == TW_FOG);
    CHECK(weather_mood(51, true) == TW_DRIZZLE);
    CHECK(weather_mood(57, true) == TW_DRIZZLE);
    CHECK(weather_mood(61, true) == TW_RAIN);
    CHECK(weather_mood(67, true) == TW_RAIN);
    CHECK(weather_mood(80, true) == TW_RAIN);
    CHECK(weather_mood(82, true) == TW_RAIN);
    CHECK(weather_mood(71, true) == TW_SNOW);
    CHECK(weather_mood(77, true) == TW_SNOW);
    CHECK(weather_mood(85, true) == TW_SNOW);
    CHECK(weather_mood(86, true) == TW_SNOW);
    CHECK(weather_mood(95, true) == TW_THUNDER);
    CHECK(weather_mood(99, true) == TW_THUNDER);
}

static void test_unknown_code_overcast(void) {
    CHECK(weather_mood(42, true) == TW_OVERCAST);
    CHECK(weather_mood(100, false) == TW_OVERCAST);
}

#define T ((time_t)1700000000)

static wv_input_t offline_input(const wv_hour_t *hours, int n, time_t now) {
    wv_input_t in = {0};
    in.pc_online = false;
    in.hours = hours;
    in.hour_count = n;
    in.clock_known = true;
    in.now = now;
    return in;
}

static void test_online_live(void) {
    wv_input_t in = {0};
    in.pc_online = true;
    in.have_weather = true;
    in.code = 61;
    in.is_day = true;
    in.temp = 12;
    wv_view_t v = weather_view(&in);
    CHECK(v.talpy == TW_RAIN);
    CHECK(v.sky == TW_RAIN);
    CHECK(v.source == WV_LIVE);
    CHECK(v.has_temp && v.temp == 12);
    CHECK(v.code == 61);
    CHECK(v.forecast_at == 0);
}

static void test_online_no_weather(void) {
    wv_input_t in = {0};
    in.pc_online = true;
    wv_view_t v = weather_view(&in);
    CHECK(v.talpy == TW_PLAIN);
    CHECK(v.sky == TW_PLAIN);
    CHECK(v.source == WV_NONE);
    CHECK(!v.has_temp);
}

static void test_offline_forecast(void) {
    wv_hour_t h[2] = {{T, 5, 0, true}, {T + 3600, 9, 61, true}};
    wv_input_t in = offline_input(h, 2, T + 3700);
    wv_view_t v = weather_view(&in);
    CHECK(v.talpy == TW_HIDDEN);
    CHECK(v.sky == TW_RAIN);
    CHECK(v.source == WV_FORECAST);
    CHECK(v.has_temp && v.temp == 9);
    CHECK(v.code == 61);
    CHECK(v.forecast_at == T + 3600);
    // any order
    wv_hour_t r[2] = {h[1], h[0]};
    in = offline_input(r, 2, T + 3700);
    v = weather_view(&in);
    CHECK(v.source == WV_FORECAST && v.forecast_at == T + 3600);
}

static void test_forecast_hour_edges(void) {
    wv_hour_t h[2] = {{T, 5, 0, true}, {T + 3600, 9, 61, true}};
    wv_input_t in = offline_input(h, 2, T);
    wv_view_t v = weather_view(&in);
    CHECK(v.source == WV_FORECAST && v.forecast_at == T && v.temp == 5);
    in = offline_input(h, 2, T + 3600);
    v = weather_view(&in);
    CHECK(v.source == WV_FORECAST && v.forecast_at == T + 3600 && v.temp == 9);
    in = offline_input(h, 2, T + 7200);
    v = weather_view(&in);
    CHECK(v.source == WV_NONE);
    CHECK(v.talpy == TW_HIDDEN);
    in = offline_input(h, 2, T - 1);
    v = weather_view(&in);
    CHECK(v.source == WV_NONE);
}

static void test_offline_no_forecast(void) {
    wv_input_t in = offline_input(NULL, 0, T);
    wv_view_t v = weather_view(&in);
    CHECK(v.talpy == TW_HIDDEN && v.sky == TW_PLAIN && v.source == WV_NONE && !v.has_temp);
    CHECK(v.code == 0 && v.forecast_at == 0);
    wv_hour_t old[1] = {{T - 10000, 5, 0, true}};
    in = offline_input(old, 1, T);
    v = weather_view(&in);
    CHECK(v.talpy == TW_HIDDEN && v.sky == TW_PLAIN && v.source == WV_NONE && !v.has_temp);
    in = offline_input(old, 0, T);
    v = weather_view(&in);
    CHECK(v.source == WV_NONE);
}

static void test_offline_clock_unknown(void) {
    wv_hour_t h[1] = {{T, 5, 0, true}};
    wv_input_t in = offline_input(h, 1, T);
    in.clock_known = false;
    wv_view_t v = weather_view(&in);
    CHECK(v.talpy == TW_HIDDEN && v.sky == TW_PLAIN && v.source == WV_NONE && !v.has_temp);
}

static void test_back_online_goes_live(void) {
    wv_hour_t h[2] = {{T, 5, 0, true}, {T + 3600, 9, 61, true}};
    wv_input_t in = offline_input(h, 2, T + 3700);
    in.pc_online = true;
    in.have_weather = true;
    in.code = 71;
    in.is_day = false;
    in.temp = -2;
    wv_view_t v = weather_view(&in);
    CHECK(v.source == WV_LIVE);
    CHECK(v.talpy == TW_SNOW && v.sky == TW_SNOW);
    CHECK(v.temp == -2 && v.forecast_at == 0);
}

int main(void) {
    test_mood_mapping();
    test_unknown_code_overcast();
    test_online_live();
    test_online_no_weather();
    test_offline_forecast();
    test_forecast_hour_edges();
    test_offline_no_forecast();
    test_offline_clock_unknown();
    test_back_online_goes_live();
    return check_failures() ? 1 : 0;
}
