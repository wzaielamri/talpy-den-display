# Every chat in a list, the lock, tap to open: Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The jobs card lists every chat (waiting first, then newest-started first) in a scrolling list; the display
goes to the clock while the laptop is locked; a tap on a chat (or **Open on PC** on an alert) brings it to the front
on the laptop through Talpy Den's new `jump.py --job`.

**Architecture:** The board orders the jobs itself (a pure C function, natively tested) and shows them in an LVGL
scrolling list in the rows' old place. `sync.py` adds `started`, watches logind's `LockedHint` and sends `lock`, and
answers the board's `open` lines by running `jump.py --job <id>` on a worker thread. Talpy Den gains `startedAt` in
`jobs.json` and a `jump.py --job` that only looks ids up in what it already lists, then does what a click on the row
does.

**Tech Stack:** C/C++ (Arduino-ESP32, LVGL 9.1, ArduinoJson 7), Python 3 stdlib + pyserial (`host/sync.py`), Python 3
stdlib (Talpy Den).

**Spec:** `docs/superpowers/specs/2026-11-18-jobs-list-lock-tap-design.md` (read it with this plan).

## Global Constraints

- Two repositories. Display: this worktree, branch `worktree-jobs-list-lock-tap`. Talpy Den: a worktree of
  `/home/wadhah.zai/repos/talpy-den` on a new branch `display-started-and-job` (made with
  `git -C /home/wadhah.zai/repos/talpy-den worktree add /home/wadhah.zai/repos/talpy-den-display-job -b display-started-and-job`;
  if this session's tools refuse git outside its own worktree, stop and ask the owner to run Tasks 5 and 6 in a
  session started in `talpy-den`). Never push to `main`.
- Commits carry only the owner's identity: no `Co-Authored-By` or "Generated with" lines. Messages in the repo's
  style: `<Area>: <what, plainly>` (e.g. "Jobs card: every chat in a list that scrolls").
- Order: waiting jobs by `since` oldest first (unknown `since` = 0, so first); then all others by `started` newest
  first, `started` 0 falling back to `since`, both 0 last; equal keys keep the PC's order.
- List: 432 x 212 at (0, 124) inside the jobs card; rows 48 px as today; at most 64 rows (`JOBS_MAX`, = sync.py's
  `MAX_JOBS`, = job_watch's `SEEN_MAX`); back to the top 30 s (`TOP_AFTER_MS 30000`) after the last touch on the list,
  when a chat starts waiting, and when the clock face shows.
- Protocol (exact): jobs row `started` (Unix s, optional); PC to board `{"type":"lock","locked":true|false}` and
  `{"type":"opened","id":"…","ok":true|false,"message":"…"}`; board to PC `{"type":"open","id":"…"}`.
- Timings: "Opening…" till the answer; the answer 4 s (`RESULT_MS 4000`); the board gives up after 20 s
  (`OPEN_WAIT_MS 20000`); `sync.py` kills `jump.py` after 15 s (`JUMP_TIMEOUT = 15`).
- Exact texts: "Opening…" (the ellipsis as "..." if the 14 px font lacks "…"), "Opened", "No answer from the PC",
  "Talpy Den took too long", "Still opening the last one", "Talpy Den not found", "That chat isn't listed any
  more", "Talpy Den is too old for this", "The PC is locked", "Not a chat id", "Open it from Talpy Den first: it asks
  before connecting VS Code", "Open on PC".
- Id check (Talpy Den `--job` and `sync.py` alike): at most 300 characters, printable ASCII, none of whitespace,
  `"`, `'`, `\`, `;`, `|`, `&`, `$`, backtick, `<`, `>`, not containing `..`, not starting with `-`.
- `sync.py` drops a board line over 1024 bytes; forwards `jump.py`'s message as `plain()` cut to 80 characters;
  logs only the id, `ok` and that message.
- Nothing private anywhere new: no prompt, reply, question or task text in `jobs.json`, on the board, in a message or
  a log.
- Fonts on the board: montserrat 14, 24, 40, 48 only (lv_conf.h).
- Build and tests: `~/.platformio/penv/bin/pio run -e advance-hmi` (no new warnings); `sh test/native/run.sh`;
  `python3 -m unittest discover -s host -p 'test_*.py'`; Talpy Den: `python3 -m unittest discover -s tests` in its
  worktree. (`pio` isn't on PATH; its penv is Python 3.12: if it complains about Python 3.15, stop and tell the
  owner.)

## Review Focus

- A jobs message re-orders the rows between a finger's press and its lift: the tap must open the chat that was
  pressed, not the one now in that row (Task 7: the id is taken at press).
- A board that restarts, or a `sync.py` that starts, while the laptop is locked: `lock` must reach the board before
  any `jobs` (Task 4: `send_all` order test).
- An id with quotes, spaces, `$(…)` or non-ASCII, or a chat name with quotes: the board's `open` line stays valid
  JSON (ArduinoJson serializes it, Task 7), and `sync.py` and `--job` refuse the id without running anything
  (Tasks 6 and 8 tests).
- No logind session, no `gdbus`, or `gdbus monitor` dying: unlocked, one warning, no busy loop or crash (Task 4
  tests).
- The chat being opened leaves the list, or the lock comes, before the answer: the answer or the 20 s timeout still
  frees the board for the next tap, and nothing is drawn on a row that's gone (Task 7).

---

### Task 1: The order, and `started` from the PC to the card

**Files:**
- Create: `src/ui/job_info.h` (job_state_t and job_info_t, moved out of card_jobs.h, no LVGL)
- Create: `src/ui/job_order.h`, `src/ui/job_order.c`
- Modify: `src/ui/card_jobs.h` (include job_info.h), `src/ui/card_jobs.c` (fill rows through `job_order`)
- Modify: `src/app/job_watch.h` (include `ui/job_info.h` instead of `ui/card_jobs.h`)
- Modify: `src/app/serial_link.cpp:63-96` (`started`)
- Modify: `host/sync.py` (`jobs_rows`: `started`)
- Test: `test/native/test_job_order.c`, `test/native/run.sh`; `host/test_sync.py` (new)

**Interfaces:**
- Produces: `job_info_t` with a last field `time_t started` (0: not known), after `id`;
  `int job_order(const job_info_t *jobs, int n, int *out)`: fills `out[0..n-1]` with indexes into `jobs` in the
  card's order, returns n (n <= 0: 0).
- Produces (host): `host/test_sync.py` imports `sync.py` with a fake `serial` module in `sys.modules` when pyserial is
  missing; later tasks add tests there.

- [ ] **Step 1: Write the failing native tests** in `test/native/test_job_order.c` (helper `J(state, since, started)`;
  states `W`=JOB_WAITING, `K`=JOB_WORKING, `D`=JOB_DONE):
  - `test_waiting_first_longest_asking`: [W since 300, K started 100, W since 100] -> [2, 0, 1]
  - `test_rest_newest_started_first_whatever_state`: [D started 500, K started 200, K started 900] -> [2, 0, 1]
  - `test_started_falls_back_to_since`: [D started 600, K started 0 since 700] -> [1, 0]
  - `test_neither_known_last`: [K 0/0, D started 50] -> [1, 0]
  - `test_waiting_unknown_since_first`: [W since 100, W since 0] -> [1, 0]
  - `test_ties_keep_pc_order`: [K started 5, D started 5, K started 5] -> [0, 1, 2]
  - `test_none`: n 0 returns 0.
  Add `run_test test_job_order src/ui/job_order.c` to `run.sh`.
- [ ] **Step 2: Run** `sh test/native/run.sh`. Expected: `FAIL (build): test_job_order` (no job_order.h).
- [ ] **Step 3: Implement** `job_info.h` (move the enum and struct with their comments; `started` documented),
  `job_order.c`: a stable insertion sort of indexes (n <= 64) by the key (waiting group 0 with `since` ascending,
  others group 1 with `started ? started : since` descending). card_jobs.c: replace the three-state loop in
  `card_jobs_set` with `job_order` (still the first 4 rows in this task). serial_link.cpp: `started` like `since`
  (`row["started"] | 0LL`, negative -> 0).
- [ ] **Step 4: Run** `sh test/native/run.sh`. Expected: `ok: test_job_order` and the others ok.
- [ ] **Step 5: Host test first** in `host/test_sync.py`: `test_jobs_rows_started` — a job with `"startedAt":
  1794145062163` gives `"started": 1794145062`; one without has no `started`; `test_fit_rows_counts_started` — rows
  with `started` stay under `MAX_LINE`. Run `python3 -m unittest discover -s host -p 'test_*.py'`: fails.
- [ ] **Step 6: Implement** in `jobs_rows`: `started` from `number(job.get("startedAt"))` // 1000 when present.
  Run the host tests: pass.
- [ ] **Step 7: Build** `~/.platformio/penv/bin/pio run -e advance-hmi`: succeeds, no new warnings.
- [ ] **Step 8: Commit** "Jobs card: waiting chats first, then the newest started" (job_info.h,
  job_order.*, card_jobs.*, job_watch.h, serial_link.cpp, sync.py, test_job_order.c, run.sh, test_sync.py).

### Task 2: The list that scrolls

**Files:**
- Modify: `src/ui/card_jobs.h`, `src/ui/card_jobs.c`
- Modify: `src/app/screen_power.c` (`show_clock(true)` calls `card_jobs_scroll_top()`)
- Modify: `src/app/serial_link.cpp` (`handle_jobs`: free heap in a `debug_printf` when the count changes)
- Modify: `host/sync.py` (`--demo [N]`, N default 12, at most 64)
- Test: `host/test_sync.py`; by hand on the board

**Interfaces:**
- Consumes: `job_order` (Task 1).
- Produces: `#define JOBS_MAX 64` (replaces `JOBS_ROWS`); `void card_jobs_scroll_top(void)`; rows as an array of
  `job_row_t` grown on demand, each remembering its job's `id` (copied, `ID_MAX` 96 bytes).

- [ ] **Step 1: Host test first**: `test_demo_rows` — the demo's jobs message for N=12 has 12 rows with at least one
  waiting, working, done, one with an ssh `machine`, and one whose name and id hold a `"` (for the board's JSON in
  Task 7); N=64 has 64 rows and stays under `MAX_LINE`. Split
  `demo()` so the rows come from `demo_rows(n)`. Run: fails; implement; run: passes.
- [ ] **Step 2: Implement the list** in card_jobs.c: a child `list` object of the card at (0, 124), 432 x 212,
  transparent, no padding, vertical scroll only (`LV_DIR_VER`), scrollbar `LV_SCROLLBAR_MODE_ACTIVE`, flex column of
  rows (48 px each, the existing row look). Rows made as needed and kept hidden when not used (never deleted),
  up to `JOBS_MAX`. Remove `jb_more` and its "+N more" text. `jb_empty` stays. Dividers: under every shown row but
  the last. `age_tick` refreshes every shown row.
- [ ] **Step 3: Back to the top**: `card_jobs_scroll_top()` = `lv_obj_scroll_to_y(list, 0, LV_ANIM_ON)`. A one-shot
  timer `TOP_AFTER_MS` restarted on the list's `LV_EVENT_PRESSED` and `LV_EVENT_SCROLL_END` calls it. In
  `card_jobs_set`: before filling, note the ids of the rows that were waiting; after, if a waiting job's id wasn't
  among them, scroll to the top. screen_power.c `show_clock(true)` calls it (without animation is fine there).
- [ ] **Step 4: Build**: succeeds, no new warnings.
- [ ] **Step 5: By hand on the board** (stop the service, flash, `python3 host/sync.py --demo 12`): 4 rows and the
  top of a fifth; a swipe scrolls with a scrollbar; 30 s later it's back at the top; double tap on the banner shows
  the clock and the list is at the top when the cards come back; `--demo 64` with `DESK_DEBUG=1`: the free heap
  printed and the list scrolls to the 64th. Note the free heap in the commit message.
- [ ] **Step 6: Commit** "Jobs card: every chat in a list that scrolls".

### Task 3: The lock on the board

**Files:**
- Modify: `src/ui/alert.h` (include `<stdbool.h>`, not lvgl.h: its API has no LVGL type)
- Modify: `src/app/screen_power.h`, `src/app/screen_power.c`
- Modify: `src/app/job_watch.h`, `src/app/job_watch.c`
- Modify: `src/app/serial_link.cpp` (`handle_lock`)
- Test: `test/native/test_job_watch.c` (fakes for `alert_show`, `alert_hide`, `alert_is_showing`,
  `screen_power_wake`, defined in the test file), `test/native/run.sh`

**Interfaces:**
- Produces: `void screen_power_set_locked(bool locked)`, `bool screen_power_locked(void)`;
  `void job_watch_set_locked(bool locked)`.
- `seen_t` keeps, per job: `id`, `state`, `since`, `name[48]`, `machine[48]`, `bool owed` (a waiting alert not yet
  seen).

- [ ] **Step 1: Write the failing native tests** (`test_job_watch.c`, `run_test test_job_watch src/app/job_watch.c`):
  - `test_alert_hidden_by_lock_comes_back`: A starts waiting -> alert shown for A; lock -> `alert_hide` called; unlock
    -> `alert_show(ALERT_WAITING, …, "A", …)`.
  - `test_waiting_while_locked_shows_on_unlock`: lock; A starts waiting -> no `alert_show`; unlock -> shown for A.
  - `test_answered_while_locked_not_shown`: lock; A waits; A working again; unlock -> no `alert_show`.
  - `test_done_while_locked_dropped`: lock; A done; unlock -> no `alert_show`.
  - `test_longest_asking_first`: lock; B waits since 200, A since 100 -> unlock shows A.
  - `test_closed_with_ok_not_owed`: A waits, alert shown, OK (the fake's `showing` set false), lock, unlock -> no
    second `alert_show`.
  Run: fails (no `job_watch_set_locked`).
- [ ] **Step 2: Implement job_watch**: while locked, `show()` marks the job owed instead of showing; locking with a
  waiting alert showing marks its job owed, then `alert_hide()`; a job no longer waiting isn't owed; unlocking shows
  the owed waiting job with the smallest `since` (its name and detail from `seen_t`). Run tests: pass.
- [ ] **Step 3: Implement screen_power**: `screen_power_set_locked(true)`: `show_clock(true)` and keep it: while
  locked `screen_power_wake`, `screen_power_set_busy(true)`, `screen_power_pc_present(true)` and `tick` don't bring
  the cards; `false`: `screen_power_wake()`. Not saved.
- [ ] **Step 4: serial_link**: `lock` -> `bool locked = doc["locked"] | false;` then on lock
  `job_watch_set_locked(true); screen_power_set_locked(true);`, on unlock `screen_power_set_locked(false);
  job_watch_set_locked(false);` (cards first, then the owed alert over them). A `lock` without a bool `locked`:
  "bad lock: needs locked". Starts unlocked at boot.
- [ ] **Step 5: Native tests and build**: all ok, build without new warnings.
- [ ] **Step 6: By hand**: `printf '{"type":"lock","locked":true}\n' > /dev/desk-display` (service stopped) shows the
  clock; a touch does nothing; `--demo` meanwhile shows no alert; `locked:false` brings the cards and the waiting
  alert.
- [ ] **Step 7: Commit** "Screen: the clock while the laptop is locked, the waiting alert back on unlock".

### Task 4: `sync.py` follows the lock

**Files:**
- Modify: `host/sync.py` (a `LockWatch`, `send_all`, the main loop)
- Test: `host/test_sync.py`

**Interfaces:**
- Produces: `parse_locked_hint(line: str) -> bool | None` (a `gdbus monitor` line: `'LockedHint': <true>` -> True,
  `<false>` -> False, else None); `display_session(run=subprocess.run) -> str | None` (`loginctl show-user <uid> -p
  Display --value`, else the user's session of Class `user` with a Type `wayland` or `x11`); `class LockWatch` with
  `fileno()`, `locked: bool`, and `poll() -> bool` (True when `locked` changed). Without logind, a session or
  `gdbus`: `locked` False forever, one `warn_once`.

- [ ] **Step 1: Write the failing tests**: `test_parse_locked_hint` (a real-looking PropertiesChanged line with
  `{'LockedHint': <true>}`, one with `<false>`, one about `IdleHint` -> None); `test_display_session` (a fake `run`
  answering "2"; one answering "" then a `list-sessions` table); `test_lockwatch_without_logind_is_unlocked` (a fake
  `run` raising `FileNotFoundError`: `locked` False, no exception, `fileno()` None);
  `test_lockwatch_reads_every_2s_when_monitor_ends` (the monitor's output at its end: `fileno()` None, and `poll()`
  reads the value through the fake `run` at most once per 2 s of a fake clock); `test_send_all_sends_lock_first`
  (a fake board records types: the first after `time` is `lock`, before `jobs`). Run: fail.
- [ ] **Step 2: Implement**: the start value from `loginctl show-session <id> -p LockedHint --value`; changes from a
  `gdbus monitor --system --dest org.freedesktop.login1 --object-path <the session's path>` subprocess (path from
  `gdbus call --system --dest org.freedesktop.login1 --object-path /org/freedesktop/login1 --method
  org.freedesktop.login1.Manager.GetSession <id>`) read in the main `select`; if it ends, read the value every 2 s
  instead. `send_all` sends `time`, then `lock`, then jobs; a change sends `lock` at once. Run tests: pass.
- [ ] **Step 3: By hand**: run `python3 host/sync.py --debug`, lock the laptop (Meta+L), unlock: `-> {"type":"lock",
  "locked":true}` then `false` in the output, within 2 s each.
- [ ] **Step 4: Commit** "sync.py: the display follows the laptop's lock".

### Task 5 (Talpy Den): `startedAt` in `jobs.json`

**Files (in the Talpy Den worktree):**
- Modify: `package/contents/code/talpy_den.py:9002-9017` (`place_jobs`)
- Modify: `docs/how-it-works.md` (The jobs file: example and the `startedAt` line), `CHANGELOG.md` (Unreleased)
- Test: `tests/test_talpy_den.py` (next to the `publish_jobs` tests around line 2364)

- [ ] **Step 1: Write the failing test** `test_jobs_carry_when_the_chat_started`: a session with `startedAt` 1000
  placed for this computer has `"startedAt": 1000`; for host `lab` with `offset_ms=500`, 1500; a session without it,
  `None`. Run `python3 -m unittest discover -s tests -p test_talpy_den.py -k started`: fails.
- [ ] **Step 2: Implement** like `since`: `int(started + offset_ms) if started is not None else None`.
- [ ] **Step 3: Docs**: in the example JSON and the field list ("`startedAt` is when the chat started, on this
  computer's clock, null when not known"); CHANGELOG: one line under Unreleased saying `jobs.json` has each chat's
  start time (for Talpy Den Display's order).
- [ ] **Step 4: Run** `python3 -m unittest discover -s tests`: all pass.
- [ ] **Step 5: Commit** in Talpy Den's style: "Jobs file: when each chat started".

### Task 6 (Talpy Den): `jump.py --job <id>`

**Files (in the Talpy Den worktree):**
- Modify: `package/contents/code/jump.py` (USAGE line, `job_id_ok`, `jump_job_id`, `main`)
- Modify: `docs/how-it-works.md` (Bring a chat to the front from another program), `CHANGELOG.md`
- Test: `tests/test_jump.py`

**Interfaces:**
- Produces: `job_id_ok(job_id) -> bool` (the Global Constraints' check and the shape
  `[<host>:]<profile>/<id>/<kind>[/<more>]`); `jump_job_id(home, extra_dirs, job_id) -> (bool, str)`; `main`:
  `jump.py --job ID` prints `{"ok": …, "message": …}`. `--job` takes exactly the next argument and nothing else may
  follow.
- Consumes: `jump`, `remote.last_answers`, `remote.check_host`, `remote.open_remote`, `jump_remote_vscode`,
  `jump_remote_terminal`, `cs.valid_id`, `cs.discover_profiles`, `cs.matches_profile`.

- [ ] **Step 1: Write the failing tests** (every case patches `subprocess.Popen`, `subprocess.run`,
  `jump.open_terminal` and `jump.raise_window` to fail the test if called, except where a case expects a call):
  - `test_job_refuses_bad_ids`: for each of `"a;b/c/live"`, `".claude/$(x)/live"`, `` ".claude/`x`/live" ``,
    `".claude/a b/live"`, `".claude/\"x/live"`, `".claude/x\n/live"`, `"../x/y/live"`, `"-x/y/live"`, `"x"*301`,
    `"lab:../a/b/live"`, `"bad/host:.claude/a/live"`: `(False, "Not a chat id")` or "That chat isn't listed any
    more", and nothing called.
  - `test_job_local_does_what_a_click_does`: `.claude-work/<uuid>/live` with that profile in the fixture -> `jump`
    called with `(<uuid>, ".claude-work")`.
  - `test_job_local_unknown_profile`: an id whose profile no `discover_profiles` has -> "That chat isn't listed any
    more", nothing called.
  - `test_job_host_open_command`: a `hosts/lab.json` answer with a session of key `.claude/<uuid>/job` and an
    `openCommand` -> `remote.open_remote("lab", <that command>)` called.
  - `test_job_host_terminal`: no `openCommand`, not VS Code -> `jump_remote_terminal("lab", <name>, …)` called.
  - `test_job_host_vscode_not_allowed`: a `claude-vscode` session with `jump_remote_vscode` returning
    `(None, "…?")` -> `(False, "Open it from Talpy Den first: it asks before connecting VS Code")`.
  - `test_job_unknown_host_id`: `nohost:.claude/<uuid>/live` with no `hosts/nohost.json` -> "That chat isn't listed
    any more".
  - `test_main_job_takes_one_argument`: `main(["--job", "x", "--allow-vscode"])` exits with USAGE.
  Run `python3 -m unittest discover -s tests -p test_jump.py -k job`: fail.
- [ ] **Step 2: Implement**: check first (`job_id_ok`); a host id: for each `hosts/*.json` name that passes
  `check_host`, compare `host + ":" + session["key"]` with the id exactly (no parsing of the host out of the id);
  found, do what the widget's `remoteShow` does (`openCommand` if a string -> `remote.open_remote`; `entrypoint ==
  "claude-vscode"` -> `jump_remote_vscode(host, sessionId, cwd)` with `allow` False, None mapped to the message above;
  else `jump_remote_terminal(host, name, vscodeTerminal, cwd)`). A local id: split on `/`, the profile must match a
  discovered profile, the id pass `cs.valid_id`, the kind be `live`, `job` or `chat` (an optional `/shared` after);
  then `jump(home, extra_dirs, id, profile)`. Check `remote.attach_line` quotes every part (it does today: keep a
  test that a container command with a `;` stays one quoted word). `extra_dirs`: the widgets' extra config folders
  if `jump.py` can read them from the settings the widgets share (`save_settings`' file); if it can't, none, and
  Step 4's docs say chats of those profiles aren't found by `--job`.
- [ ] **Step 3: Run** the job tests, then `python3 -m unittest discover -s tests`: all pass.
- [ ] **Step 4: Docs**: the paragraph from the spec (command, answer, the three code folders, that it runs only
  what Talpy Den keeps for a listed chat and never answers its questions); CHANGELOG line.
- [ ] **Step 5: Commit** "jump.py --job: bring a listed chat to the front from another program".

### Task 7: Tap a row or Open on PC (the board)

**Files:**
- Create: `src/app/pc_open.h`, `src/app/pc_open.cpp`
- Modify: `src/ui/card_jobs.h`, `src/ui/card_jobs.c` (rows tappable, press colour, "Opening…", the result)
- Modify: `src/ui/alert.h`, `src/ui/alert.c` ("Open on PC")
- Modify: `src/app/job_watch.h`, `src/app/job_watch.c` (`job_watch_init`: the alert's button opens its job)
- Modify: `src/app/serial_link.cpp` (`opened`), `src/main.cpp` (callbacks)
- Test: `test/native/test_job_watch.c` (the alert's button), by hand

**Interfaces:**
- Consumes: `screen_power_locked()` (Task 3); the row ids (Task 2).
- Produces: `bool pc_open_request(const char *id)` (false: empty id, locked, or one under way; else prints
  `{"type":"open","id":…}` serialized by ArduinoJson plus `\n`, calls `card_jobs_opening(id)`, starts the 20 s
  timer); `void pc_open_answer(const char *id, bool ok, const char *message)` (ignored unless `id` is the pending
  one; clears it; `card_jobs_open_result`); timeout -> `card_jobs_open_result(id, false, "No answer from the PC")`.
- Produces: `typedef void (*card_jobs_open_cb_t)(const char *id); void card_jobs_on_open(card_jobs_open_cb_t cb);
  void card_jobs_opening(const char *id); void card_jobs_open_result(const char *id, bool ok, const char *message);`
  (both do nothing if no shown row has that id); `typedef void (*alert_open_cb_t)(void); void
  alert_on_open(alert_open_cb_t cb);` (the button calls cb, then `alert_hide()`); `void job_watch_init(void)`.

- [ ] **Step 1: Failing native test**: `test_open_on_pc_opens_the_alerts_job` — after A's waiting alert, calling the
  callback the fake `alert_on_open` received calls the fake `pc_open_request("A's id")`. Run: fails.
- [ ] **Step 2: Implement job_watch_init** (registers that callback). Run: passes.
- [ ] **Step 3: Implement the rows**: each row clickable (`LV_OBJ_FLAG_CLICKABLE`), `LV_STATE_PRESSED` background
  `COLOR_TILE`, its id copied when filled. On `LV_EVENT_PRESSED` the card keeps the pressed row's id; on
  `LV_EVENT_CLICKED` (LVGL sends none after a scroll) it calls the open callback with that kept id, not the row's
  current one; a row without an id does nothing. "Opening…" and the result replace the row's state text (result in
  `COLOR_DONE` for ok, `COLOR_MUTED` otherwise, `LV_LABEL_LONG_DOT`, width to the row's free right side), then
  `RESULT_MS` later the state text again. The list itself stays clickable for scrolling, so a double tap on it no
  longer reaches the screen (the spec's choice); say so in screen_main.c's comment.
- [ ] **Step 4: Implement the alert's button**: "Open on PC", montserrat 24, 52 px high like OK, width to fit
  (about 180), left of OK with 16 px between; outlined (2 px border and text in the kind's colour, no fill) so OK
  stays the main one; on both kinds.
- [ ] **Step 5: serial_link** `opened`: `pc_open_answer(doc["id"], doc["ok"] | false, doc["message"] | "")`; a
  missing id: "bad opened: needs id". `main.cpp`: `card_jobs_on_open(open_row)` with a wrapper calling
  `pc_open_request`, and `job_watch_init()`.
- [ ] **Step 6: Native tests and build**: all ok, no new warnings.
- [ ] **Step 7: By hand** with the service stopped and a terminal on the port (`python3 -c` with pyserial or `pio
  device monitor`): a tap prints the `open` line once; a scroll prints nothing; a fake answer
  `{"type":"opened","id":"<id>","ok":true,"message":""}` shows "Opened" for 4 s; no answer: "No answer from the PC"
  after 20 s; a second tap meanwhile prints nothing; Open on PC on an alert prints its job's line and closes it;
  the demo chat with a `"` prints a line that `python3 -c 'import json,sys; json.loads(sys.argv[1])'` accepts;
  hold a finger on a row, send a jobs message that swaps it with its neighbour, lift: the line names the chat that
  was pressed; tap, then send a jobs message without that chat, then its answer: no crash, and the next tap works.
- [ ] **Step 8: Commit** "Jobs card: a tap on a chat, or Open on PC, asks the PC to bring it up".

### Task 8: `sync.py` answers `open`

**Files:**
- Modify: `host/sync.py`
- Test: `host/test_sync.py`

**Interfaces:**
- Consumes: Task 6's `jump.py --job ID` and its one JSON line; Task 4's `LockWatch.locked`.
- Produces: `id_ok(job_id) -> bool` (the Global Constraints' check); `board_request(line: str) -> str | None` (the
  id of an `open` line; None for a line over 1024 bytes, not a JSON object, another type, or a non-string id);
  `code_folder(env: dict, home: Path) -> Path | None` (`DESK_TALPY_CODE`, else the Plasma, GNOME, newest VS Code
  folder from the spec that has `jump.py`); `jump_command(code: Path, job_id: str) -> list[str]` =
  `["python3", "-I", str(code / "jump.py"), "--job", job_id]`; `class Opener` (one worker thread: `start(job_id)
  -> bool`, False while one runs; results on a queue as `(job_id, ok, message)`; `JUMP_TIMEOUT = 15`).

- [ ] **Step 1: Failing tests**: `test_board_request` (a good line -> id; `{"type":"ping"}`, `not json`, a list,
  1025 bytes, `"id": 5` -> None); `test_id_ok` (the bad ids of Task 6 Step 1 -> False; a real local and a host id ->
  True); `test_jump_command_is_a_list` (an id with a `;`… is refused before; a good one is the 5th argument
  exactly); `test_code_folder` (a temp home with only the VS Code folders 1.20.0 and 1.21.0 -> 1.21.0's; the env
  override wins; none -> None); `test_open_answers` with a fake `Opener` and board: id not in the current jobs ->
  "That chat isn't listed any more" and nothing started; locked -> "The PC is locked"; no code folder -> "Talpy Den
  not found"; one running -> "Still opening the last one"; a result `(id, False, "x"*200 + "é")` -> message
  plain ASCII, 80 characters; `jump.py` exit with USAGE (non-zero, no JSON) -> "Talpy Den is too old for this";
  timeout -> "Talpy Den took too long". Run: fail.
- [ ] **Step 2: Implement**, using the original `jobs.json` the last `send_jobs` read (keep it) to check the id.
  The main loop: each board line goes through `board_request`; an id answers through `Opener` or at once; the
  Opener's results are sent as `opened` in the loop. Log `open <id>: ok|failed <message>` only.
- [ ] **Step 3: Run** the host tests: pass.
- [ ] **Step 4: By hand, end to end** (Talpy Den's branch installed or `DESK_TALPY_CODE` pointing at its worktree's
  `package/contents/code`): tap a local chat (its Konsole tab comes up, "Opened"), a host's chat (its terminal),
  a finished chat that's gone (its message), Open on PC on a waiting alert; lock: the clock, no taps; unlock.
- [ ] **Step 5: Commit** "sync.py: a tap on the display brings the chat up, through Talpy Den".

### Task 9: Docs

**Files:**
- Modify: `README.md` (the Jobs bullet: every chat, the order, scrolling, a tap; the Clock face bullet: locked; the
  protocol table: `started`, `lock`, `opened`, and a line for what the board sends, `open`; Privacy: a tap sends the
  chat's id, the PC answers what happened; the lock keeps names off the screen)
- Modify: `design/desk-display-layout.html` (`jb_row` note and positions as a list, `jb_more` removed, the pressed
  row and "Opening…", the alert's `al_open` next to `al_ok`)
- Test: none new

- [ ] **Step 1: Edit** both; check every protocol line in README against serial_link.cpp and sync.py.
- [ ] **Step 2: Full check**: `sh test/native/run.sh`, `python3 -m unittest discover -s host -p 'test_*.py'`,
  `~/.platformio/penv/bin/pio run -e advance-hmi`: all pass, no new warnings.
- [ ] **Step 3: Commit** "Docs: every chat in a list, the lock, a tap to open" and push the branch.
