// Weather icons, built from simple shapes (no image files).
//
// Every icon is drawn inside a 104 x 96 box. All x/y below are inside that box, at that size,
// with y growing downward. Shapes created later are drawn on top, so:
//   - drops and the lightning bolt are created BEFORE their cloud: they start hidden behind it
//     and appear as they fall out underneath;
//   - the box clips its children, so a drop that falls past y 96 simply disappears.
//
// The hourly columns' (and, for now, the clock face's) are these pictures at half size and standing still
// (weather_icon_small_*): every x, y, w and h goes through S(), which halves it there.
//
// To change an icon, edit only its draw_... function.

#include "weather_icon.h"
#include "theme.h"
#include "ui_common.h"
#include "weather_anim.h"

#define ICON_W  104
#define ICON_H  96
#define DROP_Y0 40 // drops start here, hidden behind a cloud at y 0

typedef enum { WX_CLEAR, WX_PARTLY, WX_OVERCAST, WX_FOG, WX_DRIZZLE, WX_RAIN, WX_SNOW, WX_THUNDER } wx_kind_t;

// What the draw_... functions draw into: the box, at full size (shrink 1) or half (2), moving or
// still, and the colour behind it (the moon's cut-out disc must be exactly that to look like a hole).
static lv_obj_t *target;
static int shrink = 1;
static bool still;
static uint32_t behind = COLOR_CARD;

#define S(v) ((v) / shrink) // a size or position at the size being drawn

// ---------- WMO weather code -> icon ----------

static wx_kind_t kind_from_wmo(int code) {
    if (code <= 1) return WX_CLEAR;                                     // 0 clear, 1 mainly clear
    if (code == 2) return WX_PARTLY;
    if (code == 3) return WX_OVERCAST;
    if (code == 45 || code == 48) return WX_FOG;
    if (code >= 51 && code <= 57) return WX_DRIZZLE;
    if ((code >= 61 && code <= 67) || (code >= 80 && code <= 82)) return WX_RAIN;
    if ((code >= 71 && code <= 77) || code == 85 || code == 86) return WX_SNOW;
    if (code >= 95 && code <= 99) return WX_THUNDER;
    return WX_OVERCAST;                                                 // unknown code: plain cloud
}

// ---------- animation helpers (the shared ones: weather_anim.c) ----------

static void anim_set_opa(void *obj, int32_t v) { lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)v, 0); }

// Forever back and forth: from -> to -> from, eased. Drifting clouds, sun glow, twinkling stars.
// A still icon stays at from.
static void anim_bounce(lv_obj_t *obj, lv_anim_exec_xcb_t cb, int32_t from, int32_t to, uint32_t ms, uint32_t delay) {
    if (still) return;
    lv_anim_t a;
    wx_anim_bounce(&a, obj, cb, from, to, ms, delay);
    lv_anim_start(&a);
}

// Forever falling: y goes from -> to, then jumps back to from. Rain, drizzle, snow.
static void anim_fall(lv_obj_t *obj, int32_t from, int32_t to, uint32_t ms, uint32_t delay, lv_anim_path_cb_t path) {
    lv_anim_t a;
    wx_anim_fall(&a, obj, wx_anim_y, from, to, ms, delay, path);
    lv_anim_start(&a);
}

// ---------- building blocks (the invisible group box: wx_group, weather_anim.c) ----------

// The design page's cloud (pill + two puffs) in an 88 x 60 box.
static lv_obj_t *make_cloud(int x, int y, uint32_t color) {
    lv_obj_t *cloud = wx_group(target, S(x), S(y), S(88), S(60));
    //               x      y      w      h      radius            color
    make_blob(cloud, 0,     S(26), S(88), S(34), S(17),            color); // cloud_base
    make_blob(cloud, S(12), S(10), S(40), S(40), LV_RADIUS_CIRCLE, color); // cloud_a
    make_blob(cloud, S(40), 0,     S(48), S(48), LV_RADIUS_CIRCLE, color); // cloud_b
    return cloud;
}

// Crescent moon: a moon disc with a disc of the colour behind it over its top-right part.
static void make_moon(int x, int y, int d, int cut_x, int cut_y, int cut_d) {
    make_blob(target, S(x), S(y), S(d), S(d), LV_RADIUS_CIRCLE, COLOR_MOON);
    make_blob(target, S(cut_x), S(cut_y), S(cut_d), S(cut_d), LV_RADIUS_CIRCLE, behind);
}

// Small twinkling dot.
static void make_star(int x, int y, int d, uint32_t delay) {
    lv_obj_t *s = make_blob(target, S(x), S(y), S(d), S(d), LV_RADIUS_CIRCLE, COLOR_TEXT);
    anim_bounce(s, anim_set_opa, LV_OPA_COVER, LV_OPA_20, 1400, delay);
}

// n drops (or flakes) side by side from x0, dx apart, falling from behind the cloud to the bottom.
// Their start times are spread over one fall, so they don't all drop together. Still, they hang
// under the cloud, every other one lower.
static void make_drops(int n, int x0, int dx, int w, int h, uint32_t color, uint32_t ms, lv_anim_path_cb_t path) {
    for (int i = 0; i < n; i++) {
        int y = still ? 62 + (i % 2) * 14 : DROP_Y0;
        lv_obj_t *d = make_blob(target, S(x0 + i * dx), S(y), S(w), S(h), LV_RADIUS_CIRCLE, color);
        if (!still) anim_fall(d, S(DROP_Y0), S(ICON_H), ms, (uint32_t)i * ms / n, path);
    }
}

// ---------- one function per icon ----------

static void draw_clear(bool day) {
    if (day) {
        lv_obj_t *glow = make_blob(target, S(12), S(8), S(80), S(80), LV_RADIUS_CIRCLE, COLOR_SUN);
        lv_obj_set_style_opa(glow, LV_OPA_20, 0);
        anim_bounce(glow, anim_set_opa, LV_OPA_10, LV_OPA_40, 1800, 0); // breathing glow
        make_blob(target, S(22), S(18), S(60), S(60), LV_RADIUS_CIRCLE, COLOR_SUN);
    } else {
        make_moon(24, 20, 56, 42, 10, 46);
        make_star( 8, 12, 6,    0);
        make_star(90, 10, 4,  500);
        make_star(92, 70, 5,  900);
        make_star(14, 82, 4, 1300);
    }
}

// Design page layout: sun (36, 18) behind the cloud (10, 36). The cloud drifts a little.
static void draw_partly(bool day) {
    if (day) {
        make_blob(target, S(36), S(18), S(52), S(52), LV_RADIUS_CIRCLE, COLOR_SUN);
    } else {
        make_moon(44, 12, 44, 60, 6, 36);
        make_star(16, 14, 5,   0);
        make_star( 6, 34, 4, 700);
    }
    lv_obj_t *cloud = make_cloud(10, 36, COLOR_CLOUD);
    anim_bounce(cloud, wx_anim_x, S(10), S(4), 3000, 0);
}

// Two clouds drifting in opposite directions: a dark one behind, a light one in front.
static void draw_overcast(void) {
    lv_obj_t *back = make_cloud(14, 8, COLOR_MUTED);
    anim_bounce(back, wx_anim_x, S(14), S(8), 4500, 0);
    lv_obj_t *front = make_cloud(0, 34, COLOR_CLOUD);
    anim_bounce(front, wx_anim_x, 0, S(6), 3500, 0);
}

// Cloud with three fog bars sliding slowly under it.
static void draw_fog(void) {
    make_cloud(10, 0, COLOR_CLOUD);
    //                       x   y   w
    static const int bars[3][3] = {{4, 66, 80}, {20, 77, 76}, {8, 88, 64}};
    for (int i = 0; i < 3; i++) {
        lv_obj_t *bar = make_blob(target, S(bars[i][0]), S(bars[i][1]), S(bars[i][2]), S(6), LV_RADIUS_CIRCLE, COLOR_MUTED);
        anim_bounce(bar, wx_anim_x, S(bars[i][0]), S(bars[i][0] + 8), 2400 + 400 * i, 0);
    }
}

static void draw_drizzle(void) {
    make_drops(3, 30, 22, 4, 8, COLOR_RAIN, 1300, lv_anim_path_ease_in); // few, small, slow
    make_cloud(10, 0, COLOR_CLOUD);
}

static void draw_rain(void) {
    make_drops(4, 24, 20, 5, 14, COLOR_RAIN, 800, lv_anim_path_ease_in); // speed up as they fall
    make_cloud(10, 0, COLOR_CLOUD);
}

static void draw_snow(void) {
    make_drops(4, 22, 20, 8, 8, COLOR_TEXT, 2600, lv_anim_path_linear); // round flakes, slow and steady
    make_cloud(10, 0, COLOR_CLOUD);
}

// Dark cloud, two drops and a lightning bolt that flashes every few seconds (still: always lit).
static void draw_thunder(void) {
    // (must stay alive: lv_line keeps the pointer; one set per size)
    static const lv_point_precise_t bolt_points[] = {{14, 0}, {4, 20}, {14, 20}, {2, 42}};
    static const lv_point_precise_t bolt_half[] = {{7, 0}, {2, 10}, {7, 10}, {1, 21}};
    lv_obj_t *bolt = lv_line_create(target);
    lv_line_set_points(bolt, shrink == 1 ? bolt_points : bolt_half, 4);
    lv_obj_set_pos(bolt, S(46), S(44));
    lv_obj_set_style_line_width(bolt, shrink == 1 ? 5 : 3, 0);
    lv_obj_set_style_line_color(bolt, lv_color_hex(COLOR_SUN), 0);
    lv_obj_set_style_line_rounded(bolt, true, 0);

    if (!still) {
        lv_obj_set_style_opa(bolt, LV_OPA_TRANSP, 0);
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, bolt);
        lv_anim_set_exec_cb(&a, anim_set_opa);
        lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
        lv_anim_set_duration(&a, 80);           // flash on fast
        lv_anim_set_playback_duration(&a, 240); // fade out
        lv_anim_set_repeat_delay(&a, 2400);     // dark pause between flashes
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&a);
    }

    make_drops(2, 22, 58, 5, 14, COLOR_RAIN, 800, lv_anim_path_ease_in);
    make_cloud(10, 0, COLOR_MUTED);
}

// Clean box and draw the icon of code there, at that size, moving or not, on that background.
static void draw_into(lv_obj_t *box, int size_shrink, bool is_still, uint32_t bg, wx_kind_t kind, bool day) {
    lv_obj_clean(box); // delete the old shapes; LVGL stops their animations with them
    target = box;
    shrink = size_shrink;
    still = is_still;
    behind = bg;
    switch (kind) {
    case WX_CLEAR:    draw_clear(day);  break;
    case WX_PARTLY:   draw_partly(day); break;
    case WX_OVERCAST: draw_overcast();  break;
    case WX_FOG:      draw_fog();       break;
    case WX_DRIZZLE:  draw_drizzle();   break;
    case WX_RAIN:     draw_rain();      break;
    case WX_SNOW:     draw_snow();      break;
    case WX_THUNDER:  draw_thunder();   break;
    }
}

// Only clear and partly cloudy look different at night.
static bool day_for(wx_kind_t kind, bool is_day) {
    return (kind == WX_CLEAR || kind == WX_PARTLY) ? is_day : true;
}

// ---------- public ----------

lv_obj_t *weather_icon_small_create(lv_obj_t *parent) {
    return wx_group(parent, 0, 0, ICON_W / 2, ICON_H / 2);
}

void weather_icon_small_set(lv_obj_t *box, int wmo_code, bool is_day, uint32_t bg) {
    if (!box) return;
    wx_kind_t kind = kind_from_wmo(wmo_code);
    draw_into(box, 2, true, bg, kind, day_for(kind, is_day));
}
