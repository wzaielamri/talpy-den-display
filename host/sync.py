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
send(
    ser,
    "jobs",
    updated=time.strftime("%H:%M:%S"),
    rows=[
        {"name": "desk-display-gadget", "state": "waiting", "detail": "3m"},
        {"name": "quick-chat-next", "state": "working", "detail": "12m"},
        {"name": "fix-serial-parser", "state": "working", "detail": "41m"},
        {"name": "hourly-forecast-ui", "state": "working", "detail": "2m"},
        {"name": "release-notes", "state": "done", "detail": "5m ago"},
        {"name": "talpy-den-docs", "state": "done", "detail": "1h ago"},
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
