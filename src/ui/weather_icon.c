// Weather icons, built from simple shapes (no image files): the hourly columns' small pictures, standing still.
//
// Every icon is drawn as if inside a 104 x 96 box, and shown at half size (52 x 48): the x, y, w and h below are
// at full size (the design page's), and every one goes through S(), which halves it. y grows downward. Shapes
// created later are drawn on top, so the drops and the lightning bolt, created BEFORE their cloud, show from under
// it (the cloud hides the bolt's top).
//
// To change an icon, edit only its draw_... function.

#include "weather_icon.h"
#include "theme.h"
#include "ui_common.h"
#include "weather_anim.h"

#define ICON_W 104
#define ICON_H 96

typedef enum { WX_CLEAR, WX_PARTLY, WX_OVERCAST, WX_FOG, WX_DRIZZLE, WX_RAIN, WX_SNOW, WX_THUNDER } wx_kind_t;

// What the draw_... functions draw into: the box, and the colour behind it (the moon's cut-out disc must be exactly
// that to look like a hole).
static lv_obj_t *target;
static uint32_t behind = COLOR_CARD;

#define S(v) ((v) / 2) // a size or position at half size

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

// Small dot.
static void make_star(int x, int y, int d) {
    make_blob(target, S(x), S(y), S(d), S(d), LV_RADIUS_CIRCLE, COLOR_TEXT);
}

// n drops (or flakes) side by side from x0, dx apart, hanging under the cloud, every other one lower.
static void make_drops(int n, int x0, int dx, int w, int h, uint32_t color) {
    for (int i = 0; i < n; i++)
        make_blob(target, S(x0 + i * dx), S(62 + (i % 2) * 14), S(w), S(h), LV_RADIUS_CIRCLE, color);
}

// ---------- one function per icon ----------

static void draw_clear(bool day) {
    if (day) {
        lv_obj_t *glow = make_blob(target, S(12), S(8), S(80), S(80), LV_RADIUS_CIRCLE, COLOR_SUN);
        lv_obj_set_style_opa(glow, LV_OPA_20, 0);
        make_blob(target, S(22), S(18), S(60), S(60), LV_RADIUS_CIRCLE, COLOR_SUN);
    } else {
        make_moon(24, 20, 56, 42, 10, 46);
        make_star( 8, 12, 6);
        make_star(90, 10, 4);
        make_star(92, 70, 5);
        make_star(14, 82, 4);
    }
}

// Design page layout: sun (36, 18) behind the cloud (10, 36).
static void draw_partly(bool day) {
    if (day) {
        make_blob(target, S(36), S(18), S(52), S(52), LV_RADIUS_CIRCLE, COLOR_SUN);
    } else {
        make_moon(44, 12, 44, 60, 6, 36);
        make_star(16, 14, 5);
        make_star( 6, 34, 4);
    }
    make_cloud(10, 36, COLOR_CLOUD);
}

// Two clouds: a dark one behind, a light one in front.
static void draw_overcast(void) {
    make_cloud(14, 8, COLOR_MUTED);
    make_cloud(0, 34, COLOR_CLOUD);
}

// Cloud with three fog bars under it.
static void draw_fog(void) {
    make_cloud(10, 0, COLOR_CLOUD);
    //                       x   y   w
    static const int bars[3][3] = {{4, 66, 80}, {20, 77, 76}, {8, 88, 64}};
    for (int i = 0; i < 3; i++)
        make_blob(target, S(bars[i][0]), S(bars[i][1]), S(bars[i][2]), S(6), LV_RADIUS_CIRCLE, COLOR_MUTED);
}

static void draw_drizzle(void) {
    make_drops(3, 30, 22, 4, 8, COLOR_RAIN); // few, small
    make_cloud(10, 0, COLOR_CLOUD);
}

static void draw_rain(void) {
    make_drops(4, 24, 20, 5, 14, COLOR_RAIN);
    make_cloud(10, 0, COLOR_CLOUD);
}

static void draw_snow(void) {
    make_drops(4, 22, 20, 8, 8, COLOR_TEXT); // round flakes
    make_cloud(10, 0, COLOR_CLOUD);
}

// Dark cloud, two drops and a lightning bolt, lit.
static void draw_thunder(void) {
    // (must stay alive: lv_line keeps the pointer; at half size already)
    static const lv_point_precise_t bolt_points[] = {{7, 0}, {2, 10}, {7, 10}, {1, 21}};
    lv_obj_t *bolt = lv_line_create(target);
    lv_line_set_points(bolt, bolt_points, 4);
    lv_obj_set_pos(bolt, S(46), S(44));
    lv_obj_set_style_line_width(bolt, 3, 0);
    lv_obj_set_style_line_color(bolt, lv_color_hex(COLOR_SUN), 0);
    lv_obj_set_style_line_rounded(bolt, true, 0);

    make_drops(2, 22, 58, 5, 14, COLOR_RAIN);
    make_cloud(10, 0, COLOR_MUTED);
}

// Clean box and draw the icon of kind there, on that background.
static void draw_into(lv_obj_t *box, uint32_t bg, wx_kind_t kind, bool day) {
    lv_obj_clean(box); // delete the old shapes
    target = box;
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
    draw_into(box, bg, kind, day_for(kind, is_day));
}
