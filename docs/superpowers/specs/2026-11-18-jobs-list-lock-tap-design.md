# Jobs card: every chat in a list, the display follows the laptop's lock, a tap brings a chat up

## Why

The jobs card shows four chats and "+N more in Talpy Den" for the rest; the owner wants to see all of them, the
newest on top. A display that shows more should also show less when the owner isn't there: while the laptop is
locked it shows the clock and no chat names. And a chat on the display should be one tap from the laptop: a tap on
it brings that chat to the front there, as a click on its row in Talpy Den does.

Privacy is the frame for all of it. Talpy Den is public, and its `jobs.json` leaves prompts, replies and what a chat
asks out on purpose ("only what a status shows"). Nothing here changes that: no details screen, no conversation on
the board. What reaches the board stays what reaches it today (names, states, times, where a chat runs), plus a
start time; what the board sends back is a chat's id.

## Goals

1. **Every chat in a list**, ordered: waiting chats first (the one asking longest on top), then all the others by
   when they started, the newest first. A row stays where it is when its chat goes from working to done.
2. **Follow the lock:** the laptop locks, the display goes to the clock face and stays there; it unlocks, the
   display comes back.
3. **Tap to open on the PC:** a tap on a chat's row, or **Open on PC** on an alert, brings that chat to the front on
   the laptop, through Talpy Den's own `jump.py`.

## Non-goals

- No details of a chat on the board (what it does, prompts, replies, its task list), no live view, no answering
  permission prompts.
- Talpy Den's own order of chats doesn't change.
- No order for the clock face's chat line (it counts, as now).

## Anti-goals (a failure even if it works)

- Rows jumping around when a chat changes state.
- A chat's name on the display while the laptop is locked.
- A scroll read as a tap (a chat opened that the owner only scrolled past).
- A tap holding up `sync.py` (the board's messages late while `jump.py` runs).
- Talpy Den writing anything more private than it writes today.

## Approaches

**Order:**
- **Waiting first, then newest-started first (picked by the owner).** Talpy Den's steady order (it sorts by start, so
  rows don't move with state) turned over, so the newest isn't scrolled out of sight.
- Talpy Den's order exactly (newest last): with about 4 rows in view, the newest chat is usually below them.
- By latest change (`since`): no Talpy Den change needed, but a row jumps each time its chat changes state.

**Showing more than 4:** a scrolling list of today's 48 px rows (picked). Smaller rows still stop at about 6 and are
hard to read across the desk; pages need buttons.

**Where the order is made:** on the board (picked), so any program that feeds the board the documented way gets the
same order. `sync.py` sorting would leave that to every sender.

**A chat's details on tap:** dropped. Talpy Den would have to write prompts and replies for another program, which it
is built not to do. Tap-to-open replaces it: the details are then on the laptop's own screen.

**Bringing a chat up:** Talpy Den's `jump.py` (picked). It already knows Konsole's tabs, tmux, VS Code, background
chats and KWin; doing that again in `sync.py` would be a second copy of it. Talpy Den documents the command, so it is
an interface like `jobs.json` (the owner's call). Two ways to call it:
- **A new `jump.py --job <id>` that does what a click on that chat's row does (picked by the owner).** It takes the
  id from `jobs.json` and finds everything else in what Talpy Den already keeps (for a host's chat, that host's last
  answer, which has each chat's open command). `jobs.json` then gains only `startedAt`.
- `jump.py`'s existing commands, with `sessionId` and `profile` added to `jobs.json`: a host's chat would go to any
  terminal with an ssh connection to that host (maybe the wrong tab, still reported as opened), and a chat in VS
  Code's terminal on a host not at all.

**Knowing the lock:** logind's `LockedHint` for the owner's graphical session (picked: Plasma and GNOME both set it).
`org.freedesktop.ScreenSaver` on the session bus is the fallback if `LockedHint` turns out unreliable on Plasma.

## Talpy Den (its repository, a branch of its own)

- `place_jobs` (`package/contents/code/talpy_den.py`) adds `startedAt` to each job: when the chat started (the
  session's `startedAt`, ms, moved by `offset_ms` as `since` is; `null` when not known). Not private; no `version`
  change ("new fields may come").
- `jump.py --job <id>`, `<id>` a job's `id` in `jobs.json`: does what a click on that chat's row in the widget does,
  and prints one JSON line, `{"ok": bool, "message": str}`, as `jump.py` does.
  - This computer's chat (no `host:` in front): what `jumpTo` does, `jump(<session>, <profile>)`. A chat the widget
    would show in its Talpy Chat window opens as `jump.py` opens it (a terminal): only the widget can open that window.
  - An SSH host's or a container's chat (`<host>:` in front): the chat with that key in the host's last answer
    (`hosts/<host>.json`, kept by `remote.py --status`), then what `remoteShow` does: its `openCommand` through
    `remote.py --open`; a VS Code chat through `--remote-vscode`; else `--remote-terminal` with its name, and its
    `vscodeTerminal` and folder when it has them.
  - A host's VS Code chat with no VS Code window connected: the widget asks you once first (that host's VS Code
    server could then read your clipboard). `--job` never answers that for you: it says so (`ok` false, "Open it
    from Talpy Den first: it asks before connecting VS Code") and does nothing.
  - It never runs a command from its caller: only what Talpy Den keeps for a chat it lists. An id it doesn't list
    (or a host it doesn't check: the same name rules as `place_fresh`) is `ok` false, "That chat isn't listed any
    more".
  - The extra config folders of the widgets' shared settings count, as for the widget (the builder checks how
    `jump.py` gets them; if it can't, a known limit in the docs).
- `docs/how-it-works.md`, under "The jobs file": `startedAt`, and a paragraph **Bring a chat to the front from another
  program**: `python3 -I <code>/jump.py --job <id>`, its answer, and `<code>`: the Plasma widget's
  `~/.local/share/plasma/plasmoids/org.wzaielamri.talpyden/contents/code`, the GNOME extension's
  `~/.local/share/gnome-shell/extensions/talpy-den@wzaielamri.github.io/code`, the VS Code extension's
  `~/.vscode/extensions/wzaielamri.talpy-den-<version>/vendor/code`.
- Tests next to the existing `publish_jobs` ones (`tests/test_talpy_den.py`) and for `--job` (a local id, a host's
  each way, an unknown id, a bad host name, the VS Code question), and a CHANGELOG line.

### Talpy Den's safeguards (the owner's requirement: no injection, no privacy leak)

**No injection.** Nothing in the id ever becomes a command, an option or a path; it is only compared with what
Talpy Den lists.
- The id is checked before anything else: one argument, at most 300 characters, the shape Talpy Den writes
  (`[<host>:]<profile>/<id>/<kind>[/shared]`) in letters, digits and `_ . @ : + -` only (so no whitespace, quote,
  backslash, shell or glob character), no `..`, not starting with `-`; and it must be one of the jobs file's ids.
  Anything else: `ok` false, "Not a chat id" (or "That chat isn't listed any more"), and nothing runs. (The final
  review found a denylist let `*`, `?` and `[` through, which Talpy Den would take as a pattern for a transcript's
  path: it became this allowlist.)
- `--job` takes the next argument as its id and nothing more: an id can't add or reach any other option of `jump.py`
  (no `--allow-vscode`, no `--busy`, no answer to any question Talpy Den asks).
- The host part must pass the rules Talpy Den already has for host names (`check_host`, `place_fresh`: no `/`, no
  leading `.` or `-`) before `hosts/<host>.json` is opened; the session or chat id must pass `cs.valid_id`; the
  profile must be one `discover_profiles` finds, matched by name, never joined into a path.
- What runs is what a click runs, built by the same functions (`jump`, `open_remote`, the `--remote-*` ones), with
  argument lists or every part `shlex.quote`d, as `open_remote` does today. A host's stored `openCommand` runs only on
  that host, inside the quoted `ssh … <host> <command>`; `--job` takes it only if it is a string. The container path
  (`attach_line`) is checked for the same quoting, and fixed if it isn't.
- Tests: ids with `;`, `$(…)`, a backtick, a space, quotes, a newline, `../`, a leading `-`, an unknown or bad host,
  and one too long each answer `ok` false with no process started (the test fails if anything is run).

**No privacy leak.**
- `jobs.json` gains only `startedAt`. Talpy Den writes no new file for this.
- `--job`'s message says what happened, and to which window or host ("Switched to the terminal with the ssh
  connection to lab"), never what a chat says: no prompt, reply, question or task.
- For anyone who doesn't use the display, nothing changes: `--job` does something only when it is called.
- On the display's side, `sync.py` runs `jump.py --job` only with an id that is exactly one in the current
  `jobs.json` and passes the same character check, as an argument list. It forwards the answer's message as plain
  ASCII cut to 80 characters, and logs only the id, `ok` and that message. The README's Privacy section says what a
  tap sends (the chat's id) and gets back.

The display works before this ships: without `startedAt` the order falls back to `since`, and a `jump.py` without
`--job` (it prints its usage and exits non-zero) makes a tap answer "Talpy Den is too old for this".

## The display

### 1. Every chat in a list

**Order** (a pure C function, its own file, native tests like `test_banner_text`):
- waiting chats first, by `since`, oldest first (unknown `since` counts as 0, as in Talpy Den);
- then every other chat, working or done alike, by `started`, newest first; a chat without `started` uses its
  `since`, one with neither counts as 0 (last);
- equal keys keep the PC's order.

**Card:**
- Under the banner, the rows' place (432 x 212 at y 124 of the card) is a list that scrolls up and down. The rows
  look as today (48 px, dot, name, where it runs, "working • 2h"). About 4½ rows show, the top of the fifth telling
  there are more; a thin scrollbar shows while it scrolls.
- Rows are made as chats come and kept (hidden) when they go, up to 64 (`sync.py`'s `MAX_JOBS`, `job_watch`'s
  `SEEN_MAX`).
- "+N more in Talpy Den" goes. "No jobs right now" stays.
- A new jobs message fills the rows in place; the list keeps where it was scrolled.
- **Back to the top:** 30 s after the last touch on the list, at once when a chat starts waiting, and when the clock
  face shows (so the cards always come back at the top). (30 s: the builder's call, a constant.)
- Each row's "how long" keeps counting every 10 s, as now.

**Protocol:** jobs rows get an optional `started` (Unix s). `sync.py` sends it from `startedAt` (// 1000), as it does
`since`; its `fit_rows` byte budget counts it.

### 2. Follow the lock

**`sync.py`:**
- Finds the owner's graphical session (`loginctl show-user <uid> -p Display`) and watches its `LockedHint`: read at
  the start, then on each change (`gdbus monitor --system` on the session's object, or a read every 2 s if that is
  simpler; the builder's call). Without logind or a session: unlocked, and a warning once.
- Sends `{"type":"lock","locked":true|false}` on each change, and in `send_all` (start, board restart, reconnect).
  Like any message but `power`, it tells the board the PC is there.

**The board, while locked:**
- The clock face, at its 10%, and it stays: a touch doesn't bring the cards back, nor does a chat waiting, "Stay on"
  or the PC coming back. The pull-down panel stays shut (as over the clock now).
- Its chat line stays (counts only: "1 chat needs you"). No alerts (they show a chat's name). `job_watch` goes on
  following the states.
- Unlocked: the cards, as after a touch. A waiting alert you haven't closed with OK is still owed: one the lock hid
  (a chat waited, its alert showed, then the laptop locked itself) and one for a chat that started waiting while
  locked both show on unlock if their chat still waits (the one asking longest, if several). Finished alerts from
  meanwhile are dropped. (Builder's-check call: an alert the lock covered counts as not yet seen.)
- The lock outlasts the PC going away (locked, then suspended: still locked when it's back) and ends only with
  `locked:false`. A board that restarts starts unlocked and is told within a second (`send_all` after "Setup done").
  Not saved to flash.

### 3. Tap to open on the PC

**The board:**
- A row with an id is tappable: pressed, it takes the tile colour; a tap (LVGL's click, which a scroll never sends)
  prints `{"type":"open","id":"<id>"}` on its serial line, and its right side says "Opening…" till the answer.
- The answer `{"type":"opened","id":"<id>","ok":true|false,"message":"…"}` shows there for 4 s: "Opened" in green, or
  the message (cut to the row) in the muted colour; then the row's state text again. No answer in 20 s: "No answer
  from the PC"; an `opened` that comes after that is ignored. (`sync.py` gives up on `jump.py` at 15 s, so a PC that
  answers at all answers first.)
- One open at a time: taps while one is under way do nothing.
- A double tap on the list no longer shows the clock face (as on the weather card); everywhere else it still does,
  the banner included.
- Both alerts get **Open on PC** next to OK (the box's other button, the same size). It sends the alert's job's
  `open`, then closes the alert; the answer shows on that job's row.
- While locked nothing can be tapped (the clock face covers it all).

**`sync.py`:**
- Reads the board's lines as now; one that is a JSON object with `type` `open` is a request (others are logged as
  now).
- Takes only an id that is in the last `jobs.json` it read, and runs `python3 -I <code>/jump.py --job <id>` with the
  id as one argument (an argument list, never a shell).
- `<code>`: `DESK_TALPY_CODE` in the env file, else the first of the three folders above that has `jump.py` (the VS
  Code one: the newest version).
- Runs it without holding up the loop (as the weather fetch), at most one at a time, killed after 15 s ("Talpy Den
  took too long"); answers with its `ok` and `message` (made plain ASCII). A request while one runs is answered at
  once: "Still opening the last one". Not found: "Talpy Den not found"; an id it doesn't know: "That
  chat isn't listed any more"; a `jump.py` without `--job`: "Talpy Den is too old for this"; while locked: "The PC
  is locked".
- Logs "board: open <id>" and the answer.

### Security

What a tap can do is what a click on the chat's row in Talpy Den can do, and only while the laptop is unlocked:
- **The board can't make the PC run anything else.** Anything on the serial line (the board, or a USB device posing
  as it) can only name a chat: `sync.py` drops an id that isn't in the current `jobs.json` and passes the rest as
  one argument, without a shell; `jump.py --job` runs only what Talpy Den keeps for a chat it lists. At worst, one
  of your listed chats comes to the front.
- **Locked, nothing opens**, twice over: the board takes no taps (the clock face), and `sync.py` refuses an `open`
  while it knows the session is locked.
- **Talpy Den's own questions stay Talpy Den's** (a host's VS Code connection): the display never says yes for you.
- **The lock is a privacy screen, not a lock on the board:** locked, the board still gets the chat names (as today),
  it only doesn't show them. If `sync.py` stops while locked, the board stays locked till it hears otherwise; if
  `sync.py` can't read the lock at all (no logind), it says so once and the display behaves as today.
- **No more private data anywhere:** `jobs.json` gains a start time; the board gets nothing new but `started` and
  the answers to its own taps.
- The board's lines are parsed with `json.loads` and bounded (it prints short lines; `sync.py` drops a line over
  1 KB).

### Tests and checks

- Native: the order (waiting by `since`, then by `started` newest first, the fallbacks, ties).
- `sync.py`: unit tests for what's pure (`started` in the rows, the `jump.py` command for a job, finding `<code>`,
  parsing a board line, `LockedHint` lines if `gdbus monitor` is used).
- Talpy Den: `startedAt` (a host's with the offset) and `jump.py --job` (above).
- `sync.py`: an id not in `jobs.json` runs nothing; an `open` while locked runs nothing.
- On the board: `--demo` sends about 12 chats (some waiting, working, done, on hosts) so the list scrolls; free heap
  printed with 64 rows (`DESK_DEBUG`), and nothing breaks then.
- By hand: lock and unlock the laptop; tap a local chat, a host's chat, one gone; Open on PC on an alert; scroll
  without opening anything.

### Docs

README (the jobs bullet, the clock face bullet, the protocol table: `started`, `lock`, and a line for what the board
sends back, `open`; Privacy: a tap sends the chat's id to the PC, nothing more), the layout spec
(`design/desk-display-layout.html`: the list, the pressed and "Opening…" row, the alert's second button), and the
comments in the files touched.

## Left to the builder

Pixel details within the above, LVGL's scroll settings (momentum, elastic ends), the code's structure (which file
holds the list, how `sync.py` runs `jump.py` and watches the lock), and the order of the work (the list, then the
lock, then the tap, each working on its own).
