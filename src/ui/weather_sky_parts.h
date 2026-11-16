#pragma once

// The live sky's insides, shared by weather_sky.c (one draw function per sky, the public calls) and
// weather_sky_parts.c (the building blocks: clouds, sun, moon, stars, drops, the lightning, and the animation list).
// Not for anything else.

#include "lvgl.h"
#include "talpy_set.h"
#include "weather_sky.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#define BOX_W 104
#define BOX_H 96

// compose.py's colours that theme.h doesn't have (COLOR_CLOUD is its overcast grey and fog, COLOR_DIM the storm cloud,
// COLOR_MUTED the night cloud, COLOR_TEXT the stars, COLOR_SUN the sun and bolt, COLOR_MOON the moon).
#define DAY_CLOUD  0xEEF1F8
#define DAY_EDGE   0xC4CBDC
#define GREY_EDGE  0x9AA5B5
#define STORM_EDGE 0x454C57
#define NIGHT_EDGE 0x6B7480
#define DROP_COLOR 0x7CC4F2
#define SNOW_COLOR 0xFFFFFF
#define BEAM_COLOR 0xFFD86B // the fog lamp's glow and beam

#define OPA(pct) ((lv_opa_t)((pct) * 255 / 100))

// Calm (the clock face): drifting, turning, breathing and twinkling take 7/4 as long. Falling doesn't change.
#define PACE(s, ms) ((uint32_t)((s)->calm ? (ms) * 7 / 4 : (ms)))

#define SKY_ANIMS 16 // the most any sky starts: rain's 10 drops

typedef struct {
    lv_anim_t a; // the one started (to find it again), or, while frozen, the copy to start again
    bool held;   // frozen: a is a copy waiting to start again
} sky_anim_t;

struct weather_sky {
    lv_obj_t *back, *front;
    tw_mood_t shown; // the sky drawn (TW_PLAIN: none)
    bool calm;
    bool frozen;
    bool lit; // the lightning is showing (its callback was called for this flash)
    void (*flash_cb)(void *user);
    void *flash_user;
    // The sun's rays: lv_line keeps a pointer to its points, so they live here.
    lv_obj_t *rays[8];
    lv_point_precise_t ray_pts[8][2];
    int ray_c, ray_r1, ray_r2; // the rays' centre in their group; radii in 1/4 px
    int anim_count;
    sky_anim_t anims[SKY_ANIMS];
};

// compose.py's units -> scene px: its viewBox -14 -22 95 88 is shown at 104 x 96.
static inline float ux(float x) { return (x + 14.f) * BOX_W / 95.f; }
static inline float uy(float y) { return (y + 22.f) * BOX_H / 88.f; }
static inline float us(float v) { return v * 1.093f; } // a length (radius, width): between 104/95 and 96/88
static inline int rnd(float v) { return (int)lroundf(v); }

// Start an animation of this sky (frozen: keep it to start on unfreeze) and note it for freeze.
void sky_start(weather_sky_t *s, lv_anim_t *a);
// lv_anim callback: background opacity (glows).
void sky_anim_bg_opa(void *obj, int32_t v);

// Plain rounded shape, flat colour at opacity opa, not clickable.
lv_obj_t *sky_blob(lv_obj_t *parent, int x, int y, int w, int h, int radius, uint32_t color, lv_opa_t opa);
// Disc of radius r (px) centred on (cx, cy) (px); discs with the same centre line up exactly.
lv_obj_t *sky_disc(lv_obj_t *parent, float cx, float cy, float r, uint32_t color, lv_opa_t opa);
// Forever drifting by dx px from where it is and back, ms each way (calm: slower).
void sky_drift(weather_sky_t *s, lv_obj_t *obj, int dx, uint32_t ms);

// compose.py's cloud(x, y, k) (units) in box, as one group (so it can drift). bottom (if not NULL): the first row
// under it.
lv_obj_t *sky_cloud(lv_obj_t *box, float x, float y, float k, uint32_t fill, uint32_t edge, int *bottom);
// compose.py's sun(x, y, r); turning: its rays turn (a full turn in 24 s; calm 42 s).
void sky_sun(weather_sky_t *s, float x, float y, float r, bool turning);
// compose.py's moon(x, y), its cut-out in the colour behind the boxes.
void sky_moon(weather_sky_t *s, float x, float y);
// compose.py's 4-point star at (x, y), size k, opacity o, twinkling ms each way (calm: slower) after delay.
void sky_star(weather_sky_t *s, float x, float y, float k, float o, uint32_t ms, uint32_t delay);

// Something falling forever in the front box: a w x h pill at x (box px) from above the box to below it over ms
// after delay, its bottom stopping at stop (box px; 96: never). parent: the front box, or a group in it at oy.
void sky_faller(weather_sky_t *s, lv_obj_t *parent, int oy, int x, int w, int h, int stop, uint32_t color, uint32_t ms,
                uint32_t delay, lv_anim_path_cb_t path);

// A drop: its centre x, the line its bottom stops at (box px), and whether it falls from behind the sky's cloud
// (showing only under it) or from above the box.
typedef struct { int16_t x, stop; bool cloud; } drop_t;
// n drops of w x h (calm: the first half), ms a fall, speeding up. cloud_bottom: the first row under the cloud the
// cloud drops come out of (0: none).
void sky_drops(weather_sky_t *s, const drop_t *t, int n, int w, int h, uint32_t ms, int cloud_bottom);

// compose.py's lightning bolt in the back box, hidden, flashing now and then (8-15 s; calm 50-70 s) and calling
// the flash callback as it lights up.
void sky_bolt(weather_sky_t *s);
