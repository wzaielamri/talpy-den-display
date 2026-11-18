#!/usr/bin/env python3
"""Feed the desk display over USB serial: Talpy Den's jobs and plan usage, the time, the weather.

    python3 host/sync.py                        # feed the board, with ~/.config/desk-display.env's place
    python3 host/sync.py --lat LAT --lon LON    # another place for this run
    python3 host/sync.py --port /dev/ttyUSB1    # another port
    python3 host/sync.py --dry-run              # print the messages, send nothing
    python3 host/sync.py --debug                # send them and print them too
    python3 host/sync.py --demo [N]             # send made-up examples once, to try the board (N chats, 12)

Your place stays out of the repository, in ~/.config/desk-display.env (the systemd service reads it too);
left empty, there is no weather:

    DESK_LAT=
    DESK_LON=
    DESK_PORT=/dev/ttyUSB0    (optional: by default /dev/desk-display if the udev rule made it, else this)
    DESK_DEBUG=1              (optional: as --debug, also for the service)
    DESK_TALPY_CODE=          (optional: the folder of Talpy Den's jump.py, for a tap on a chat; by default
                               the Plasma widget's, the GNOME extension's or the newest VS Code extension's)

Each message is one JSON line with a "type" (see src/app/serial_link.cpp):
- jobs and usage: from $XDG_RUNTIME_DIR/talpy-den/jobs.json, which Talpy Den writes whenever a chat's
  state or an account's usage changes; this script waits for that file with inotify (no polling) and
  sends only what changed. The board makes its alerts from the jobs itself (job_watch.c). A jobs message
  holds at most MAX_JOBS jobs and MAX_LINE bytes (the board drops a longer line whole): with more, the
  done ones go first, then the working ones.
- time: at the start and every 10 minutes (the board keeps it in its RTC).
- weather and hourly: from Open-Meteo every 15 minutes, for --lat/--lon (none without them), fetched on
  a thread of its own so that a slow network can't hold up the rest. A failed fetch (no network yet at
  login, say) is tried again after 30 s, then 1, 2, 4, 8 and 15 min.
- lock: whether the laptop's screen is locked (logind's LockedHint of your graphical session, watched with gdbus
  monitor, else read every 2 s), at once when it changes and before the jobs whenever everything is sent: locked,
  the board shows the clock and no chat's name.
- ping: when nothing else went out for 10 s: the board takes the PC for away after 60 s of silence.
- opened: the answer to the board's {"type":"open","id":...} (a chat tapped on it): only an id that is exactly one
  of the jobs file's, while unlocked, goes to Talpy Den's jump.py --job (one at a time, on a thread, 15 s at
  most), which brings that chat to the front as a click on its row in Talpy Den does.
When the board restarts (it prints "Setup done") or the port comes back, everything is sent again.
What the board prints goes to this script's output (the journal, for the service): its errors and
"PC back/away", and with DESK_DEBUG 1 in platformio.ini every message it got as well.

Needs pyserial; the rest is Python's own.
"""
import argparse
import ctypes
import ctypes.util
import json
import os
import queue
import re
import select
import struct
import subprocess
import sys
import threading
import time
import unicodedata
import urllib.request
from pathlib import Path

import serial

TIME_EVERY = 600     # s between time messages
WEATHER_EVERY = 900  # s between weather fetches
WEATHER_RETRY = 30   # s after a failed fetch till the next try (doubling each time, up to WEATHER_EVERY)
PING_EVERY = 10      # s of silence before a ping
LOCK_READ_EVERY = 2  # s between reads of the session's lock while gdbus monitor isn't there to say
JUMP_TIMEOUT = 15    # s Talpy Den's jump.py gets to bring a chat up (the board waits 20 s for the answer)
REQUEST_MAX = 1024   # bytes of a board's line that can be a request; a longer one is only logged
ID_MAX = 300         # characters of a jobs file id (Talpy Den's jump.py --job takes no longer)
USAGE_ROWS = 2       # accounts the board's usage card has room for
MAX_LINE = 8000      # bytes of a jobs message: the board drops a longer line whole (SERIAL_LINE_MAX, serial_link.cpp)
MAX_JOBS = 64        # jobs in a jobs message: what the board follows for its alerts (SEEN_MAX, job_watch.c)
NAME_MAX = 40        # characters of a job's name (the card shows about 20)
JOBS_VERSION = 1     # the jobs file's "version" this script reads
OPEN_METEO = ("https://api.open-meteo.com/v1/forecast?latitude={lat}&longitude={lon}"
              "&current=temperature_2m,weather_code,is_day"
              "&hourly=temperature_2m,weather_code,is_day,precipitation_probability"
              "&daily=temperature_2m_max,temperature_2m_min"
              "&forecast_hours=8&forecast_days=1&timeformat=unixtime&timezone=auto")


# ---------- the board ----------

class Board:
    """The serial port, opened again when it goes; or, dry, the terminal."""

    def __init__(self, port, dry, debug=False):
        self.port, self.dry, self.debug, self.ser, self.last_sent = port, dry, debug, None, 0.0

    def connect(self):
        """Open the port (waiting till it's there); True once open."""
        if self.dry:
            return True
        while self.ser is None:
            try:
                self.ser = serial.Serial(self.port, 115200, timeout=0)
                self.ser.reset_input_buffer()
                self.ser.write(b"\n")  # ends a line cut half-way before, on the board
                print(f"connected to {self.port}")
            except serial.SerialException as exc:
                print(f"no board on {self.port} ({exc}); trying again in 5 s")
                time.sleep(5)
        return True

    def close(self):
        if self.ser is not None:
            try:
                self.ser.close()
            except serial.SerialException:
                pass
        self.ser = None

    def send(self, type_, **fields):
        line = json.dumps({"type": type_, **fields}, separators=(",", ":"))  # (ASCII: \u escapes)
        self.last_sent = time.time()
        if self.dry or self.debug:
            print("->", line[:200] + ("..." if len(line) > 200 else ""))
        if self.dry:
            return
        self.ser.write((line + "\n").encode())

    def fileno(self):
        return None if self.dry or self.ser is None else self.ser.fileno()

    def lines(self):
        """What the board printed since the last call."""
        if self.dry or self.ser is None:
            return []
        data = self.ser.read(self.ser.in_waiting or 1)
        self._buf = getattr(self, "_buf", b"") + data
        *done, self._buf = self._buf.split(b"\n")
        return [d.decode(errors="replace").strip() for d in done]


# ---------- Talpy Den's jobs file ----------

def jobs_file():
    """Where Talpy Den writes it (talpy_den.py jobs_file_path)."""
    runtime = os.environ.get("XDG_RUNTIME_DIR")
    if runtime and os.path.isdir(runtime):
        return Path(runtime) / "talpy-den" / "jobs.json"
    return Path.home() / ".cache" / "talpy-den" / "jobs.json"


class FolderWatch:
    """inotify on a folder: Talpy Den writes jobs.json as a new file renamed over the old one, so a
    file moved in (IN_MOVED_TO) is the event. fileno() goes to select(); changed() reads the events."""
    IN_MOVED_TO, IN_CLOSE_WRITE = 0x80, 0x08

    def __init__(self, folder):
        libc = ctypes.CDLL(ctypes.util.find_library("c") or "libc.so.6", use_errno=True)
        self.fd = libc.inotify_init1(os.O_NONBLOCK | os.O_CLOEXEC)
        if self.fd < 0 or libc.inotify_add_watch(self.fd, os.fsencode(str(folder)),
                                                 self.IN_MOVED_TO | self.IN_CLOSE_WRITE) < 0:
            raise OSError(ctypes.get_errno(), f"inotify on {folder}")

    def fileno(self):
        return self.fd

    def changed(self, name):
        """Whether name was among the files written or moved in since the last call."""
        found = False
        try:
            while True:
                data = os.read(self.fd, 4096)
                off = 0
                while off + 16 <= len(data):
                    _, _, _, length = struct.unpack_from("iIII", data, off)
                    found |= data[off + 16:off + 16 + length].rstrip(b"\0").decode(errors="replace") == name
                    off += 16 + length
        except BlockingIOError:
            pass
        return found


# Letters that don't split into a letter and an accent, which the NFKD below would lose: spelled out.
SPELLED = str.maketrans({"ß": "ss", "ẞ": "SS", "Æ": "AE", "æ": "ae", "Œ": "OE", "œ": "oe", "Ø": "O", "ø": "o",
                         "Đ": "D", "đ": "d", "Ð": "D", "ð": "d", "Þ": "Th", "þ": "th", "Ł": "L", "ł": "l",
                         "ı": "i", "Ħ": "H", "ħ": "h"})

warned = set()


def warn_once(message):
    """Print message the first time only: the same trouble in every update would fill the journal."""
    if message not in warned:
        warned.add(message)
        print(message)


def number(value):
    """value if it is a number (a JSON true or false is not), else None."""
    return value if isinstance(value, (int, float)) and not isinstance(value, bool) else None


def plain(text):
    """What the board's fonts can show: printable ASCII ("Übersicht" -> "Ubersicht", "Straße" -> "Strasse").
    Letters of other scripts have no ASCII spelling here, so they go."""
    text = unicodedata.normalize("NFKD", str(text or "").translate(SPELLED))
    return " ".join("".join(c for c in text if 0x20 <= ord(c) < 0x7F).split())  # (no double space where one went)


def machine_of(job):
    """The small line under a job's name: "local", "lab (ssh)", "web (podman)"."""
    host, where = job.get("host") or "", job.get("where") or "local"
    if not host:
        return "local"
    if where == "ssh":
        return f"{host} (ssh)"
    return f"{host.split(':', 1)[-1]} ({where})"  # podman:web -> web, docker:api@lab -> api@lab


URGENCY = {"waiting": 0, "working": 1, "done": 2}


def fit_rows(rows):
    """rows as one message can hold them (MAX_JOBS jobs, MAX_LINE bytes). When there are too many the least
    urgent go: done ones, then working ones, the oldest started first. The card's counts then miss them."""
    budget = MAX_LINE - 100  # (the message's own type and time)
    kept = set()
    def keep_first(i):  # (waiting: the one asking longest first; the others: the newest started first, as the board)
        row = rows[i]
        if row["state"] == "waiting":
            return 0, row.get("since") or 0
        return URGENCY[row["state"]], -(row.get("started") or row.get("since") or 0)
    for i in sorted(range(len(rows)), key=keep_first):
        budget -= len(json.dumps(rows[i], separators=(",", ":"))) + 1
        if len(kept) == MAX_JOBS or budget < 0:
            break
        kept.add(i)
    if len(kept) < len(rows):
        warn_once(f"jobs: {len(rows)} don't fit in one message, the card counts {len(kept)}")
    return [row for i, row in enumerate(rows) if i in kept]


def jobs_rows(data):
    """The jobs message's rows: Talpy Den's idle (its turn is over, the chat open) shows as done."""
    rows = []
    for job in data.get("jobs") or []:
        if not isinstance(job, dict):
            continue
        state = {"working": "working", "waiting": "waiting", "idle": "done", "done": "done"}.get(job.get("state"))
        if not state:
            continue
        name = plain(job.get("name")) or plain(job.get("folder")) or "(unnamed)"
        row = {"id": str(job.get("id") or ""), "name": name[:NAME_MAX].rstrip(), "state": state,
               "machine": plain(machine_of(job))}
        since = number(job.get("since"))
        if since is not None:
            row["since"] = int(since // 1000)
        started = number(job.get("startedAt"))  # (the board's order: the newest started first)
        if started is not None:
            row["started"] = int(started // 1000)
        rows.append(row)
    return fit_rows(rows)


def usage_accounts(data):
    """The usage message's accounts: each account once (the same id on several machines), this
    computer's numbers first (the freshest), up to the card's room."""
    chosen = {}
    for acc in data.get("accounts") or []:
        if isinstance(acc, dict) and (acc.get("id") not in chosen or not acc.get("host")):
            chosen[acc.get("id")] = acc
    out = []
    for acc in sorted(chosen.values(), key=lambda a: bool(a.get("host")))[:USAGE_ROWS]:
        row = {"name": plain(acc.get("name")) or "?", "plan": plain(acc.get("plan"))}
        for w in acc.get("windows") or []:
            if not isinstance(w, dict) or w.get("key") not in ("five_hour", "seven_day"):
                continue
            pct, resets = number(w.get("pct")), number(w.get("resetsAt"))
            if pct is None:
                continue  # (the card shows "--" for a window that isn't there)
            row[w["key"]] = round(pct)
            if resets:
                row[w["key"] + "_resets"] = int(resets // 1000)
        out.append(row)
    return out


def read_jobs(path):
    """The jobs file's content; None while there is none (Talpy Den not running), it's being written, or it
    isn't something this script reads."""
    try:
        data = json.loads(path.read_text())
    except (OSError, ValueError):
        return None
    if not isinstance(data, dict):
        warn_once("jobs file: not a JSON object, ignored")
        return None
    if data.get("version", JOBS_VERSION) != JOBS_VERSION:  # (a writer that names no version is taken for this one)
        warn_once(f"jobs file: version {data.get('version')!r}, this sync.py reads {JOBS_VERSION}: ignored")
        return None
    return data


# ---------- the weather ----------

def hhmm(epoch):
    return time.strftime("%H:%M", time.localtime(epoch))


def rain_summary(hours):
    """One line about the next hours: the first run of hours with rain likely (50% or more)."""
    for i, h in enumerate(hours):
        if (h.get("rain") or 0) >= 50:
            end = i
            while end + 1 < len(hours) and (hours[end + 1].get("rain") or 0) >= 50:
                end += 1
            start = "now" if i == 0 else f"from {hhmm(h['at'])}"
            return f"Rain likely {start} till {hhmm(hours[end]['at'] + 3600)}"
    return "No rain expected"


def weather_messages(data):
    """Open-Meteo's answer as the weather and hourly messages: ({...}, {...})."""
    cur, hourly, daily = data["current"], data["hourly"], data["daily"]
    weather = {"temp": round(cur["temperature_2m"]), "code": int(cur["weather_code"]), "day": bool(cur["is_day"]),
               "hi": round(daily["temperature_2m_max"][0]), "lo": round(daily["temperature_2m_min"][0]),
               "updated": time.strftime("%H:%M")}
    hours = []
    for at, temp, code, day, rain in zip(hourly["time"], hourly["temperature_2m"], hourly["weather_code"],
                                         hourly["is_day"], hourly["precipitation_probability"]):
        hour = {"at": int(at), "temp": round(temp), "code": int(code), "day": bool(day)}
        if rain is not None:
            hour["rain"] = int(rain)
        hours.append(hour)
    return weather, {"updated": weather["updated"], "summary": rain_summary(hours), "hours": hours[:8]}


def fetch_weather(lat, lon):
    url = OPEN_METEO.format(lat=lat, lon=lon)
    with urllib.request.urlopen(url, timeout=15) as reply:
        return weather_messages(json.load(reply))


class WeatherWorker(threading.Thread):
    """Fetches the weather when asked, on a thread of its own: a slow network (a DNS lookup has no timeout at
    all) must not hold up the serial loop with its pings and alerts. What it fetched waits in results."""

    def __init__(self, lat, lon):
        super().__init__(daemon=True)
        self.lat, self.lon = lat, lon
        self.asked = threading.Event()
        self.results = queue.SimpleQueue()

    def run(self):
        retry = None  # s till the next try after a failed one (None: the last one worked, wait to be asked)
        while True:
            self.asked.wait(retry)  # (asked, or the retry's time came)
            self.asked.clear()
            try:
                self.results.put(fetch_weather(self.lat, self.lon))
                retry = None
            except Exception as exc:  # (whatever the network does: this thread must not end)
                # Soon again: at login the network often isn't up yet, and the card would wait 15 min.
                retry = min(retry * 2, WEATHER_EVERY) if retry else WEATHER_RETRY
                print(f"weather: {exc}; again in {retry} s" if retry < 60 else
                      f"weather: {exc}; again in {retry // 60} min")


# ---------- the laptop's lock ----------
#
# logind keeps a LockedHint for each session, which the desktop sets while its screen is locked (Plasma and GNOME
# both do). The board shows the clock while it's set, and no chat's name (the "lock" message).

def parse_locked_hint(line):
    """True or False from a gdbus monitor line that changes LockedHint, else None."""
    m = re.search(r"'LockedHint': <(true|false)>", line)
    return None if m is None else m.group(1) == "true"


def logind(run, *args):
    """What loginctl answers to args, stripped; "" when it can't (no logind, no loginctl, an error)."""
    try:
        done = run(["loginctl", *args], capture_output=True, text=True, timeout=5)
    except (OSError, subprocess.SubprocessError):
        return ""
    return (done.stdout or "").strip() if done.returncode == 0 else ""


def display_session(run=subprocess.run):
    """The id of this user's graphical session: logind's Display for the user, else the user's session of class
    user that runs Wayland or X11; None without one."""
    shown = logind(run, "show-user", str(os.getuid()), "-p", "Display", "--value")
    if shown:
        return shown
    for line in logind(run, "list-sessions", "--no-legend").splitlines():
        words = line.split()
        if len(words) < 2 or words[1] != str(os.getuid()):
            continue
        if (logind(run, "show-session", words[0], "-p", "Class", "--value") == "user"
                and logind(run, "show-session", words[0], "-p", "Type", "--value") in ("wayland", "x11")):
            return words[0]
    return None


class LockWatch:
    """Whether the laptop's screen is locked (locked), and when that changes: a gdbus monitor of the session's
    logind object (fileno() for select) says so at once; without one (it ended, or gdbus is missing), the value is
    read every LOCK_READ_EVERY s. Without logind or a session: never locked, said once."""

    def __init__(self, run=subprocess.run, popen=subprocess.Popen, clock=time.monotonic):
        self.run, self.clock = run, clock
        self.locked, self.proc, self.buf, self.next_read = False, None, b"", 0.0
        self.session = display_session(run)
        if self.session is None:
            warn_once("lock: no logind session found; the display won't follow the laptop's lock")
            return
        self.locked = self.read() is True
        try:
            answer = run(["gdbus", "call", "--system", "--dest", "org.freedesktop.login1", "--object-path",
                          "/org/freedesktop/login1", "--method", "org.freedesktop.login1.Manager.GetSession",
                          self.session], capture_output=True, text=True, timeout=5).stdout or ""
            m = re.search(r"objectpath '(/org/freedesktop/login1/session/[A-Za-z0-9_]+)'", answer)
            if m:
                self.proc = popen(["gdbus", "monitor", "--system", "--dest", "org.freedesktop.login1",
                                   "--object-path", m.group(1)], stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
                os.set_blocking(self.proc.stdout.fileno(), False)
        except (OSError, subprocess.SubprocessError):
            self.proc = None
        if self.proc is None:
            warn_once(f"lock: no gdbus monitor; reading the lock every {LOCK_READ_EVERY} s")

    def read(self):
        """The session's LockedHint now: True, False, or None when it can't be read."""
        value = logind(self.run, "show-session", self.session, "-p", "LockedHint", "--value")
        return {"yes": True, "no": False}.get(value)

    def fileno(self):
        return None if self.proc is None else self.proc.stdout.fileno()

    def poll(self):
        """Look for a change (the monitor's new lines, or a read when due); True when locked changed."""
        if self.session is None:
            return False
        state = None
        if self.proc is not None:
            try:
                data = os.read(self.proc.stdout.fileno(), 4096)
            except BlockingIOError:
                data = None
            except OSError:
                data = b""
            if data == b"":  # it ended: read the value from now on
                warn_once(f"lock: gdbus monitor ended; reading the lock every {LOCK_READ_EVERY} s")
                self.proc.stdout.close()
                self.proc = None
            elif data:
                self.buf += data
                *lines, self.buf = self.buf.split(b"\n")
                for line in lines:
                    hint = parse_locked_hint(line.decode(errors="replace"))
                    state = hint if hint is not None else state
        if self.proc is None and self.clock() >= self.next_read:
            self.next_read = self.clock() + LOCK_READ_EVERY
            state = self.read()
        if state is None or state == self.locked:
            return False
        self.locked = state
        return True


def send_everything(board, tz, locked, send_jobs, weather):
    """What a board that knows nothing needs, the lock before any chat's name: the lock, the time, the jobs and
    usage (send_jobs), and the weather if fetched. The lock is the very first message: a board that took the PC
    for away is locked, and the first message back unlocks it unless it is a lock that says otherwise."""
    board.send("lock", locked=bool(locked))
    board.send("time", epoch=int(time.time()), tz=tz)
    send_jobs()
    if weather:
        board.send("weather", **weather[0])
        board.send("hourly", **weather[1])


# ---------- a tap on a chat ----------
#
# The board prints {"type":"open","id":"<id>"} when a chat is tapped (its row, or "Open on PC" on an alert). This
# script answers {"type":"opened","id":...,"ok":...,"message":...} after Talpy Den's jump.py --job brought it to the
# front (or said why not). The board's line is only trusted to name a chat: an id that isn't exactly one of the
# jobs file's, or hasn't a jobs file id's shape (ID_SHAPE), runs nothing; the rest goes to jump.py as one argument,
# never through a shell, and jump.py again only looks it up.

# [<host>:]<profile>/<id>/<kind>[/shared] in letters, digits and _ . @ : + - (no blank, quote, shell or glob character)
ID_SHAPE = re.compile(r"(?:[A-Za-z0-9_.@][A-Za-z0-9_.@:-]*:)?[A-Za-z0-9_.+-]+/[A-Za-z0-9_.-]+/(?:live|job|chat)(?:/shared)?")
TALPY_CODE = (".local/share/plasma/plasmoids/org.wzaielamri.talpyden/contents/code",
              ".local/share/gnome-shell/extensions/talpy-den@wzaielamri.github.io/code")
NOT_LISTED = "That chat isn't listed any more"


def id_ok(job_id):
    """Whether job_id has a jobs file id's shape (ID_SHAPE), at most ID_MAX, without ".." and not starting with "-"
    (Talpy Den's jump.py --job checks the same)."""
    return (isinstance(job_id, str) and len(job_id) <= ID_MAX and ID_SHAPE.fullmatch(job_id) is not None
            and ".." not in job_id and not job_id.startswith("-"))


def open_log(job_id, ok, message):
    """The log line of an open: the id only if it's a chat id (the board's line may hold anything, a newline too)."""
    shown = job_id if id_ok(job_id) else "(not a chat id)"
    return f"open {shown}: {'ok' if ok else 'failed'} {message}".rstrip()


# What jump.py needs to show a window, from the user's systemd manager: a service started at login, before the
# desktop told the manager its display, hasn't them (KDE and GNOME import them there once they run).
DESKTOP_KEYS = ("DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "XDG_CURRENT_DESKTOP", "XDG_SESSION_DESKTOP",
                "XDG_SESSION_TYPE", "DESKTOP_SESSION", "KDE_FULL_SESSION", "KDE_SESSION_VERSION",
                "DBUS_SESSION_BUS_ADDRESS", "XDG_RUNTIME_DIR")


def desktop_env(base=None, run=subprocess.run):
    """base (this process's environment) with the desktop's display variables as the user's systemd manager has
    them now; base alone without one."""
    env = dict(os.environ if base is None else base)
    try:
        out = run(["systemctl", "--user", "show-environment"], capture_output=True, text=True, timeout=5).stdout
    except (OSError, subprocess.SubprocessError):
        return env
    for line in (out or "").splitlines():
        key, sep, value = line.partition("=")
        if sep and key in DESKTOP_KEYS and not value.startswith("$'"):  # ($'...': quoted, not a plain value)
            env[key] = value
    return env


def board_request(line):
    """The id of a board's open line; None for anything else (a log line, another type, a non-string id, a line
    over REQUEST_MAX bytes, not a JSON object)."""
    if len(line.encode()) > REQUEST_MAX or not line.startswith("{"):
        return None
    try:
        msg = json.loads(line)
    except ValueError:
        return None
    if not isinstance(msg, dict) or msg.get("type") != "open" or not isinstance(msg.get("id"), str):
        return None
    return msg["id"]


def code_folder(env, home):
    """The folder of Talpy Den's jump.py: DESK_TALPY_CODE, else the Plasma widget's, the GNOME extension's, or the
    newest VS Code extension's; None without one."""
    if env.get("DESK_TALPY_CODE"):
        mine = Path(env["DESK_TALPY_CODE"]).expanduser()
        return mine if (mine / "jump.py").is_file() else None
    for folder in TALPY_CODE:
        if (home / folder / "jump.py").is_file():
            return home / folder

    def version(path):  # (.../wzaielamri.talpy-den-1.21.0/vendor/code/jump.py -> (1, 21, 0))
        name = path.parent.parent.parent.name.rsplit("-", 1)[-1]
        return tuple(int(n) if n.isdigit() else -1 for n in name.split("."))
    found = sorted((home / ".vscode/extensions").glob("wzaielamri.talpy-den-*/vendor/code/jump.py"), key=version)
    return found[-1].parent if found else None


def jump_command(code, job_id):
    """Talpy Den's jump.py bringing up the chat of job_id: a list (no shell), the id one word of its own."""
    return ["python3", "-I", str(code / "jump.py"), "--job", job_id]


def handle_open(job_id, listed, locked, code, opener):
    """An open from the board: (ok, message) to answer at once, or None when opener runs jump.py (its answer comes
    from opener.results)."""
    if not id_ok(job_id) or job_id not in listed:
        return False, NOT_LISTED
    if locked:
        return False, "The PC is locked"
    if code is None:
        return False, "Talpy Den not found"
    if not opener.start(job_id, code):
        return False, "Still opening the last one"
    return None


def jump_answer(done):
    """(ok, message) from jump.py's run: its JSON line; a jump.py without --job prints its usage and fails."""
    try:
        answer = json.loads((done.stdout or "").strip().splitlines()[-1])
    except (ValueError, IndexError):
        answer = None
    if done.returncode != 0 or not isinstance(answer, dict):
        if "usage:" in (done.stderr or ""):
            return False, "Talpy Den is too old for this"
        return False, "Talpy Den couldn't open it"
    return bool(answer.get("ok")), str(answer.get("message") or "")


class Opener:
    """Runs jump.py for one open at a time on a thread of its own, so a slow one holds up nothing; each answer goes
    to results as (job_id, ok, message), the message plain ASCII and at most 80 characters (it's shown on a row)."""

    def __init__(self, run=subprocess.run, environment=desktop_env):
        self.run, self.environment, self.results, self.thread = run, environment, queue.Queue(), None

    def start(self, job_id, code):
        if self.thread is not None and self.thread.is_alive():
            return False
        self.thread = threading.Thread(target=self._open, args=(job_id, code), daemon=True)
        self.thread.start()
        return True

    def _open(self, job_id, code):
        try:
            ok, message = jump_answer(self.run(jump_command(code, job_id), capture_output=True, text=True,
                                               timeout=JUMP_TIMEOUT, stdin=subprocess.DEVNULL,
                                               env=self.environment()))
        except subprocess.TimeoutExpired:
            ok, message = False, "Talpy Den took too long"
        except OSError:
            ok, message = False, "Talpy Den not found"
        self.results.put((job_id, ok, plain(message)[:80]))


# ---------- made-up examples (--demo) ----------

def demo_rows(n, now):
    """n made-up chats (1 to MAX_JOBS) for --demo: every state, a host's, and one whose name and id hold a
    quote (the board's open line must escape it). Enough of them make the jobs card's list scroll."""
    first = [  # (name, state, since minutes ago, started minutes ago, machine)
        ("desk-display-gadget", "waiting", 3, 20, "local"),
        ("quick-chat-next", "working", 12, 15, "local"),
        ("fix-serial-parser", "working", 41, 90, "lab (ssh)"),
        ("release-notes", "done", 5, 30, "local"),
        ("talpy-den-docs", "done", 60, 120, "local"),
        ('say "hi" back', "waiting", 1, 50, "web (podman)"),
    ]
    rows = []
    for i in range(max(1, min(n, MAX_JOBS))):
        if i < len(first):
            name, state, since, started, machine = first[i]
        else:
            name, state = f"demo-chat-{i + 1}", ("working", "done", "done")[i % 3]
            since, started, machine = 7 * i, 30 + 9 * i, ("local", "lab (ssh)", "web (podman)")[i % 3]
        rows.append({"id": f'd"{i + 1}' if '"' in name else f"d{i + 1}", "name": name, "state": state,
                     "since": now - since * 60, "started": now - started * 60, "machine": machine})
    return rows


def demo(board, n=12):
    now = int(time.time())
    board.send("time", epoch=now, tz=local_tz())
    board.send("weather", temp=12, hi=14, lo=7, code=2, day=True, updated="14:45")
    board.send("jobs", updated=time.strftime("%H:%M:%S"), rows=demo_rows(n, now))
    board.send("usage", accounts=[
        {"name": "personal", "plan": "Pro", "five_hour": 40, "seven_day": 84,
         "five_hour_resets": now + 3 * 3600, "seven_day_resets": now + 4 * 86400},
        {"name": "work", "plan": "Team", "five_hour": 12, "seven_day": 100,
         "five_hour_resets": now + 2 * 3600, "seven_day_resets": now + 26 * 3600},
    ])
    hour = now // 3600 * 3600
    hours = [{"at": hour + i * 3600, "temp": t, "code": c, "day": d, "rain": r} for i, (t, c, d, r) in enumerate([
        (12, 2, True, 10), (12, 3, True, 20), (11, 61, True, 60), (10, 63, False, 70),
        (9, 61, False, 45), (8, 3, False, 20), (8, 2, False, 10), (7, 0, False, 5)])]
    board.send("hourly", updated="14:45", summary=rain_summary(hours), hours=hours)


# ---------- the loop ----------

def local_tz():
    """This computer's time zone as a POSIX TZ string (the last line of /etc/localtime), for the board."""
    try:
        with open("/etc/localtime", "rb") as f:
            return f.read().strip().split(b"\n")[-1].decode()
    except OSError:
        return ""


def load_config():
    """DESK_LAT, DESK_LON and DESK_PORT from ~/.config/desk-display.env's KEY=value lines, so a run by
    hand finds the same place as the service. What's already in the environment wins."""
    try:
        lines = (Path.home() / ".config" / "desk-display.env").read_text().splitlines()
    except OSError:
        return
    for line in lines:
        key, sep, value = line.strip().partition("=")
        if sep and key.strip().startswith("DESK_"):
            os.environ.setdefault(key.strip(), value.strip().strip("'\""))


def main():
    load_config()
    port = os.environ.get("DESK_PORT") or ("/dev/desk-display" if os.path.exists("/dev/desk-display")
                                           else "/dev/ttyUSB0")
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--port", default=port)
    ap.add_argument("--lat", type=float, default=os.environ.get("DESK_LAT") or None)
    ap.add_argument("--lon", type=float, default=os.environ.get("DESK_LON") or None)
    ap.add_argument("--dry-run", action="store_true", help="print the messages instead of sending them")
    ap.add_argument("--debug", action="store_true", default=os.environ.get("DESK_DEBUG") == "1",
                    help="print every message sent (off by default: a ping every 10 s fills the journal)")
    ap.add_argument("--demo", nargs="?", type=int, const=12, default=None, metavar="N",
                    help="send made-up examples once and stop (N chats, 12 unless given; 64 at most)")
    args = ap.parse_args()

    board = Board(args.port, args.dry_run, args.debug)
    board.connect()
    if args.demo is not None:
        demo(board, args.demo)
        time.sleep(0.5)
        return
    if args.lat is None or args.lon is None:
        print("no --lat/--lon (or DESK_LAT/DESK_LON in ~/.config/desk-display.env): no weather")

    path = jobs_file()
    path.parent.mkdir(mode=0o700, parents=True, exist_ok=True)  # (to watch it before Talpy Den writes)
    watch = FolderWatch(path.parent)
    tz = local_tz()
    sent = {}         # type -> the fields last sent (to send jobs and usage only when they change)
    weather = None    # the last (weather, hourly) fetched
    worker = None     # what fetches it, without holding up the loop
    if args.lat is not None and args.lon is not None:
        worker = WeatherWorker(args.lat, args.lon)
        worker.start()
    next_time = next_weather = 0.0
    lock = LockWatch()
    opener = Opener()
    listed = set()  # the jobs file's ids (send_jobs)

    def answer(job_id, ok, message):
        print(open_log(job_id, ok, message))
        board.send("opened", id=job_id, ok=bool(ok), message=message)

    def send_jobs(force=False):
        nonlocal listed
        data = read_jobs(path)
        if data is None:
            return
        # (the ids a tap may open: exactly those of the jobs file)
        listed = {job["id"] for job in data.get("jobs") or [] if isinstance(job, dict) and isinstance(job.get("id"), str)}
        for type_, key, build in (("jobs", "rows", jobs_rows), ("usage", "accounts", usage_accounts)):
            try:
                fields = {key: build(data)}
            except (AttributeError, KeyError, TypeError, ValueError) as exc:  # (a jobs file of another shape)
                warn_once(f"{type_}: the jobs file isn't what this sync.py reads ({exc!r}), skipped")
                continue
            if force or sent.get(type_) != fields:
                sent[type_] = fields
                extra = {"updated": time.strftime("%H:%M:%S")} if type_ == "jobs" else {}
                board.send(type_, **extra, **fields)

    def send_all():
        """The board knows nothing (it just started, or the port came back): everything again."""
        nonlocal next_time
        send_everything(board, tz, lock.locked, lambda: send_jobs(force=True), weather)
        next_time = time.time() + TIME_EVERY

    send_all()
    while True:
        try:
            ready, _, _ = select.select([f for f in (watch, board, lock) if f.fileno() is not None], [], [], 1.0)
            if watch in ready and watch.changed(path.name):
                send_jobs()
            if lock.poll():
                board.send("lock", locked=lock.locked)
            for line in board.lines():
                job_id = board_request(line)
                if job_id is None:
                    print("board:", line)
                    if line == "Setup done":  # it restarted: it knows nothing
                        send_all()
                    continue
                now_answer = handle_open(job_id, listed, lock.locked, code_folder(os.environ, Path.home()), opener)
                if now_answer is not None:
                    answer(job_id, *now_answer)
            while not opener.results.empty():
                answer(*opener.results.get())

            now = time.time()
            if now >= next_time:
                board.send("time", epoch=int(now), tz=tz)
                next_time = now + TIME_EVERY
            if worker:
                if now >= next_weather:
                    next_weather = now + WEATHER_EVERY
                    worker.asked.set()
                while not worker.results.empty():
                    weather = worker.results.get()
                    board.send("weather", **weather[0])
                    board.send("hourly", **weather[1])
            if now - board.last_sent >= PING_EVERY:
                board.send("ping")
        except (serial.SerialException, OSError) as exc:
            if board.dry:
                raise
            print(f"lost the board ({exc}); reconnecting")
            board.close()
            board.connect()
            send_all()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        sys.exit(0)
