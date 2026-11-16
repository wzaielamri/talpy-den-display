# Weather Talpy Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The weather card's icon and the clock face's weather become an animated weather Talpy (Talpy with a prop, in front of a live sky), which follows the hourly forecast with Talpy hidden in his burrow while the PC is away.

**Architecture:** Talpy and his props are pre-rendered by the existing frame tool into a second generated set (`TALPY_WEATHER_SET`) and played by the existing player and widget, generalized to any number of moods. The sky is LVGL shapes animated by a new per-instance module, built from `weather_icon.c`'s shapes at the mockups' sizes. A pure-C rule picks what to show from the PC's state, the last weather and the hourly forecast; app glue feeds it and pushes the result to the card and the clock face.

**Tech Stack:** C (ESP32-S3, LVGL 9.1, PlatformIO pioarduino), host gcc native tests (`test/native/`), node (Talpy Den's code), rsvg-convert, Python 3 + Pillow.

**Spec:** `docs/superpowers/specs/2026-11-16-weather-talpy-design.md` (mockups and their sources in `docs/superpowers/specs/2026-11-16-weather-talpy-mockup/`)

## Global Constraints

- Talpy Den is not edited; its code is used (`/home/wadhah.zai/repos/talpy-den`, commit `6574f528`).
- The banner Talpy, the alerts and the hourly view behave exactly as today; every existing native test still passes.
- `pio run` needs nothing new; generated `src/ui/talpy_data.c` and `src/ui/talpy_weather_data.c` are committed; both are byte-identical when regenerated on the same machine.
- Weather frames ≤ 800,000 bytes compressed (the tool fails above); firmware < 4,194,304 bytes (app partition unchanged).
- Weather scene box: 104 × 96 px at the weather card's top right (where `weather_icon_create(parent, 140, 0)` puts the icon today). Mockup box units map to it as `px = (x + 14) × 104 / 95`, `py = (y + 22) × 96 / 88` (viewBox `-14 -22 95 88` in `compose.py`).
- Talpy scale for the weather set: 1.1 px per box unit (rsvg width 158 for Talpy Den's `-40 -40 144 144` box).
- Props and their shapes come from the mockup's `render.mjs` (sunglasses, umbrella, rain hat, beanie, scarf; Talpy Den's mug); sky sizes and colours from `compose.py` (round 3): day clouds `#eef1f8` edge `#c4cbdc`, overcast `#C9D2DD` edge `#9aa5b5`, storm `#5D6672` edge `#454c57`, night cloud `#8C96A3` edge `#6b7480`, sun `#FFCF5A`, drops `#7cc4f2`, moon `#E6E0C8`, snow `#ffffff`, stars `#E9EDF2`, fog `#C9D2DD` at 42 %, lamp beam `#ffd86b`.
- Talpy's lamp is Talpy Den's `idle` (warm yellow) in every weather picture.
- Calm: the weather Talpy pauses longer than the banner's (pauses 4000–9000 ms); the clock face's instance is slower still (pace 3) with a calm sky (half the drops/flakes at normal falling speed, lightning about once a minute).
- `motion_hold`: the card's weather Talpy (Talpy and sky) freezes like today's icon; the clock face's runs only while the clock shows.
- Decode buffers from `heap_caps_malloc` (internal first), never `lv_malloc`.
- Commit messages: plain, no Claude/Anthropic attribution lines.

## Review Focus

- The forecast hour at its edges: `now == at` is in that hour, `now == at + 3600` is not (the next entry, or none). → Task 3 `test_forecast_hour_edges`.
- The board's clock not set yet while offline: no forecast, Talpy hidden, no sky (never a random hour). → Task 3 `test_offline_clock_unknown`.
- The PC coming back: the very next view is the live weather with Talpy out, even if a forecast hour exists. → Task 3 `test_back_online_goes_live`.
- A slow-paced clock-face Talpy (pace 3, pauses up to 27 s) must not be treated as overdue by the player's 30-day guard. → Task 1 `test_slow_pace_not_overdue`.
- An unknown WMO code shows the overcast picture and sky, not nothing. → Task 3 `test_unknown_code_overcast`.

---

## File Structure

| File | Responsibility |
|---|---|
| `src/ui/talpy_set.h` (modify) | Set type for any number of moods; both sets' mood enums (`talpy_mood_t`, `tw_mood_t`); `TALPY_WEATHER_SET`. |
| `src/ui/talpy_player.h/.c` (modify) | Moods by index, `play_once`, pace. |
| `tools/talpy_draw.mjs`, `tools/make_talpy_frames.py` (modify) | `--set weather`: props, clips, scale, placement; new set layout. |
| `src/ui/talpy_data.c` (regenerated), `src/ui/talpy_weather_data.c` (new, generated) | The two sets. |
| `src/ui/weather_view.h/.c` (new) | Pure C: WMO → weather mood; what to show (live / forecast / none). |
| `src/ui/weather_sky.h/.c` (new) | The live sky: per-instance LVGL shapes and animations, calm option, flash callback. |
| `src/ui/talpy.h/.c` (modify) | Widget takes a set; mood by index; pace; play once. |
| `src/ui/weather_talpy.h/.c` (new) | The 104 × 96 scene: sky back, Talpy, sky front. |
| `src/app/weather_state.h/.c` (new) | Keeps the PC's state, the last weather and hourly; recomputes the view; pushes it to card and clock. |
| `src/ui/card_weather.c/.h`, `src/app/serial_link.cpp`, `src/ui/motion.c`, `src/ui/weather_icon.c/.h` (modify) | Card uses the weather Talpy and the view; old card icon code goes (hourly icons stay). |
| `src/ui/clock_face.c/.h`, `src/ui/card_jobs.c/.h` (modify) | Clock face: calm weather Talpy, temperature from the view, chat line. |
| `test/native/test_*.c`, `test/native/run.sh` (modify/new) | Native tests. |

---

### Task 1: Set type for any number of moods; player play-once and pace

**Files:**
- Modify: `src/ui/talpy_set.h`, `src/ui/talpy_player.h`, `src/ui/talpy_player.c`, `tools/make_talpy_frames.py` (emit the new layout), `src/ui/talpy_data.c` (regenerate), `test/native/test_player.c`, `test/native/test_data.c`, `src/ui/talpy.c` (compile fixes only: `finished_clip` → `react_clip`), `src/ui/alert.c`/`card_jobs.c` only if they need compile fixes

**Interfaces:**
- Produces, in `talpy_set.h`:

```c
typedef enum { TALPY_LOADING, TALPY_NEEDS, TALPY_WORKING, TALPY_DONE, TALPY_MOODS } talpy_mood_t;   // banner set
typedef enum { TW_CLEAR_DAY, TW_PARTLY_DAY, TW_OVERCAST, TW_FOG, TW_DRIZZLE, TW_RAIN, TW_SNOW, TW_THUNDER,
               TW_CLEAR_NIGHT, TW_PARTLY_NIGHT, TW_PLAIN, TW_HIDDEN, TW_MOODS } tw_mood_t;          // weather set
typedef struct {
    ...                                   // data, data_size, w, h, frames, steps, clips, picks as today
    const talpy_mood_def_t *moods; uint16_t mood_count;   // was moods[TALPY_MOODS]
    uint16_t react_clip;                  // was finished_clip: banner "finished", weather the thunder duck
    uint16_t sleep_still;                 // banner only (removed in Task 7); weather set: 0xFFFF
    int16_t x, y;                         // where the canvas sits in its scene box (banner 0, 0)
} talpy_set_t;
extern const talpy_set_t TALPY_SET;
extern const talpy_set_t TALPY_WEATHER_SET;   // defined in Task 2 (declare now)
```
- Produces, in `talpy_player.h`: `p->mood` becomes `uint16_t`; `void talpy_player_set_mood(talpy_player_t *p, uint16_t mood, uint32_t now_ms);` `bool talpy_player_play_once(talpy_player_t *p, uint16_t clip, uint32_t now_ms);` (plays a clip once, then rests in the mood; a mood change stops it; returns false if `clip >= clip_count`) — `talpy_player_react` stays as today (react_clip, refused in `TALPY_NEEDS`; banner only) and calls it; `void talpy_player_set_pace(talpy_player_t *p, uint8_t pace);` (1..8, clamped; drawn pauses × pace; default 1). `due()`'s "too far ahead means overdue" bound becomes `0xFFFF * 8` ms.
- A mood with `picks == 0` rests forever (rest clip loops, no pick).

- [ ] **Step 1: Write the failing tests** in `test/native/test_player.c` (fake set adapted to `moods` pointer + `mood_count`):
  - `test_play_once_plays_then_rests`: in WORKING, `play_once(clip B)` → frame 7, then working's rest frame 100 ms later.
  - `test_play_once_bad_clip`: `play_once(99)` → false, frame unchanged.
  - `test_mood_without_picks_rests`: a mood with `picks = 0`: over 60 s of 10 ms ticks the frame stays its rest frame.
  - `test_slow_pace_not_overdue`: pace 3 on a mood with pauses 9000..9000: no clip before `now + 27000`; one by `now + 27100`.
  - `test_pace_clamped`: pace 0 behaves as 1, pace 20 as 8 (pause 1000..1000 → clip at 1000 resp. 8000).
  - Keep every existing test (react ignored while NEEDS, freeze, wrap, 30-day) passing.
- [ ] **Step 2: Run** `sh test/native/run.sh` → compile errors / failures.
- [ ] **Step 3: Implement** the set and player changes; make `tools/make_talpy_frames.py` emit `static const talpy_mood_def_t MOODS[]` + `.moods = MOODS, .mood_count = N`, `.react_clip`, `.x = 0, .y = 0`; regenerate `python3 tools/make_talpy_frames.py ../talpy-den src/ui/talpy_data.c` (Talpy Den path from this worktree: `/home/wadhah.zai/repos/talpy-den`); update `test_data.c` for the new fields (per-set mood loop; `picks >= 1` stays required for the banner set).
- [ ] **Step 4: Verify:** `sh test/native/run.sh` all pass (test_data's checksums prove the banner frames are unchanged); `python3 -m unittest tools/test_make_talpy_frames.py`; `~/.platformio/penv/bin/pio run` SUCCESS, no new warnings.
- [ ] **Step 5: Commit** `git commit -m "Talpy sets: any number of moods; player play-once and pace"`

---

### Task 2: The weather frames

**Files:**
- Modify: `tools/talpy_draw.mjs`, `tools/make_talpy_frames.py`, `tools/test_make_talpy_frames.py` (only if a pure helper is added)
- Create (generated): `src/ui/talpy_weather_data.c`
- Modify: `test/native/test_data.c`, `test/native/run.sh`

**Interfaces:**
- Consumes: Task 1's set layout and `tw_mood_t`.
- Command: `python3 tools/make_talpy_frames.py ../talpy-den src/ui/talpy_weather_data.c --set weather` (default `--set banner` unchanged); the driver passes the set to `node tools/talpy_draw.mjs <den> <outdir> weather`.
- Produces: `TALPY_WEATHER_SET` with `mood_count = TW_MOODS`, moods in `tw_mood_t` order, `react_clip` = the thunder duck, `x, y` = the canvas's place in the 104 × 96 scene box.

The weather set (Talpy only, no sky; lamp `idle`; Talpy Den particles that are weather — `shiver`'s snow — left out; `sleep`'s "z" kept):

| Mood | Rest (looped) | Clips (equal weights) | Props (from `render.mjs`) |
|---|---|---|---|
| CLEAR_DAY, PARTLY_DAY | still: eyes open, mouth smile, depth half | `glint` (glare sweeps across the lenses, ~4 frames), `sway` (Talpy Den body `sway`, ~1.5 s) | sunglasses |
| OVERCAST | prop mug, arms hold, depth high, eyes happy, mouth small; loop of the mug's steam (6 × 367 ms) | `sip` (Talpy Den `coffee`'s sip steps) | mug |
| FOG | eyes squint, mouth flat, depth half | none | — |
| DRIZZLE | eyes open, mouth smile | `look` (Talpy Den `lookaround`) | rain hat |
| RAIN | eyes open, mouth smile, right arm at 160° holding the umbrella | `twirl` (canopy turns once about its shaft, ~8 frames) | umbrella |
| SNOW | eyes happy, mouth smile | `shiver` (Talpy Den, its snow left out) | beanie + scarf |
| THUNDER | depth peek, eyes wide, mouth o | none; react clip `duck`: depth hidden then back to peek (~2 s) | — |
| CLEAR_NIGHT, PARTLY_NIGHT | eyes closed, mouth small, depth low, plume droop | `sleep` (Talpy Den, with its "z") | — |
| PLAIN | eyes open, mouth smile, depth half | none | — |
| HIDDEN | depth hidden (only the lamp shows) | none | — |

Pauses for every weather mood: 4000–9000 ms. Scale 1.1 px per unit (rsvg 158 × 158); canvas = union box of all weather frames; `x, y` = canvas top-left minus the scene box's top-left in raster px (`(−14 + 40) × 1.1`, `(−22 + 40) × 1.1`), rounded. Fail if the canvas leaves the 104 × 96 box or data > 800,000 bytes.

- [ ] **Step 1: Write the failing tests** in `test/native/test_data.c` for `TALPY_WEATHER_SET`: `test_weather_frames_decode` (each frame decodes, FNV matches), `test_weather_tables` (`mood_count == TW_MOODS`; every rest clip/pick in range; FOG, THUNDER, PLAIN, HIDDEN have `picks == 0`; the others ≥ 1; `react_clip < clip_count`; `data_size <= 800000`; `x >= 0 && y >= 0 && x + w <= 104 && y + h <= 96`); add `talpy_weather_data.c` to run.sh's test_data sources.
- [ ] **Step 2: Run** `sh test/native/run.sh` → link error (no `TALPY_WEATHER_SET`).
- [ ] **Step 3: Implement** the weather set in the tools; generate `src/ui/talpy_weather_data.c`; open the contact sheet and compare every picture with `docs/superpowers/specs/2026-11-16-weather-talpy-mockup/mockups.png` (Talpy and props; the sky isn't in the frames).
- [ ] **Step 4: Verify:** run the generator twice to two paths, `cmp` identical; regenerate the banner set and confirm `git diff --stat src/ui/talpy_data.c` is empty; `sh test/native/run.sh`; `pio run` SUCCESS (report firmware.bin size; it won't include the weather data until something references it — Task 6 measures).
- [ ] **Step 5: Commit** `git commit -m "Talpy: weather frames from Talpy Den's drawing code"`

---

### Task 3: What to show (pure C)

**Files:**
- Create: `src/ui/weather_view.h`, `src/ui/weather_view.c`, `test/native/test_weather_view.c`; Modify: `test/native/run.sh`

**Interfaces:**
- Consumes: `tw_mood_t` (Task 1).
- Produces:

```c
typedef struct { time_t at; int temp; int code; bool is_day; } wv_hour_t;
typedef struct {
    bool pc_online;
    bool have_weather; int code; bool is_day; int temp;   // the last weather message
    const wv_hour_t *hours; int hour_count;              // the last hourly message (any order)
    bool clock_known; time_t now;                        // the board's clock
} wv_input_t;
typedef enum { WV_LIVE, WV_FORECAST, WV_NONE } wv_source_t;
typedef struct {
    tw_mood_t talpy;     // TW_PLAIN, TW_HIDDEN or a weather
    tw_mood_t sky;       // a weather, or TW_PLAIN: no sky
    wv_source_t source;
    bool has_temp; int temp;
    int code;            // WV_LIVE / WV_FORECAST: for the condition text
    time_t forecast_at;  // WV_FORECAST: the hour shown
} wv_view_t;
tw_mood_t weather_mood(int wmo_code, bool is_day);
wv_view_t weather_view(const wv_input_t *in);
```

- [ ] **Step 1: Write the failing tests:**
  - `test_mood_mapping`: 0, 1 → CLEAR_DAY / CLEAR_NIGHT by `is_day`; 2 → PARTLY_DAY / PARTLY_NIGHT; 3 → OVERCAST (day or night); 45, 48 → FOG; 51, 57 → DRIZZLE; 61, 67, 80, 82 → RAIN; 71, 77, 85, 86 → SNOW; 95, 99 → THUNDER.
  - `test_unknown_code_overcast`: 42 and 100 → OVERCAST.
  - `test_online_live`: online, weather (code 61, day, 12) → talpy RAIN, sky RAIN, WV_LIVE, temp 12.
  - `test_online_no_weather`: → talpy PLAIN, sky PLAIN, WV_NONE, `has_temp` false.
  - `test_offline_forecast`: offline, hours at T, T+3600 (codes 0 day / 61), now T+3700 → talpy HIDDEN, sky RAIN, WV_FORECAST, temp of the second hour, `forecast_at` T+3600.
  - `test_forecast_hour_edges`: now == T → the first hour; now == T+3600 → the second; now == T+7200 with only two hours → WV_NONE.
  - `test_offline_no_forecast`: offline, no hours (or all too old) → talpy HIDDEN, sky PLAIN, WV_NONE, no temp.
  - `test_offline_clock_unknown`: offline, hours present, `clock_known` false → talpy HIDDEN, sky PLAIN, WV_NONE.
  - `test_back_online_goes_live`: the offline-forecast input with `pc_online` true and a weather → WV_LIVE, talpy out.
- [ ] **Step 2: Run** `sh test/native/run.sh` → failures.
- [ ] **Step 3: Implement** `weather_view.c` (pure C; the mapping mirrors `kind_from_wmo` in `weather_icon.c` plus day/night).
- [ ] **Step 4: Run** → all pass.
- [ ] **Step 5: Commit** `git commit -m "Weather: what to show, live, forecast or none"`

---

### Task 4: The live sky

**Files:**
- Create: `src/ui/weather_sky.h`, `src/ui/weather_sky.c`

**Interfaces:**
- Consumes: `tw_mood_t` (Task 1).
- Produces:

```c
typedef struct weather_sky weather_sky_t;                          // per instance (card, clock face)
weather_sky_t *weather_sky_create(lv_obj_t *back, lv_obj_t *front); // two 104 x 96 boxes, made by the caller
void weather_sky_set(weather_sky_t *s, tw_mood_t sky, bool calm);   // TW_PLAIN / TW_HIDDEN: empty; same as shown: nothing
void weather_sky_freeze(weather_sky_t *s, bool on);                 // on: every animation stops where it is
void weather_sky_on_flash(weather_sky_t *s, void (*cb)(void *user), void *user); // at each lightning flash
```

What each sky is (positions/sizes from `compose.py` round 3 through the Global Constraints' formula; behind Talpy unless "front"):
- CLEAR_DAY: sun + rays + glow, rays turning slowly (a full turn ≥ 20 s). PARTLY_DAY: smaller sun + day cloud in front of it drifting across and back (≥ 10 s each way). OVERCAST: two grey clouds drifting slowly. FOG: lamp glow + beam at Talpy's lamp, glow breathing; fog bands **front**, drifting side to side. DRIZZLE: day cloud; light drops **front**, some stopping at the hat brim. RAIN: day cloud; drops **front**, falling at today's rain speed, stopping at the umbrella canopy over Talpy. SNOW: flakes **front**, falling slowly and steadily. THUNDER: storm cloud; bolt hidden, flashing on at intervals (card: every 8–15 s; calm: about every 60 s), calling the flash callback; a few drops **front**. CLEAR_NIGHT: moon + glow, stars twinkling slowly. PARTLY_NIGHT: moon, night cloud drifting across it slowly, stars.
- Clouds: flat bottom, bumpy top (puffs on a rounded base, as `compose.py`'s `cloud()`), with a 0.7-unit edge in the edge colour.
- `calm`: half the drops/flakes (same speed), flashes about every 60 s, drift and twinkle slower.
- Reuse `weather_icon.c`'s animation helpers by moving them into `weather_sky.c` (or a small shared file); the hourly row's still small icons keep working unchanged.

- [ ] **Step 1: Implement** `weather_sky.c`.
- [ ] **Step 2: Build:** `pio run` SUCCESS, no new warnings.
- [ ] **Step 3: Commit** `git commit -m "Weather: the live sky"`

---

### Task 5: The weather Talpy widget

**Files:**
- Modify: `src/ui/talpy.h`, `src/ui/talpy.c`
- Create: `src/ui/weather_talpy.h`, `src/ui/weather_talpy.c`

**Interfaces:**
- Consumes: player (Task 1), `TALPY_WEATHER_SET` (Task 2), sky (Task 4).
- Produces, `talpy.h`: `lv_obj_t *talpy_create_set(lv_obj_t *parent, const talpy_set_t *set);` (`talpy_create(parent)` = `talpy_create_set(parent, &TALPY_SET)`); `void talpy_set_mood(lv_obj_t *t, uint16_t mood);`; `void talpy_set_pace(lv_obj_t *t, uint8_t pace);`; `void talpy_play_once(lv_obj_t *t, uint16_t clip);`; `talpy_react_finished` unchanged.
- Produces, `weather_talpy.h`:

```c
lv_obj_t *weather_talpy_create(lv_obj_t *parent, bool calm);       // a 104 x 96 box (clips children), not clickable
void weather_talpy_set(lv_obj_t *w, tw_mood_t talpy, tw_mood_t sky); // shown at once; same as shown: nothing
void weather_talpy_freeze(lv_obj_t *w, bool on);                   // Talpy and sky
```

Layers in the box, bottom to top: sky back, Talpy image at (`TALPY_WEATHER_SET.x`, `.y`), sky front. `calm`: Talpy pace 3 and `weather_sky_set(..., calm = true)`. The sky's flash callback calls `talpy_play_once(talpy, TALPY_WEATHER_SET.react_clip)` while `talpy == TW_THUNDER`. Created frozen = false.

- [ ] **Step 1: Implement** the talpy.c changes and the widget.
- [ ] **Step 2: Build:** `pio run` SUCCESS; `sh test/native/run.sh` passes.
- [ ] **Step 3: Commit** `git commit -m "Weather Talpy widget: Talpy in front of his sky"`

---

### Task 6: The weather card and the glue

**Files:**
- Create: `src/app/weather_state.h`, `src/app/weather_state.c`
- Modify: `src/ui/card_weather.c`, `src/ui/card_weather.h`, `src/app/serial_link.cpp`, `src/ui/motion.c`, `src/ui/weather_icon.c`, `src/ui/weather_icon.h`, `src/main.cpp` (only if a periodic call is needed). The clock face is left as it is here: serial_link keeps calling `clock_face_set_weather()` until Task 7 switches the clock face to the view.

**Interfaces:**
- Consumes: `weather_view()` (Task 3), `weather_talpy_*` (Task 5).
- Produces:

```c
// weather_state.h (app glue, like job_watch.c)
void weather_state_weather(const weather_update_t *w);   // serial_link's weather message
void weather_state_hourly(const hourly_update_t *u);     // serial_link's hourly message
void weather_state_pc(bool online);                      // serial_link's set_pc_online
void weather_state_tick(void);                           // once a minute (an lv_timer inside weather_state.c is fine)
// card_weather.h
void card_weather_view(const wv_view_t *v);   // Talpy, sky and (forecast/none) texts
void card_weather_freeze(bool on);            // motion_hold
```

Rules: on every input (and when the minute tick crosses an hour) `weather_state` rebuilds `wv_input_t` (`clock_known` = the board's `time(NULL) >= CLOCK_SET`) and pushes `weather_view()` to `card_weather_view()` (and, from Task 7, to `clock_face_set_view()`). `card_weather_set()` keeps filling the live texts; `card_weather_view()`: WV_LIVE → live texts as today; WV_FORECAST → the hour's temperature, `wmo_text(code)`, bottom line `"Forecast " LV_SYMBOL_BULLET " %H:%M"` of `forecast_at`, high/low as last received unless the date changed since they came (then `"--"`); WV_NONE offline → `"--°"` and condition `"PC offline"`; WV_NONE online → `"--°"`, `"Waiting for PC"` as today. `motion.c` calls `card_weather_freeze()` where it called `weather_icon_freeze()`. Remove the card icon's functions from `weather_icon.c/.h` (`weather_icon_create/_set/_freeze` and what only they use); the small still icons stay.

- [ ] **Step 1: Implement.**
- [ ] **Step 2: Build:** `pio run` SUCCESS, no new warnings; report firmware.bin size (must be < 4,194,304); `sh test/native/run.sh` passes.
- [ ] **Step 3: Commit** `git commit -m "Weather card: the weather Talpy, and the forecast while the PC is away"`

---

### Task 7: The clock face

**Files:**
- Modify: `src/ui/clock_face.c`, `src/ui/clock_face.h`, `src/ui/card_jobs.c`, `src/ui/card_jobs.h`, `src/app/serial_link.cpp`, `tools/talpy_draw.mjs` + `tools/make_talpy_frames.py` + `src/ui/talpy_set.h` + `src/ui/talpy_data.c` + `src/ui/talpy.c/.h` + `test/native/test_data.c` (remove the sleep still and `talpy_create_still`, now unused)

**Interfaces:**
- Consumes: `weather_talpy_*` (Task 5), `wv_view_t` (Task 3), `banner_headline()` (`src/ui/banner_text.h`).
- Produces: `void clock_face_set_view(const wv_view_t *v);` (replaces `clock_face_set_weather`; `weather_state` pushes the view to it as it does to the card), `void clock_face_set_chats(bool have_data, int waiting, int working, int done);`, `void card_jobs_counts(const job_update_t *u, int *waiting, int *working, int *done);` (used by `card_jobs_set` and by serial_link to feed the clock face).

Layout: the content column becomes time, date, weather row (the weather Talpy `weather_talpy_create(row, true)` + the temperature `montserrat_40`, hidden when `!has_temp`), chat line (`montserrat_14`, `COLOR_MUTED`, the banner headline; hidden while the PC is offline), "PC offline". The face still shifts each minute. `clock_face_show()` unfreezes its weather Talpy, `clock_face_hide()` freezes it (created frozen).

- [ ] **Step 1: Implement.**
- [ ] **Step 2: Build and test:** `pio run` SUCCESS, `sh test/native/run.sh` passes (banner set regenerated without the sleep still: checksums of its other frames unchanged).
- [ ] **Step 3: Commit** `git commit -m "Clock face: the calm weather Talpy and the chats' state"`

---

### Task 8: Docs, layout spec and screenshots

**Files:** `design/desk-display-layout.html`, `design/desk-display-normal.png`, `design/desk-display-clock.png` (and any other screenshot showing the weather card), `README.md`

- [ ] **Step 1:** Layout spec: the weather card's icon → the weather Talpy (embed mockup pictures as `data:` URIs), the clock face's new column, and a note on the PC-away rules; screenshots re-exported via the page's `#png-…` mode with headless Firefox; README (weather card, clock face, regenerating both sets, credits).
- [ ] **Step 2: Verify:** native tests, unittest, `pio run`.
- [ ] **Step 3: Commit** `git commit -m "Docs: the weather Talpy"`

---

### Task 9: On the board (needs the owner's go)

- [ ] Flash (stop the sync service first), run `python3 host/sync.py --demo`, and check every weather day and night (a test weather message per code), the clock face with the PC connected and away (Talpy hidden, forecast sky, no chat line), the pull-down drag over the card, the alert.
