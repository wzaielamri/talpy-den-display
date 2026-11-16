# Talpy on the display

Date: 2026-11-16. Status: draft, for approval.

## Why

The display should feel like part of Talpy Den. It already uses Talpy Den's state colours
(orange waiting, blue working, green done) on dark cards, but what makes Talpy Den recognisable
is Talpy, the mole in its burrow, and he's missing. The owner asked for one Talpy, animated, and
for the board to stay fast.

## Goals

- One animated Talpy at the top of the jobs card. He acts out the most important state, as the big
  Talpy does in Talpy Den's popup.
- He's the same Talpy, made from Talpy Den's own drawing code: same shapes, colours, moods and moves.
- The board stays as quick as it is today: touch, the pull-down panel and the alerts don't get slower.
- Building the firmware needs nothing new. Making Talpy's pictures again is a separate tool step,
  like `tools/make_clock_font.py`.

## Non-goals

- A Talpy for each chat (Talpy Den's pet strip), per-chat colour stripes, and per-chat colours.
- Restyling the usage bars, weather card or header. (Talpy Den's usage bars are its accent blue
  below 70%, the display's are green. Leave that for later.)
- Playing with Talpy by touch (petting, tugging, cookies). A tap on him still counts toward the
  screen's double tap.
- Talpy Den's playful extras (props, jokes, games). The display uses Talpy Den's "calm" set only.
- Changes to `host/sync.py`, the serial protocol or Talpy Den.

## Anti-goals (a failure even if it works)

- Touch, the pull-down drag or the alert feel slower than now, or the screen tears or stutters.
- Talpy looks or moves differently from the popup's (redrawn by hand, other colours, other poses).
- Something on the clock face moves. That face is up 24/7 at 10% brightness and is meant to be
  still apart from the minute's redraw and its burn-in shift.
- A normal `pio run` needs node, Talpy Den's checkout or a network.

## Constraints (from the project as it is)

- ESP32-S3, 800×480 RGB565, LVGL 9.1. Two full-frame buffers in PSRAM, `LV_DISPLAY_RENDER_MODE_DIRECT`,
  so a redraw costs in proportion to the area that changed. Big gradients, shadows, layers with
  opacity and large moving areas are what make it slow.
- App partition: 4 MB (`partitions.csv`), firmware about 0.94 MB today. The chip has 16 MB.
- The jobs card is 472×376 at (312, 88), padding 20 (432×336 inside). The three count tiles sit at
  y = 28, 136×88 each. The four rows start at y = 124, 48 px each, then the "+N more" line.
- The jobs file gives the board only three states: `working`, `waiting`, `done` (sync.py folds
  Talpy Den's `idle` into `done`). No agents, limits or failures.
- `motion_hold()` (src/ui/motion.c) freezes the screen's animations while the pull-down panel, an
  alert or the clock face covers them. `card_jobs_freeze()` is how the jobs card hears it.
- `job_watch.c` already spots a chat going from working or waiting to done, for the done alert.
- Talpy Den draws Talpy in code: `package/contents/ui/talpy/` (talpy.mjs, actions.mjs, art.mjs,
  films.mjs). Its `tools/talpy_icons.mjs` already records his drawing as SVG through a small
  canvas stand-in (`SvgContext`). That class isn't exported: the tool runs when imported.
- Available on the build machine: node, rsvg-convert, Python 3 with Pillow (already needed for the
  clock font).
- Measured (a probe run of Talpy Den's code with a seeded random, each clip stepped at 100 ms,
  rasterised at 1.5 px per box unit, each clip cropped to what it draws):
  - Talpy reaches outside his 64-unit box: in `finished` his jump takes his head and lamp about 17
    units above it; in `sign` the sign sticks out about 15 units to the right; in `alert` the "!"
    goes about 5 units up. The popup gives him room for that (TalpyView.qml: a canvas 1.6 × 1.5
    the box).
  - The ten clips below are about 350 frames, nearly all different (eased moves, blinks and things
    that fly about make frames unique; dropping identical ones saves almost nothing).
  - Stored as RGB565A8: about 7.6 MB raw, about 1.9 MB run-length encoded, about 1.0 MB with zlib
    per frame (extrapolated from 145 frames of five clips: 3.17 MB raw, 0.78 MB RLE, 0.43 MB zlib).
    Most of each frame is transparent, which is why it compresses well.

## Approaches

1. **Picture loops made from Talpy Den's code (chosen).** A tool runs Talpy Den's drawing code and
   saves each move as a short loop of pictures, stored compressed in the firmware. The board
   unpacks and flips between them, about 10 a second, and only Talpy's square is redrawn.
   Each move is the popup's. It costs flash (about 1–2 MB compressed, measured) and a re-run of
   the tool when Talpy changes in Talpy Den.
2. **One still per mood, moved by the board.** A blink and a bob done by shifting the picture.
   About 150 KB, but he looks stiff: no typing, no sign, no cheering. Rejected: doesn't feel like
   the popup's Talpy.
3. **Draw Talpy live on the board.** Port thousands of lines of drawing code to C and keep them in
   step, and draw anti-aliased curves every frame on the ESP32. Rejected: slow and diverges.

The owner picked 1.

## What gets built

### Where Talpy is: the banner (replaces the card's title row and the three count tiles)

```
┌────────────────────────────────────────────────────┐
│ ┌──────────────────────────────────────────────┐   │
│ │ 1 chat needs you                     (Talpy) │   │  banner: 432 × 116 at the card's top
│ │ ● 1 waiting  ● 2 working  ● 2 done    /||\   │   │  (the old title row + tiles: y 0..116)
│ │ updated 14:52:05                     ~~~~~~  │   │
│ └──────────────────────────────────────────────┘   │
│ ● desk-display-gadget                waiting · 3m  │  the four rows, unchanged (y = 124)
│ ● quick-chat-next                    working · 12m │
```

- Like the popup's hero box: the headline left, Talpy right, the counts as a line of coloured dots
  under the headline. The headline is the card's title now, as in the popup: "CLAUDE JOBS" goes,
  and "updated 14:52:05" moves into the banner as a small muted line.
- The banner takes y 0..116 of the card's inside (the title row and the tiles' strip), so Talpy has
  room for his full reach (see Speed: nothing of him is clipped), and the rows stay where they are.
- Headline, as Talpy Den's `headlineText()`, first that applies:
  - any waiting: "1 chat needs you" / "N chats need you" (orange)
  - else any working: "1 chat working" / "N chats working"
  - else any done: "1 finished chat" / "N finished chats" (Talpy Den adds "to look at"; dropped
    here, it doesn't fit beside Talpy: 322 px for 12 chats, about 292 px free)
  - else: "All quiet"
  - before the first jobs message: "Waiting for the PC" (today's empty-rows text)
- The counts line keeps the three numbers the tiles showed today: "● 1 waiting ● 2 working ● 2 done",
  each dot in its state's colour.
- Background: the tile colour; while a chat waits, the popup's warm orange tint and orange border
  (theme.h's `COLOR_ALERT` and `COLOR_WAIT`). Flat colours only, no gradient or shadow.
- The waiting tile's breathing border goes. Talpy's alert moves catch the eye instead, and the
  banner doesn't redraw its whole area for a pulse.
- The four rows and the "+N more" line stay where they are.

### What Talpy does (the moods)

As Talpy Den's `petMood` for the big Talpy, and its `MOODS[...].calm` pools:

| Board's state | Talpy Den mood | His moves (clips) | Pause between |
|---|---|---|---|
| any chat waiting | `needs` | `alert`, `sign`, `wave` | 300–900 ms |
| else any working | `working` | `typing`, `thinking`, `read` | 500–1500 ms |
| else (done or none) | `done` | `sleep`, `nap` (asleep, z's) | 1500–3500 ms |
| no jobs message yet | `loading` | `lookaround` | 800–2000 ms |
| a chat just finished | reaction | `finished` (jumps, confetti), once | then back to the mood |

- In a mood the board plays a clip picked at random (weighted as Talpy Den's pool), then holds the
  mood's base pose for a random pause in that range, then picks again. Talpy Den does the same.
- The base pose between clips: for `needs`, its bounce as a short loop (Talpy Den's base pose for
  `needs` bounces); for the other moods, one still picture, with a blink (a few frames) now and
  then. The lamp's slow glow cycle (about 4.8 s) isn't played: it would cost dozens of frames.
- One take of each clip. Talpy Den varies a clip's length and extras each time it plays; here the
  variety comes from the random pick and pauses. (The tool may take a number of takes, for later.)
- So the differences from the popup, on purpose: one take per clip, no lamp glow cycle, no
  pictures in between moods. Everything drawn is Talpy Den's.
- The "finished" reaction plays when `job_watch.c` sees a chat finish (where the done alert comes
  from), on top of whatever the mood is, then goes back to it. If the mood is `needs`, it's skipped:
  the one who needs you matters more.
- A mood change cuts to the new mood's base pose and starts its loop. Talpy Den's in-between pictures
  aren't required.

### On the alert screen

- Talpy takes the bell's place. Waiting alert: the `needs` mood (sign, alert, wave). Done alert: the
  `finished` reaction, then the `done` mood.
- Suggested placement: peeking over the alert box's top edge, as if his burrow were on it. The
  builder may place him elsewhere on the alert if it reads better; the name keeps its width.

### On the clock face

- Talpy asleep, one still picture (the `done` mood's sleeping pose), placed with the clock and
  shifted with it each minute. No animation, no z's: the face stays still (see Anti-goals).

### Speed

- One scale everywhere (banner, alert, clock face), so no scaling on the board: chosen so the
  tallest clip (`finished`'s jump) fits the banner's 116 px, about 1.3–1.35 px per box unit (the
  64-unit box about 83–86 px; the jump about 105–109 px tall, the sign about 97–100 px wide).
- Each picture covers Talpy's whole reach, not just his box: nothing of him is clipped (the jump,
  the sign, the "!"). The burrow stays at the same spot from clip to clip.
- Pictures are RGB565 with an 8-bit alpha (`LV_COLOR_FORMAT_RGB565A8`), so one set works on every
  background: the banner's two tints, the alert box and the clock face's black.
- Stored compressed (run-length, or zlib inflated with the ESP32-S3 ROM's miniz; the builder picks
  after measuring), each frame on its own. The board unpacks the current frame into one RAM buffer
  per place and points an `lv_image` at it. Unpacking one frame (about 25 KB) takes on the order of
  a millisecond. No LVGL image cache of all frames.
- Played with one `lv_image` and one `lv_timer` per place; at most about 10 pictures a second. Only
  the image's area is invalidated (about 3% of the screen).
- Stopped while covered: the banner Talpy freezes through `card_jobs_freeze()` / `motion_hold()`
  like the other animations; the alert's Talpy runs only while the alert shows.
- Budget: frames total at most 2 MB in flash, compressed. The app partition stays at 4 MB (about
  3 MB free today). The tool prints the total and fails above the budget.

### The tool that makes the pictures

- As built: `tools/talpy_draw.mjs` (node, draws the frames as SVG) and `tools/make_talpy_frames.py`
  (runs it, rasterises, encodes, writes `src/ui/talpy_data.c`, a C array: the builder's choice the
  spec allowed), run by hand, like the clock font tool:
  `python3 tools/make_talpy_frames.py ../talpy-den src/ui/talpy_data.c`
- The node part imports Talpy Den's `talpy.mjs` from the checkout given, plays each clip of the table above at
  a fixed step (100 ms), draws each frame, compresses it, and writes the frames plus, per clip, the
  frame order and each frame's duration, and a header.
- Repeatable: `Math.random` is replaced by a seeded generator, so the same Talpy Den gives the same
  output on the same machine (the z's use the system's sans-serif, and the output depends on the rsvg version). The output names the Talpy Den commit it was made from.
- Uses only what's installed: node, rsvg-convert, Python with Pillow. No npm packages. (A probe of
  this, rendering frames through a copy of talpy_icons.mjs's `SvgContext` and rsvg-convert, worked.)
- The output is committed, so `pio run` doesn't need the tool. Keep it compact in git: a binary blob
  embedded with PlatformIO's `board_build.embed_files`, or a C array if that stays reasonable
  (a C array of 1 MB is about 6 MB of text); the builder picks.
- It also writes a contact sheet PNG of every clip's frames (not committed), to look at.

### Docs

- `design/desk-display-layout.html` (the layout spec): the banner replaces the tiles; the alert's and
  clock face's Talpy. Its screenshots in `design/` and the README's are made again.
- README: What it does (the banner and Talpy), Design (how to make the frames again), Credits
  (Talpy is from Talpy Den, same author, MIT).

## Left to the builder

- Talpy's exact scale (within the range above), the text positions and fonts within the banner
  (from the fonts already built in).
- The compression (run-length or ROM zlib) and how the output is stored (embedded binary or C).
- How the tool gets Talpy Den's drawing into pixels (SVG through rsvg-convert, as talpy_icons.mjs
  does, is the expected path; copying its `SvgContext` into this repo's tool is fine, credited).
- How each clip is driven in Talpy Den's code (`react()`, `pose()`, `setMood()` with the brain off,
  or the `Films` class), as long as the moves are Talpy Den's.
- The module's API on the board (e.g. `talpy_create()`, `talpy_set_mood()`, `talpy_react()`,
  `talpy_freeze()`).
- The alert's exact Talpy placement.

## Checking it

- The tool: run twice on the same Talpy Den, identical output; a contact sheet PNG of every clip's
  frames to look at; total size printed and under budget.
- `pio run` builds.
- On the board: flashing needs the owner's go (the sync service holds the port). Check every
  screen with `python3 host/sync.py --demo`, the pull-down drag, and that the banner Talpy stops
  while the shade, alert or clock face covers it.
