import json
import sys
import time

import serial

ser = serial.Serial()  # no port yet = not opened
ser.port = "/dev/ttyUSB0"
ser.baudrate = 115200
ser.timeout = 1
ser.open()

with open("/etc/localtime", "rb") as f:
    tz = f.read().strip().split(b"\n")[-1].decode()

ser.reset_input_buffer()
ser.write(b"\n")


def send(ser, type_, **fields):
    line = json.dumps({"type": type_, **fields}, separators=(",", ":"))
    ser.write((line + "\n").encode())


send(ser, "weather", temp=50, hi=10, lo=-7, code=2, day=False, updated="14:45")

# Every job, not only 4. The board counts them all, shows waiting ones first,
# and puts the rest in "+N more in Talpy Den". state: "working", "waiting" or "done".
# since: when it got into that state, in whole Unix seconds (Talpy Den's jobs.json
# has milliseconds: // 1000). The board counts the minutes from there by itself.
# machine: where it runs, shown small under the name. jobs.json says "host" and
# "where": "" + "local", "lab" + "ssh", "podman:web" + "podman"; how you
# word it ("local", "lab (ssh)", "web (podman)") is up to you.
# id: jobs.json's "id", the same while the job is listed. The board compares each jobs
# message with the one before and alerts by itself when a job starts waiting (amber, till
# OK) or finishes (green, goes after 15 s): no separate message for that. Here the first
# message already alerts for desk-display-gadget, as it waits.
now = int(time.time())
send(
    ser,
    "jobs",
    updated=time.strftime("%H:%M:%S"),
    rows=[
        {"id": "1", "name": "desk-display-gadget", "state": "waiting", "since": now - 3 * 60, "machine": "local"},
        {"id": "2", "name": "quick-chat-next", "state": "working", "since": now - 12 * 60, "machine": "local"},
        {"id": "3", "name": "fix-serial-parser", "state": "working", "since": now - 41 * 60, "machine": "lab (ssh)"},
        {"id": "4", "name": "hourly-forecast-ui", "state": "working", "since": now - 2 * 60, "machine": "web (podman)"},
        {"id": "5", "name": "release-notes", "state": "done", "since": now - 5 * 60, "machine": "local"},
        {"id": "6", "name": "talpy-den-docs", "state": "done", "since": now - 60 * 60, "machine": "local"},
    ],
)

# Each account's plan usage: jobs.json's "accounts", the "pct" of their windows "five_hour"
# and "seven_day" (40.0 is fine, the board rounds) and their "resetsAt" // 1000, and the plan.
# The card has room for 2 accounts.
send(
    ser,
    "usage",
    accounts=[
        {"name": "personal", "plan": "Pro", "five_hour": 40, "seven_day": 84,
         "five_hour_resets": now + 3 * 3600, "seven_day_resets": now + 4 * 86400},
        {"name": "work", "plan": "Team", "five_hour": 12, "seven_day": 100,
         "five_hour_resets": now + 2 * 3600, "seven_day_resets": now + 26 * 3600},
    ],
)

# The next 8 hours, for the hourly card (tap the weather card): Open-Meteo's hourly
# temperature_2m, weather_code, is_day and precipitation_probability; at = the hour's
# start in Unix seconds. summary: one line about them, your words.
hour = now // 3600 * 3600
send(
    ser,
    "hourly",
    updated="14:45",
    summary="Rain likely from 16:00 to 18:00",
    hours=[
        {"at": hour + i * 3600, "temp": t, "code": c, "day": d, "rain": r}
        for i, (t, c, d, r) in enumerate([
            (12, 2, True, 10), (12, 3, True, 20), (11, 61, True, 60), (10, 63, False, 70),
            (9, 61, False, 45), (8, 3, False, 20), (8, 2, False, 10), (7, 0, False, 5),
        ])
    ],
)

sys.exit()

while True:  # outer: every 10 minutes
    send(ser, "time", epoch=int(time.time()), tz=tz)

    while True:  # inner: read the answer
        raw = ser.readline()  # b"" after 1 s of silence
        if not raw:
            break  # leaves only this inner loop
        print("board:", raw.decode(errors="replace").strip())

    time.sleep(5)
