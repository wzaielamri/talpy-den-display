// The live sky's building blocks (see weather_sky_parts.h; the skies themselves are in weather_sky.c).
//
// All x/y are scene px in the sky's 104 x 96 boxes, y growing downward; functions named after compose.py's take its
// units and turn them into px (ux, uy, us). Shapes created later are drawn on top; a box or group clips its insides.

#include "weather_sky_parts.h"
#include "theme.h"
#include "weather_anim.h"

// ---------- the sky's animations ----------

// One per object and callback, as lv_anim keeps them: a new one for the same pair (the next lightning flash) takes
// the old one's place. Every shape stands at its animation's start value already, and a copy going on after a freeze
// must not jump back, so the start value is never applied early.
void sky_start(weather_sky_t *s, lv_anim_t *a) {
    lv_anim_set_early_apply(a, false);
    int i = 0;
    while (i < s->anim_count && !(s->anims[i].a.var == a->var && s->anims[i].a.exec_cb == a->exec_cb))
        i++;
    if (i == s->anim_count) {
        if (s->anim_count == SKY_ANIMS)
            return;
        s->anim_count++;
    }
    s->anims[i].a = *a;
    s->anims[i].held = s->frozen;
    if (!s->frozen)
        lv_anim_start(a);
}

void sky_anim_bg_opa(void *obj, int32_t v) { lv_obj_set_style_bg_opa((lv_obj_t *)obj, (lv_opa_t)v, 0); }

// ---------- shapes ----------

lv_obj_t *sky_blob(lv_obj_t *parent, int x, int y, int w, int h, int radius, uint32_t color, lv_opa_t opa) {
    lv_obj_t *b = lv_obj_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_radius(b, radius, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(b, opa, 0);
    lv_obj_remove_flag(b, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE); // a tap goes to the card under it
    return b;
}

// An odd diameter around the pixel under the centre.
lv_obj_t *sky_disc(lv_obj_t *parent, float cx, float cy, float r, uint32_t color, lv_opa_t opa) {
    const int rr = rnd(r);
    return sky_blob(parent, (int)floorf(cx) - rr, (int)floorf(cy) - rr, 2 * rr + 1, 2 * rr + 1, LV_RADIUS_CIRCLE,
                    color, opa);
}

void sky_drift(weather_sky_t *s, lv_obj_t *obj, int dx, uint32_t ms) {
    const int32_t x = lv_obj_get_style_x(obj, LV_PART_MAIN); // (the set x: coords aren't laid out yet)
    lv_anim_t a;
    wx_anim_bounce(&a, obj, wx_anim_x, x, x + dx, PACE(s, ms), 0);
    sky_start(s, &a);
}

// Flat bottom, bumpy top: three puffs on a rounded base, every shape first 0.7 units (1 px) bigger in the edge colour,
// then in the fill colour.
lv_obj_t *sky_cloud(lv_obj_t *box, float x, float y, float k, uint32_t fill, uint32_t edge, int *bottom) {
    //                                dx    dy     r     (units, times k, from (x, y))
    static const float puff[3][3] = {{-6, 0.5f, 4.2f}, {0.5f, -2.6f, 6}, {7, 0, 4.4f}};
    // the group: base x - 10.5k .. x + 11.5k, top puff's top y - 8.6k .. base's bottom y + 5k, 2 px spare (edge, rounding)
    const int gx = (int)floorf(ux(x - 10.5f * k)) - 2, gy = (int)floorf(uy(y - 8.6f * k)) - 2;
    lv_obj_t *g = wx_group(box, gx, gy, (int)ceilf(ux(x + 11.5f * k)) + 2 - gx, (int)ceilf(uy(y + 5 * k)) + 2 - gy);
    // the base: x - 10.5k, y - 0.5k, 22k x 5.5k, round ends
    const int bx = rnd(ux(x - 10.5f * k)) - gx, by = rnd(uy(y - 0.5f * k)) - gy;
    const int bw = rnd(us(22 * k)), bh = rnd(us(5.5f * k));
    for (int grow = 1; grow >= 0; grow--) {
        const uint32_t c = grow ? edge : fill;
        for (int i = 0; i < 3; i++)
            sky_disc(g, ux(x + puff[i][0] * k) - gx, uy(y + puff[i][1] * k) - gy, us(puff[i][2] * k) + grow, c,
                     LV_OPA_COVER);
        sky_blob(g, bx - grow, by - grow, bw + 2 * grow, bh + 2 * grow, LV_RADIUS_CIRCLE, c, LV_OPA_COVER);
    }
    if (bottom)
        *bottom = gy + by + bh + 1;
    return g;
}

// ---------- sun ----------

// The 8 rays at deg (and every 45 degrees from it).
static void place_rays(weather_sky_t *s, int32_t deg) {
    for (int i = 0; i < 8; i++) {
        const int32_t c = lv_trigo_cos((int16_t)(deg + 45 * i)), sn = lv_trigo_sin((int16_t)(deg + 45 * i));
        // radius (1/4 px) x trig (<< 15) -> px, rounded
        s->ray_pts[i][0].x = s->ray_c + ((s->ray_r1 * c + (1 << 16)) >> 17);
        s->ray_pts[i][0].y = s->ray_c + ((s->ray_r1 * sn + (1 << 16)) >> 17);
        s->ray_pts[i][1].x = s->ray_c + ((s->ray_r2 * c + (1 << 16)) >> 17);
        s->ray_pts[i][1].y = s->ray_c + ((s->ray_r2 * sn + (1 << 16)) >> 17);
        lv_line_set_points(s->rays[i], s->ray_pts[i], 2);
    }
}

static void anim_rays(void *group, int32_t deg) { place_rays(lv_obj_get_user_data((lv_obj_t *)group), deg); }

// A glow (r + 6, 18 %), 8 rays from r + 3 to r + 7.5 (2.4 units wide, round ends), the disc. Turning: 45 degrees in
// 3 s, over and over (the picture repeats every 45).
void sky_sun(weather_sky_t *s, float x, float y, float r, bool turning) {
    const float cx = ux(x), cy = uy(y);
    sky_disc(s->back, cx, cy, us(r + 6), COLOR_SUN, OPA(18));
    const int half = (int)ceilf(us(r + 7.5f)) + 2; // the rays' group: centred on the sun, a ray's end and width spare
    const int size = 2 * half + 1;
    lv_obj_t *g = wx_group(s->back, (int)floorf(cx) - half, (int)floorf(cy) - half, size, size);
    lv_obj_set_user_data(g, s);
    s->ray_c = half;
    s->ray_r1 = rnd(us(r + 3) * 4);
    s->ray_r2 = rnd(us(r + 7.5f) * 4);
    for (int i = 0; i < 8; i++) {
        lv_obj_t *l = lv_line_create(g);
        lv_obj_remove_style_all(l);
        lv_obj_set_size(l, size, size); // fixed: turning never lays anything out again
        lv_obj_set_style_line_width(l, 3, 0);
        lv_obj_set_style_line_color(l, lv_color_hex(COLOR_SUN), 0);
        lv_obj_set_style_line_rounded(l, true, 0);
        lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        s->rays[i] = l;
    }
    place_rays(s, 0);
    if (turning) {
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, g);
        lv_anim_set_exec_cb(&a, anim_rays);
        lv_anim_set_values(&a, 0, 45);
        lv_anim_set_duration(&a, PACE(s, 3000));
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        sky_start(s, &a);
    }
    sky_disc(s->back, cx, cy, us(r), COLOR_SUN, LV_OPA_COVER);
}

// ---------- moon and stars ----------

// The colour behind the boxes (the card's, the clock face's): the first opaque background up from obj.
static uint32_t color_behind(lv_obj_t *obj) {
    for (; obj; obj = lv_obj_get_parent(obj))
        if (lv_obj_get_style_bg_opa(obj, LV_PART_MAIN) >= LV_OPA_COVER)
            return lv_color_to_u32(lv_obj_get_style_bg_color(obj, LV_PART_MAIN)) & 0xFFFFFF;
    return COLOR_CARD;
}

// A glow (r 15, 12 %), the disc (r 10.5) and over its top right a disc (r 9, 5.5 right and 4.5 up) in the colour
// behind the sky, which makes it a crescent (and takes a bite of the glow, as in the mockup).
void sky_moon(weather_sky_t *s, float x, float y) {
    sky_disc(s->back, ux(x), uy(y), us(15), COLOR_MOON, OPA(12));
    sky_disc(s->back, ux(x), uy(y), us(10.5f), COLOR_MOON, LV_OPA_COVER);
    sky_disc(s->back, ux(x + 5.5f), uy(y - 4.5f), us(9), color_behind(s->back), LV_OPA_COVER);
}

// A star's parts (a group's children) all at opacity v.
static void anim_star(void *obj, int32_t v) {
    const uint32_t n = lv_obj_get_child_count((lv_obj_t *)obj);
    for (uint32_t i = 0; i < n; i++)
        lv_obj_set_style_bg_opa(lv_obj_get_child((lv_obj_t *)obj, (int32_t)i), (lv_opa_t)v, 0);
}

// Arms 3.4k units long: a cross of two 2 px pills (brighter where they cross), twinkling from o down to a quarter of
// it and back.
void sky_star(weather_sky_t *s, float x, float y, float k, float o, uint32_t ms, uint32_t delay) {
    const int half = rnd(us(3.4f * k)), cx = rnd(ux(x)), cy = rnd(uy(y));
    const lv_opa_t opa = (lv_opa_t)rnd(o * 255);
    lv_obj_t *g = wx_group(s->back, cx - half, cy - half, 2 * half, 2 * half);
    sky_blob(g, half - 1, 0, 2, 2 * half, LV_RADIUS_CIRCLE, COLOR_TEXT, opa);
    sky_blob(g, 0, half - 1, 2 * half, 2, LV_RADIUS_CIRCLE, COLOR_TEXT, opa);
    lv_anim_t a;
    wx_anim_bounce(&a, g, anim_star, opa, opa / 4, PACE(s, ms), delay);
    sky_start(s, &a);
}

// ---------- drops and flakes ----------

// A falling drop or flake: its y follows the fall until its bottom reaches its stop line (its top then: kept in its
// user data), then it waits out of sight above its box until it falls again.
#define PARKED (-64)
static void anim_fall(void *obj, int32_t v) {
    const int32_t stop_top = (int32_t)(intptr_t)lv_obj_get_user_data((lv_obj_t *)obj);
    lv_obj_set_y((lv_obj_t *)obj, v <= stop_top ? v : PARKED);
}

void sky_faller(weather_sky_t *s, lv_obj_t *parent, int oy, int x, int w, int h, int stop, uint32_t color, uint32_t ms,
                uint32_t delay, lv_anim_path_cb_t path) {
    lv_obj_t *d = sky_blob(parent, x, -h - oy, w, h, LV_RADIUS_CIRCLE, color, LV_OPA_COVER);
    lv_obj_set_user_data(d, (void *)(intptr_t)(stop - h + 1 - oy)); // its top when its bottom is on the stop line
    lv_anim_t a;
    wx_anim_fall(&a, d, anim_fall, -h - oy, BOX_H - oy, ms, delay, path);
    sky_start(s, &a);
}

// The cloud drops fall in a group under the cloud, which hides them for the first part of their fall. Start times
// spread over one fall, so they don't all fall together.
void sky_drops(weather_sky_t *s, const drop_t *t, int n, int w, int h, uint32_t ms, int cloud_bottom) {
    if (s->calm)
        n = (n + 1) / 2;
    lv_obj_t *under = cloud_bottom > 0 ? wx_group(s->front, 0, cloud_bottom, BOX_W, BOX_H - cloud_bottom) : NULL;
    for (int i = 0; i < n; i++) {
        const bool from_cloud = t[i].cloud && under;
        sky_faller(s, from_cloud ? under : s->front, from_cloud ? cloud_bottom : 0, t[i].x - w / 2, w, h,
                   s->talpy_out ? t[i].stop : t[i].burrow, DROP_COLOR, ms, (uint32_t)i * ms / (uint32_t)n,
                   lv_anim_path_ease_in);
    }
}

// ---------- lightning ----------

// compose.py's BOLT, a filled polygon, made once into an alpha mask (4 x 4 samples a pixel, so its edges are
// smooth) and shown as an A8 image tinted COLOR_SUN. Its box: ux(54 .. 75) = 74.4 .. 97.4, uy(0 .. 38) = 24 .. 65.5.
#define BOLT_X 74
#define BOLT_Y 24
#define BOLT_W 24
#define BOLT_H 42

static bool inside(const float *vx, const float *vy, int n, float x, float y) {
    bool in = false;
    for (int i = 0, j = n - 1; i < n; j = i++)
        if ((vy[i] > y) != (vy[j] > y) && x < (vx[j] - vx[i]) * (y - vy[i]) / (vy[j] - vy[i]) + vx[i])
            in = !in;
    return in;
}

static const lv_image_dsc_t *bolt_image(void) {
    static uint8_t alpha[BOLT_W * BOLT_H]; // one picture for every sky, made on first use
    static lv_image_dsc_t dsc;
    if (dsc.data)
        return &dsc;
    static const int8_t poly[7][2] = {{64, 0}, {54, 20}, {63, 20}, {56, 38}, {75, 11}, {66, 11}, {73, 0}};
    float vx[7], vy[7];
    for (int i = 0; i < 7; i++) {
        vx[i] = ux(poly[i][0]) - BOLT_X;
        vy[i] = uy(poly[i][1]) - BOLT_Y;
    }
    for (int py = 0; py < BOLT_H; py++)
        for (int px = 0; px < BOLT_W; px++) {
            int hits = 0;
            for (int sy = 0; sy < 4; sy++)
                for (int sx = 0; sx < 4; sx++)
                    hits += inside(vx, vy, 7, px + (sx + 0.5f) / 4, py + (sy + 0.5f) / 4);
            alpha[py * BOLT_W + px] = (uint8_t)(hits * 255 / 16);
        }
    dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    dsc.header.cf = LV_COLOR_FORMAT_A8;
    dsc.header.w = BOLT_W;
    dsc.header.h = BOLT_H;
    dsc.header.stride = BOLT_W;
    dsc.data_size = sizeof alpha;
    dsc.data = alpha;
    return &dsc;
}

// The bolt's opacity; the flash callback as it lights up (here, not in a start callback: from here the callback may
// change or freeze the sky, lv_anim allows that).
static void anim_flash(void *bolt, int32_t v) {
    weather_sky_t *s = lv_obj_get_user_data((lv_obj_t *)bolt);
    lv_obj_set_style_image_opa((lv_obj_t *)bolt, (lv_opa_t)v, 0);
    if (v == 0) {
        s->lit = false;
    } else if (!s->lit) {
        s->lit = true;
        if (s->flash_cb)
            s->flash_cb(s->flash_user);
    }
}

static void flash_next(weather_sky_t *s, lv_obj_t *bolt);
static void flash_done(lv_anim_t *a) { flash_next(lv_anim_get_user_data(a), a->var); }

// The next flash, after a random wait: on in 60 ms, held 120 ms, faded out in 300 ms.
static void flash_next(weather_sky_t *s, lv_obj_t *bolt) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, bolt);
    lv_anim_set_exec_cb(&a, anim_flash);
    lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_duration(&a, 60);
    lv_anim_set_playback_delay(&a, 120);
    lv_anim_set_playback_duration(&a, 300);
    lv_anim_set_delay(&a, s->calm ? lv_rand(50000, 70000) : lv_rand(8000, 15000));
    lv_anim_set_user_data(&a, s);
    lv_anim_set_completed_cb(&a, flash_done);
    sky_start(s, &a);
}

void sky_bolt(weather_sky_t *s) {
    lv_obj_t *bolt = lv_image_create(s->back);
    lv_obj_remove_style_all(bolt);
    lv_obj_remove_flag(bolt, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_image_set_src(bolt, bolt_image());
    lv_obj_set_pos(bolt, BOLT_X, BOLT_Y);
    lv_obj_set_size(bolt, BOLT_W, BOLT_H);
    lv_obj_set_style_image_recolor(bolt, lv_color_hex(COLOR_SUN), 0);
    lv_obj_set_style_image_recolor_opa(bolt, LV_OPA_COVER, 0);
    lv_obj_set_style_image_opa(bolt, LV_OPA_TRANSP, 0);
    lv_obj_set_user_data(bolt, s);
    flash_next(s, bolt);
}
