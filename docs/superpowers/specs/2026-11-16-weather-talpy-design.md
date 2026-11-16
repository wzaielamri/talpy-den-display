# Weather Talpy

Date: 2026-11-16. Status: draft, for approval.
Mockups the owner approved: [`2026-11-16-weather-talpy-mockup/mockups.png`](2026-11-16-weather-talpy-mockup/mockups.png)
(rendered by `render.mjs` + `compose.py` in the same folder; see Appendix).

## Why

After Talpy came to the jobs banner ([2026-11-16-talpy-on-the-display-design.md](2026-11-16-talpy-on-the-display-design.md)),
the owner wants the weather to feel like Talpy Den too: Talpy acting out the weather (in the rain, in fog, enjoying the
sun) instead of a plain icon. It should read as weather at a glance from across the desk, and be nicely animated.

## Goals

- The weather card's icon (104 × 96, top right of the card) becomes a **weather Talpy**: Talpy with a prop for the
  weather, in front of a sky that shows it. Ten pictures, one per kind of weather the board knows (below).
- Nicely animated, but **calmer and slower than the banner Talpy**, so the banner's Talpy still wins the eye when a chat
  needs you.
- The **clock face** shows the weather Talpy, animated (slower still), in place of today's still sleeping Talpy and the
  small weather icon, plus a short line with the chats' state.
- Talpy and his props look like Talpy Den's: drawn by Talpy Den's code (the props in its style: outlines, shading, held in
  his paw), as in the mockups.
- The board stays as quick as today.

## Non-goals

- The hourly forecast keeps its plain icons (52 × 48 is too small for Talpy).
- No change to Talpy Den (its code is used, not edited), to the banner Talpy, to the alerts, or to the serial protocol
  (the weather message already carries `code`, `day`, `temp`).
- No new weather data or kinds.

## Anti-goals

- The weather can't be told at a glance at real size (the sky must stay as big as in round 3 of the mockups).
- The weather Talpy competes with the banner Talpy (quick or frequent moves).
- Touch, the pull-down drag or the alerts get slower; anything animates under the pull-down's dim (the board's rule:
  `motion_hold`).
- The app no longer fits its 4 MB partition.

## What it shows

The board maps WMO codes to eight kinds (`kind_from_wmo` in `src/ui/weather_icon.c`); clear and partly cloudy also have
a night version (`is_day`). So ten pictures. Talpy's antenna light is the same calm warm yellow in all of them (Talpy
Den's `idle` lamp), so it never reads as a chat state.

| # | Weather | Talpy | Sky | How it moves |
|---|---|---|---|---|
| 1 | Clear (day) | sunglasses on, smiling | big sun with rays and glow, top right | rays turn slowly; glasses glint now and then; he sways a little |
| 2 | Partly cloudy (day) | sunglasses on | sun, a cloud in front of it | the cloud drifts over the sun and away |
| 3 | Overcast | holding Talpy Den's mug (steaming), happy eyes | two grey clouds | clouds drift; he sips now and then (Talpy Den's `coffee` move) |
| 4 | Fog | squinting | fog bands across him; his light glows like a fog lamp, with a soft beam | the bands drift side to side; the lamp glows |
| 5 | Drizzle | yellow rain hat | white cloud, light drops | drops fall; some land on the brim |
| 6 | Rain | umbrella held over his head in his right paw | white cloud, more drops | rain falls and stops at the canopy; now and then he twirls the umbrella |
| 7 | Snow | beanie and scarf, happy eyes | snowflakes | snow falls; he shivers now and then (Talpy Den's `shiver`) |
| 8 | Thunderstorm | ducked into his burrow (peek), wide eyes | dark cloud, lightning bolt, drops | at each flash he ducks right in, then peeks out again |
| 9 | Clear (night) | asleep, no hat | moon with glow, twinkling stars | stars twinkle; a soft "z" floats up now and then |
| 10 | Partly cloudy (night) | asleep | moon, a grey cloud | the cloud drifts slowly across the moon |

The owner turned down, on purpose: sunglasses pushed up on his head (read as eyebrows), a closed umbrella, a leaf, a
nightcap, and the first hand-drawn umbrella. The props in `render.mjs` (sunglasses, umbrella, rain hat, beanie, scarf) are
the approved ones.

## Approaches

- **A. Everything as ready-made pictures** (Talpy, props and sky). Most consistent, but falling rain/snow and drifting
  clouds make nearly every frame unique: roughly 1.5–2 MB more, past the ~1.8 MB free, so the app partition would have to
  grow to 8 MB.
- **B. Talpy and props as ready-made pictures, the sky drawn live by the board (chosen).** Talpy's pictures cost about
  0.6 MB and fit; the sky (sun, clouds, drops, snow, fog, bolt, moon, stars) is LVGL shapes animated by the board, as
  today's weather icons are, so it stays smooth and costs little, and the board can sync it with Talpy (a flash makes him
  duck). The sky is flat shapes, as in the mockups, not Talpy Den's outlined style.
- **C. Draw Talpy live on the board.** Ruled out, as for the banner: slow, and a second copy of Talpy Den's drawing code.

Where the props are drawn: in this repo's frame tool, hooked onto Talpy Den's Talpy (as `render.mjs` does: a subclass
that draws the face props after `drawFace` and the held props in the right arm's frame), so Talpy Den doesn't change.

The owner picked B (recommended; their answer to "sizes right, and OK with B?" was "perfect now").

## Constraints (from the project as it is)

- Pictures: the existing pipeline — `tools/talpy_draw.mjs` (node, Talpy Den's code, seeded, SVG frames) and
  `tools/make_talpy_frames.py` (rsvg-convert, Pillow, the run-length codec, a generated C file) — and on the board
  `talpy_codec.c`, `talpy_player.c` (moods, weighted picks, pauses, rest loops, a reaction; wrap-safe timing) and the LVGL
  widget `talpy.c`. Reuse them.
- `talpy_set_t` (`src/ui/talpy_set.h`) has a fixed `moods[TALPY_MOODS]` (4) and the player indexes it by
  `talpy_mood_t`. The weather set needs ten "moods" (one per picture), so the set type and the player have to take a
  mood count (or the weather set gets its own type); the banner's data and behaviour must not change.
- Flash today: firmware 2,213,744 bytes of the 4 MB app partition (about 1.8 MB free). Budget for the weather frames:
  ≤ 0.8 MB compressed; the tool prints the total and fails above it.
- Scale: in the mockups the scene is 95 × 88 box units shown at 104 × 96 px (about 1.1 px per unit; the banner uses 1.3).
  The weather Talpy is smaller than the banner's so the sky has room; the sky sizes are those of round 3 (see
  `compose.py`): e.g. sun radius 10.5 units with rays to 18, clouds 1.45–1.8× Talpy Den's cloud puff, moon radius 10.5.
- Today's `weather_icon.c` draws the eight kinds from LVGL shapes in a 104 × 96 box, animated on the card
  (`weather_icon_create/_set`, `weather_icon_freeze` from `motion_hold`) and still at half size for the hourly row
  (`weather_icon_small_*`). The hourly row must keep working unchanged.
- `motion_hold` (`src/ui/motion.c`) freezes the main screen's animations while the pull-down panel (MOTION_SHADE), the
  clock face (MOTION_CLOCK) or an alert (MOTION_ALERT) covers them; the card's weather Talpy and its sky must join it.
- The clock face (`src/ui/clock_face.c`) is a flex column (time, date, sleeping Talpy, weather row with small icon and
  temperature, "PC offline") that shifts a few pixels each minute against burn-in. It shows after 2 min without a touch
  only when no chat is working or waiting (or while the PC is away, or on a double tap), so its chat line will mostly say
  "All quiet" or "N finished chats".
- One Talpy decode buffer is about 30 KB (`heap_caps_malloc`, internal RAM first).

## What gets built

### The weather Talpy (card)

- A widget for the 104 × 96 box at the card's top right (where `weather_icon_create(parent, 140, 0)` puts the icon
  today): the sky layer (LVGL shapes) and the Talpy layer (a `talpy` image from the weather set), plus the sky parts that
  go in front of him (drops, fog bands). `card_weather_set` hands it the code and `is_day` as it does the icon now.
- Changing weather switches the picture at once (the new weather's rest picture and sky), as a mood change does for the
  banner.
- Calm timing: longer pauses between his moves than the banner's (the builder sets them per weather; as a guide,
  several seconds between moves), and the sky's motion slow (clouds drifting over 10+ s, rays turning slowly). Rain and
  snow fall at a natural pace.
- Sync: the thunderstorm's flash and Talpy's duck happen together (the board starts both); the rain stops at the
  umbrella's canopy; drizzle drops may land on the brim.
- Freezes with `motion_hold` like today's icon (the whole widget: Talpy and sky).

### The pictures

- `tools/talpy_draw.mjs` gains the weather set: for each weather, Talpy's rest picture (with prop) and his few moves
  (table above), drawn with Talpy Den's code plus the props (from `render.mjs`), Talpy only, no sky. Seeded, repeatable,
  as the banner's.
- Scale 1.1 px per unit (the builder may adjust within ±0.1 to fit the 104 × 96 box with the sky's room).
- `make_talpy_frames.py` writes a second generated set (e.g. `src/ui/talpy_weather_data.c`, `TALPY_WEATHER_SET`) with
  its own canvas and checks (budget, size), and the native data test covers it.
- Moves with Talpy Den equivalents use them (sip: `coffee`; shiver: `shiver`; duck/peek: depth `hidden`/`peek`; sleep's
  "z"); the others (glasses glint, umbrella twirl, sway) are new poses/steps in the tool.

### The sky

- Built from `weather_icon.c`'s shapes and animations, re-laid out around Talpy at the round-3 sizes (bigger than
  today's icon parts) and colours (`COLOR_SUN`, `COLOR_CLOUD`, `COLOR_RAIN`, `COLOR_MOON`, the theme greys for overcast
  and storm clouds). Clouds have flat bottoms and bumpy tops. Its parts are drawn in the 104 × 96 box like today's icon.
- The hourly row's half-size still icons stay as they are (same code or a kept copy of it; the builder decides).

### The clock face

- The weather row's small icon and the still sleeping Talpy are replaced by the weather Talpy (same widget, its own
  instance), animated, with the temperature beside it as now.
- Slower than the card's: longer pauses, and his moves at most every so often (the builder picks; the face is up all
  night at 10% brightness and should feel still at a glance).
- Under it, a short muted line with the chats' state: the banner's headline (`banner_headline()`: "All quiet",
  "2 finished chats", "1 chat working", …), updated when the jobs change. "PC offline" stays as now.
- The face still shifts a few pixels each minute. Only one weather Talpy animates at a time: the card's is frozen while
  the clock shows (MOTION_CLOCK), the clock's runs only while the clock shows.

### Docs

- `design/desk-display-layout.html`: the weather card's icon becomes the weather Talpy (with the mockup pictures), and
  the clock face section; screenshots `desk-display-normal.png`, `desk-display-clock.png` (and any other showing the
  weather card) made again. README: the weather line, the clock face, credits.

## Left to the builder

- The exact poses, steps and timings of each move; how many frames each takes (within the budget).
- How the set type and player take more moods; whether the weather widget is a new file or grows `weather_icon.c`.
- The sky's exact layout per weather, within the round-3 sizes; whether rain stopping at the canopy is a clip region or
  per-drop end height.
- Placement of the chat line and the weather row on the clock face, within its current column.

## Checking it

- Native tests for the new set (decode every frame, checksums, tables) and for any player change (more moods; the
  banner's behaviour unchanged: its tests still pass).
- `pio run` builds; firmware under 4 MB; the tool's contact sheet of every weather clip looked at against the mockups.
- On the board (owner's go): every weather via `host/sync.py --demo` (or a test message per code), day and night; the
  clock face; the pull-down drag over the card; the alert; nothing animates under the dim.

## Appendix: the mockup sources

`2026-11-16-weather-talpy-mockup/render.mjs` draws Talpy with the approved props using Talpy Den's code (a `Talpy`
subclass; `svgctx.mjs` is the SvgContext from `tools/talpy_draw.mjs`); `compose.py` adds the sky and makes the PNGs.
Throwaway code: the source of the props' shapes and the sky's sizes, not something to ship as is.
