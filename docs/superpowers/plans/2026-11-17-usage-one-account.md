# Usage card, one account bigger: Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** With exactly one account, the usage card shows each window (5h, week) on a line of its own, bigger; with two,
it is as before.

**Architecture:** A second set of widgets in `card_usage.c` for the one-account layout, a `usage_row_t` like the
rows', so `show_window` (bar, %, colour, reset, the passing of a reset) serves both unchanged. `card_usage_set`
shows one layout or the other by the number of accounts; `recheck` rechecks whichever is shown.

**Tech Stack:** C, LVGL 9.1 (montserrat 14 and 24 built in).

**Spec:** `docs/superpowers/specs/2026-11-17-usage-one-account-design.md`

## Global Constraints

- The card's inside: 240 x 100 px (280 x 140 card, CARD_PAD 20). Title "USAGE" at (0, 0), montserrat 14.
- Two accounts: positions, sizes and fonts exactly as today.
- Colours: as `usage_color` (green, COLOR_WAIT from 70, COLOR_LIMIT from 90); "--" in COLOR_MUTED while not known.

## Review Focus

- 1 -> 2 -> 1 accounts: each switch shows only one layout's widgets, with the current numbers (not stale ones).
- 0 accounts ("No accounts") after 1: the one-account widgets hidden, like the rows.
- A long account name with a plan: dotted, never over "USAGE".
- 100% in montserrat 24 fits its label on one line ("100%" mustn't wrap the "%").
- A reset passing while one account is shown: recheck shows 0% and drops the reset time, as for the rows.

---

### Task 1: The one-account layout

**Files:**
- Modify: `src/ui/card_usage.c` (make_meter, new make_single, card_usage_set, recheck, card_usage_create)
- Modify: `src/ui/card_usage.h` (card_usage_set's comment)
- Test: a native LVGL render of the card (job tmp, not in the repo: the native tests don't link LVGL) to PNG, and
  checks of which widgets are hidden

**Interfaces:**
- Consumes: `usage_row_t`, `show_window(usage_row_t *, int)`, `make_line_label`, `make_label`.
- Produces: nothing new outside the file; `card_usage_set(const usage_account_t *, int)` keeps its signature.

- [ ] **Step 1: Render check, failing**: render the card with 1 account (work, Pro, 42 % reset in 3 h, 84 % reset
  in 3 days), with 2, then 1 again, then 0; assert with 1: the rows and the "5h"/"7d" heads hidden, the single
  layout's 2 bars, % labels (font montserrat 24) and window names visible; with 2: the reverse; with 0: both hidden.
  Fails today (no single layout).
- [ ] **Step 2: Implement.** `make_meter(parent, x, y, w, h)` (radius h/2; the rows pass 36, 6). `make_single(parent)`
  -> `usage_row_t`: `name` a title-line label right-aligned at x 90, w 150, LONG_DOT, COLOR_MUTED, "name · plan";
  `plan` unused (NULL; card_usage_set skips it); for window c at y0 = 24 + 40c: its name ("5h", "Week") at (0, y0),
  COLOR_TEXT; reset at (0, y0 + 18), w 64; bar at (64, y0 + 12), 100 x 10; % montserrat 24, w 70, right-aligned at
  (170, y0 + 3). `card_usage_set`: one = n == 1; the heads and rows shown only when !one; the single layout only when
  one. `recheck`: the shown layout.
- [ ] **Step 3: Render check passes**; look at the PNGs (1 account: 0 %, 42 %, 75 %, 95 %, 100 %, "--"; 2 accounts
  unchanged from before the change).
- [ ] **Step 4: Firmware builds with no warnings**: `pio run -e advance-hmi`; native tests `sh test/native/run.sh`.
- [ ] **Step 5: Commit** spec, plan and code.
