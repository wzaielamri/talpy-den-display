#!/usr/bin/env python3
"""Feed the desk display over USB serial: Talpy Den's jobs and plan usage, the time, the weather.

    python3 host/sync.py --lat 0 --lon 0            # the board on /dev/ttyUSB0
    python3 host/sync.py --port /dev/desk-display --lat ... # another port (a udev name, say)
    python3 host/sync.py --dry-run --lat ...                # print the messages, send nothing
    python3 host/sync.py --demo                             # send made-up examples once, to try the board

Each message is one JSON line with a "type" (see src/app/serial_link.cpp):
- jobs and usage: from $XDG_RUNTIME_DIR/talpy-den/jobs.json, which Talpy Den writes whenever a chat's
  state or an account's usage changes; this script waits for that file with inotify (no polling) and
  sends only what changed. The board makes its alerts from the jobs itself (job_watch.c).
- time: at the start and every 10 minutes (the board keeps it in its RTC).
- weather and hourly: from Open-Meteo every 15 minutes, for --lat/--lon (none without them).
- ping: when nothing else went out for 10 s: the board takes the PC for away after 60 s of silence.
When the board restarts (it prints "Setup done") or the port comes back, everything is sent again.

Needs pyserial; the rest is Python's own.
"""
import argparse
import ctypes
import ctypes.util
import json
import os
import select
import struct
import sys
import time
import unicodedata
import urllib.request
from pathlib import Path

import serial

TIME_EVERY = 600     # s between time messages
WEATHER_EVERY = 900  # s between weather fetches
PING_EVERY = 10      # s of silence before a ping
USAGE_ROWS = 2       # accounts the board's usage card has room for
OPEN_METEO = ("https://api.open-meteo.com/v1/forecast?latitude={lat}&longitude={lon}"
              "&current=temperature_2m,weather_code,is_day"
              "&hourly=temperature_2m,weather_code,is_day,precipitation_probability"
              "&daily=temperature_2m_max,temperature_2m_min"
              "&forecast_hours=8&forecast_days=1&timeformat=unixtime&timezone=auto")


# ---------- the board ----------

class Board:
    """The serial port, opened again when it goes; or, dry, the terminal."""

    def __init__(self, port, dry):
        self.port, self.dry, self.ser, self.last_sent = port, dry, None, 0.0

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
        if self.dry:
            print("->", line[:200] + ("..." if len(line) > 200 else ""))
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


def plain(text):
    """What the board's fonts can show: printable ASCII, accents dropped ("Übersicht" -> "Ubersicht")."""
    text = unicodedata.normalize("NFKD", str(text or ""))
    return " ".join("".join(c for c in text if 0x20 <= ord(c) < 0x7F).split())  # (no double space where one went)


def machine_of(job):
    """The small line under a job's name: "local", "lab (ssh)", "web (podman)"."""
    host, where = job.get("host") or "", job.get("where") or "local"
    if not host:
        return "local"
    if where == "ssh":
        return f"{host} (ssh)"
    return f"{host.split(':', 1)[-1]} ({where})"  # podman:web -> web, docker:api@lab -> api@lab


def jobs_rows(data):
    """The jobs message's rows: Talpy Den's idle (its turn is over, the chat open) shows as done."""
    rows = []
    for job in data.get("jobs") or []:
        state = {"working": "working", "waiting": "waiting", "idle": "done", "done": "done"}.get(job.get("state"))
        if not state:
            continue
        row = {"id": job.get("id") or "", "name": plain(job.get("name")) or plain(job.get("folder")) or "(unnamed)",
               "state": state, "machine": plain(machine_of(job))}
        if isinstance(job.get("since"), (int, float)):
            row["since"] = int(job["since"] // 1000)
        rows.append(row)
    return rows


def usage_accounts(data):
    """The usage message's accounts: each account once (the same id on several machines), this
    computer's numbers first (the freshest), up to the card's room."""
    chosen = {}
    for acc in data.get("accounts") or []:
        if acc.get("id") not in chosen or not acc.get("host"):
            chosen[acc.get("id")] = acc
    out = []
    for acc in sorted(chosen.values(), key=lambda a: bool(a.get("host")))[:USAGE_ROWS]:
        row = {"name": plain(acc.get("name")) or "?", "plan": plain(acc.get("plan"))}
        for w in acc.get("windows") or []:
            if w.get("key") in ("five_hour", "seven_day"):
                row[w["key"]] = round(w.get("pct") or 0)
                if w.get("resetsAt"):
                    row[w["key"] + "_resets"] = int(w["resetsAt"] // 1000)
        out.append(row)
    return out


def read_jobs(path):
    try:
        return json.loads(path.read_text())
    except (OSError, ValueError):
        return None  # not there yet (Talpy Den not running), or being written


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


# ---------- made-up examples (--demo) ----------

def demo(board):
    now = int(time.time())
    board.send("time", epoch=now, tz=local_tz())
    board.send("weather", temp=12, hi=14, lo=7, code=2, day=True, updated="14:45")
    board.send("jobs", updated=time.strftime("%H:%M:%S"), rows=[
        {"id": "1", "name": "desk-display-gadget", "state": "waiting", "since": now - 3 * 60, "machine": "local"},
        {"id": "2", "name": "quick-chat-next", "state": "working", "since": now - 12 * 60, "machine": "local"},
        {"id": "3", "name": "fix-serial-parser", "state": "working", "since": now - 41 * 60, "machine": "lab (ssh)"},
        {"id": "4", "name": "release-notes", "state": "done", "since": now - 5 * 60, "machine": "local"},
        {"id": "5", "name": "talpy-den-docs", "state": "done", "since": now - 60 * 60, "machine": "local"},
    ])
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


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--port", default=os.environ.get("DESK_PORT", "/dev/ttyUSB0"))
    ap.add_argument("--lat", type=float, default=os.environ.get("DESK_LAT"))
    ap.add_argument("--lon", type=float, default=os.environ.get("DESK_LON"))
    ap.add_argument("--dry-run", action="store_true", help="print the messages instead of sending them")
    ap.add_argument("--demo", action="store_true", help="send made-up examples once and stop")
    args = ap.parse_args()

    board = Board(args.port, args.dry_run)
    board.connect()
    if args.demo:
        demo(board)
        time.sleep(0.5)
        return
    if args.lat is None or args.lon is None:
        print("no --lat/--lon (or DESK_LAT/DESK_LON): no weather")

    path = jobs_file()
    path.parent.mkdir(mode=0o700, parents=True, exist_ok=True)  # (to watch it before Talpy Den writes)
    watch = FolderWatch(path.parent)
    tz = local_tz()
    sent = {}         # type -> the fields last sent (to send jobs and usage only when they change)
    weather = None    # the last (weather, hourly) fetched
    next_time = next_weather = 0.0

    def send_jobs(force=False):
        data = read_jobs(path)
        if data is None:
            return
        for type_, fields in (("jobs", {"rows": jobs_rows(data)}), ("usage", {"accounts": usage_accounts(data)})):
            if force or sent.get(type_) != fields:
                sent[type_] = fields
                extra = {"updated": time.strftime("%H:%M:%S")} if type_ == "jobs" else {}
                board.send(type_, **extra, **fields)

    def send_all():
        """The board knows nothing (it just started, or the port came back): everything again."""
        nonlocal next_time
        board.send("time", epoch=int(time.time()), tz=tz)
        next_time = time.time() + TIME_EVERY
        send_jobs(force=True)
        if weather:
            board.send("weather", **weather[0])
            board.send("hourly", **weather[1])

    send_all()
    while True:
        try:
            ready, _, _ = select.select([f for f in (watch, board) if f.fileno() is not None], [], [], 1.0)
            if watch in ready and watch.changed(path.name):
                send_jobs()
            for line in board.lines():
                print("board:", line)
                if line == "Setup done":  # it restarted: it knows nothing
                    send_all()

            now = time.time()
            if now >= next_time:
                board.send("time", epoch=int(now), tz=tz)
                next_time = now + TIME_EVERY
            if args.lat is not None and args.lon is not None and now >= next_weather:
                next_weather = now + WEATHER_EVERY
                try:
                    weather = fetch_weather(args.lat, args.lon)
                    board.send("weather", **weather[0])
                    board.send("hourly", **weather[1])
                except (OSError, ValueError, KeyError, IndexError, TypeError) as exc:
                    print(f"weather: {exc}; again in {WEATHER_EVERY // 60} min")
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
