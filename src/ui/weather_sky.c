// The live sky behind and in front of the weather Talpy (see weather_sky.h), built from simple shapes.
//
// Every sky is drawn in two 104 x 96 boxes: back (behind Talpy) and front (in front of him: drops, flakes, fog bands).
// All x/y below are scene px in those boxes, with y growing downward. Shapes created later are drawn on top, and a box
// or group clips what's inside it: a cloud past x 104 or a drop past y 96 is simply cut off.
//
// Sizes, places and colours are those of the approved mockup (round 3 of compose.py, in
// docs/superpowers/specs/2026-11-16-weather-talpy-mockup/), whose scene is in box units (viewBox -14 -22 95 88) shown
// at 104 x 96 px:
//     px = (x + 14) * 104 / 95,   py = (y + 22) * 96 / 88,   a length: 1.093 px a unit
// The draw_... functions pass compose.py's own numbers; ux(), uy() and us() turn them into px. The building blocks
// (clouds, sun, moon, stars, drops, the lightning) are in weather_sky_parts.c.
//
// Animations are lv_anims on the shapes, as in weather_icon.c. Each sky keeps a list of the ones it started, so freeze
// can stop them where they are (keeping a copy of each) and start the copies again later, going on from there.
//
// To change a sky, edit only its draw_... function.

#include "weather_sky.h"
#include "theme.h"
#include "weather_anim.h"
#include "weather_sky_parts.h"
#include <stdlib.h>

#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

// Falls from above the box (its top at -h) to below it (its top at 96), at today's weather icon speeds: there rain
// fell 56 px in 800 ms, drizzle in 1300 ms, snow in 2600 ms.
#define FALL_MS(h, ms_per_56) ((uint32_t)((BOX_H + (h)) * (ms_per_56) / 56))

// ---------- Talpy, measured ----------
//
// Measured on TALPY_WEATHER_SET's rest frames (decoded on the PC with talpy_codec.c, as test/native/test_data.c
// does), moved by the canvas's place in the box (x 20, y 28):
//   FOG: the antenna light's yellow core centres at (53.4, 42.7), in a ring of about 7 px radius.
//   Stop lines (the drop tables below): the first row with alpha >= 128 across a drop's columns, i.e. the top of
//   whatever is there (umbrella canopy, hat brim, beanie, head, the burrow's mound); nothing there: 96.
//   RAIN: the canopy spans x 30..82, its top 29 at the ferrule (x 55..57), 30 at x 50..62, 33..34 at x 38 and 75,
//   42 at the rims (the twirl's frames don't rise above it at the drops' columns).
//   DRIZZLE: the brims x 28..36 and 63..71, their top 62 over most of them (61..65 from the crown to the tips).
//   SNOW: the pompom's top 44..45 (x 38..42), the beanie's 45..47 (x 43..48), the mound 75..85 at the sides.
//   THUNDER (peeking): the lamp's top 49 (x 49..57), his head 67..74 beside it, the mound 80..85.
#define LAMP_X 53 // the lamp's pixel
#define LAMP_Y 42

// ---------- one function per sky ----------

// 1 Clear (day): compose sun(62, -6, 10.5), its rays turning (a full turn in 24 s).
static void draw_clear_day(weather_sky_t *s) { sky_sun(s, 62, -6, 10.5f, true); }

// 2 Partly cloudy (day): compose sun(66, -10, 9), rays still, and a day cloud(55, 3, 1.55) partly over it, drifting
// 18 px left (off the sun) over 12 s and back.
static void draw_partly_day(weather_sky_t *s) {
    sky_sun(s, 66, -10, 9, false);
    sky_drift(s, sky_cloud(s->back, 55, 3, 1.55f, DAY_CLOUD, DAY_EDGE, NULL), -18, 12000);
}

// 3 Overcast: compose cloud(10, -11, 1.45) and cloud(60, -4, 1.7), grey, drifting 6 px towards each other and back.
static void draw_overcast(weather_sky_t *s) {
    sky_drift(s, sky_cloud(s->back, 10, -11, 1.45f, COLOR_CLOUD, GREY_EDGE, NULL), 6, 14000);
    sky_drift(s, sky_cloud(s->back, 60, -4, 1.7f, COLOR_CLOUD, GREY_EDGE, NULL), -6, 17000);
}

// 4 Fog: compose's BEAM and FOG. The beam and the glow sit on Talpy's lamp (measured), behind him; the five bands
// in front of him.
#define BEAM_R     56 // past the box's right edge at the beam's spread: the edge cuts the slice straight, as in compose
#define BAND_DRIFT 6
static void draw_fog(weather_sky_t *s) {
    // The beam: compose's triangle from the lamp to the right edge (18 units up and down over 47: +-21 degrees), 12 %,
    // as a pie slice (an arc as wide as its radius).
    lv_obj_t *beam = lv_arc_create(s->back);
    lv_obj_remove_style_all(beam);
    lv_obj_remove_flag(beam, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(beam, LAMP_X - BEAM_R, LAMP_Y - BEAM_R);
    lv_obj_set_size(beam, 2 * BEAM_R, 2 * BEAM_R);
    lv_arc_set_bg_angles(beam, 339, 21);
    lv_obj_set_style_arc_width(beam, BEAM_R, LV_PART_MAIN);
    lv_obj_set_style_arc_color(beam, lv_color_hex(BEAM_COLOR), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(beam, OPA(12), LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(beam, false, LV_PART_MAIN);

    // The glow: compose's r 8 at 30 %, here on the lamp and r 12 px so it shows around the lamp's own 7 px ring,
    // breathing between 20 and 40 % over 2.6 s each way.
    lv_obj_t *glow = sky_disc(s->back, LAMP_X, LAMP_Y, 12, BEAM_COLOR, OPA(20));
    lv_anim_t a;
    wx_anim_bounce(&a, glow, sky_anim_bg_opa, OPA(20), OPA(40), PACE(s, 2600), 0);
    sky_start(s, &a);

    // The bands: compose (x, y, w; 7 high, round ends) at 42 %, drifting BAND_DRIFT px, one way and the next the other,
    // over 7-9.4 s. A band running off the box is longer there, so its drift never shows that end.
    static const int8_t band[5][3] = {{24, -16, 62}, {-16, -2, 52}, {-16, 24, 64}, {20, 37, 70}, {-16, 50, 56}};
    for (int i = 0; i < COUNT(band); i++) {
        int x = rnd(ux(band[i][0])), w = rnd(ux(band[i][0] + band[i][2])) - x;
        if (x < 0) {
            x -= BAND_DRIFT + 4;
            w += BAND_DRIFT + 4;
        }
        if (x + w > BOX_W)
            w += BAND_DRIFT + 4;
        lv_obj_t *b = sky_blob(s->front, x, rnd(uy(band[i][1])), w, rnd(us(7)), LV_RADIUS_CIRCLE, COLOR_CLOUD, OPA(42));
        sky_drift(s, b, i % 2 ? -BAND_DRIFT : BAND_DRIFT, 7000 + 600 * (uint32_t)i);
    }
}

// 5 Drizzle: compose cloud(60, -8, 1.55) and 6 light drops (compose drops(..., 1.2): 2.9 x 6.2 units -> 3 x 7 px),
// two of them landing on the hat's brims. Calm: the first 3.
static const drop_t DRIZZLE_DROPS[] = {
    {67, 62, true},  // from the cloud onto the right brim
    { 9, 96, false},
    {88, 96, true},
    {33, 62, false}, // onto the left brim
    {75, 81, true},  // the mound
    {99, 96, true},
};
static void draw_drizzle(weather_sky_t *s) {
    int bottom;
    sky_cloud(s->back, 60, -8, 1.55f, DAY_CLOUD, DAY_EDGE, &bottom);
    sky_drops(s, DRIZZLE_DROPS, COUNT(DRIZZLE_DROPS), 3, 7, FALL_MS(7, 1300), bottom);
}

// 6 Rain: compose cloud(62, -10, 1.55) and 10 drops (compose drops(..., 1.3): 3.1 x 6.8 units -> 3 x 8 px, a little
// longer for the speed), at today's rain speed, those over Talpy stopping at the umbrella's canopy. Calm: the first 5.
static const drop_t RAIN_DROPS[] = {
    {75, 34, true},  // from the cloud onto the canopy
    {99, 96, true},
    {38, 33, false}, // canopy
    { 5, 96, false},
    {84, 96, true},  // just past the canopy's right rim
    {50, 30, false}, // canopy
    {91, 96, true},
    {14, 96, false},
    {62, 30, false}, // canopy
    {26, 80, false}, // the mound, left of the canopy
};
static void draw_rain(weather_sky_t *s) {
    int bottom;
    sky_cloud(s->back, 62, -10, 1.55f, DAY_CLOUD, DAY_EDGE, &bottom);
    sky_drops(s, RAIN_DROPS, COUNT(RAIN_DROPS), 3, 8, FALL_MS(8, 800), bottom);
}

// 7 Snow: compose's 10 flakes (r 1.9-2.6 units -> 4-6 px), no cloud, falling slowly and steadily from above the box,
// those over Talpy settling on the beanie, his head or the mound. Calm: the first 5.
typedef struct { int16_t x, stop, d; } flake_t; // centre x, stop line, diameter (px)
static const flake_t SNOW_FLAKES[] = {
    {45, 45, 5}, // the beanie
    {92, 96, 6},
    {15, 96, 4},
    {72, 75, 5}, // the mound
    {26, 79, 6}, // the mound
    {62, 51, 5}, // the beanie
    { 5, 96, 4},
    {83, 96, 5},
    {38, 44, 5}, // the pompom
    {100, 96, 4},
};
static void draw_snow(weather_sky_t *s) {
    const int n = s->calm ? (COUNT(SNOW_FLAKES) + 1) / 2 : COUNT(SNOW_FLAKES);
    const uint32_t ms = FALL_MS(5, 2600);
    for (int i = 0; i < n; i++) {
        const flake_t *f = &SNOW_FLAKES[i];
        sky_faller(s, s->front, 0, f->x - f->d / 2, f->d, f->d, f->stop, SNOW_COLOR, ms, (uint32_t)i * ms / (uint32_t)n,
                   lv_anim_path_linear);
    }
}

// 8 Thunderstorm: compose's storm cloud(62, -10, 1.8) with the bolt under it, hidden, flashing now and then (and
// calling the flash callback), and 4 drops on the left (compose drops(..., 1.2) -> 3 x 7 px). Calm: 2 drops.
static const drop_t THUNDER_DROPS[] = {
    {35, 72, false}, // his head (peeking)
    { 9, 96, false},
    {44, 67, false}, // his head
    {20, 84, false}, // the mound
};
static void draw_thunder(weather_sky_t *s) {
    sky_cloud(s->back, 62, -10, 1.8f, COLOR_DIM, STORM_EDGE, NULL);
    sky_bolt(s);
    sky_drops(s, THUNDER_DROPS, COUNT(THUNDER_DROPS), 3, 7, FALL_MS(7, 800), 0);
}

// 9 Clear (night): compose moon(62, -6) and its 4 stars, twinkling slowly.
static void draw_clear_night(weather_sky_t *s) {
    sky_moon(s, 62, -6);
    sky_star(s, 38, -14, 1.2f, 1.0f, 2600, 0);
    sky_star(s, 80, 12, 0.9f, 0.8f, 3000, 700);
    sky_star(s, 12, -8, 1.0f, 0.9f, 2800, 1400);
    sky_star(s, 82, -18, 0.8f, 0.7f, 3200, 2100);
}

// 10 Partly cloudy (night): the moon, a night cloud(66, 3, 1.35) over its lower right, drifting 22 px left across it
// over 16 s and back, and two of the stars.
static void draw_partly_night(weather_sky_t *s) {
    sky_moon(s, 62, -6);
    sky_drift(s, sky_cloud(s->back, 66, 3, 1.35f, COLOR_MUTED, NIGHT_EDGE, NULL), -22, 16000);
    sky_star(s, 38, -14, 1.2f, 1.0f, 2600, 0);
    sky_star(s, 12, -8, 1.0f, 0.9f, 2800, 1400);
}

// ---------- public ----------

static void sky_deleted(lv_event_t *e) { free(lv_event_get_user_data(e)); }

weather_sky_t *weather_sky_create(lv_obj_t *back, lv_obj_t *front) {
    weather_sky_t *s = calloc(1, sizeof *s); // (not lv_malloc: LVGL's own heap is small)
    if (!s)
        return NULL;
    s->back = back;
    s->front = front;
    s->shown = TW_PLAIN;
    lv_obj_add_event_cb(back, sky_deleted, LV_EVENT_DELETE, s);
    return s;
}

void weather_sky_set(weather_sky_t *s, tw_mood_t sky, bool calm) {
    if (!s || (sky == s->shown && calm == s->calm))
        return; // the same sky: keep it running
    lv_obj_clean(s->back); // delete the old shapes; LVGL deletes their animations with them
    lv_obj_clean(s->front);
    s->anim_count = 0;
    s->lit = false;
    s->shown = sky;
    s->calm = calm;
    switch (sky) {
    case TW_CLEAR_DAY:    draw_clear_day(s);    break;
    case TW_PARTLY_DAY:   draw_partly_day(s);   break;
    case TW_OVERCAST:     draw_overcast(s);     break;
    case TW_FOG:          draw_fog(s);          break;
    case TW_DRIZZLE:      draw_drizzle(s);      break;
    case TW_RAIN:         draw_rain(s);         break;
    case TW_SNOW:         draw_snow(s);         break;
    case TW_THUNDER:      draw_thunder(s);      break;
    case TW_CLEAR_NIGHT:  draw_clear_night(s);  break;
    case TW_PARTLY_NIGHT: draw_partly_night(s); break;
    default:              break; // TW_PLAIN, TW_HIDDEN: no sky
    }
}

void weather_sky_freeze(weather_sky_t *s, bool on) {
    if (!s || on == s->frozen)
        return;
    s->frozen = on;
    for (int i = 0; i < s->anim_count; i++) {
        sky_anim_t *e = &s->anims[i];
        if (on) {
            lv_anim_t *live = lv_anim_get(e->a.var, e->a.exec_cb);
            e->held = live != NULL;
            if (live) {
                e->a = *live; // where it is: its time, direction, values (the shape stays there)
                lv_anim_delete(e->a.var, e->a.exec_cb);
            }
        } else if (e->held) {
            e->held = false;
            lv_anim_start(&e->a); // goes on from the copy's time
        }
    }
}

void weather_sky_on_flash(weather_sky_t *s, void (*cb)(void *user), void *user) {
    if (!s)
        return;
    s->flash_cb = cb;
    s->flash_user = user;
}
