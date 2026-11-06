import json
import time

import serial

ser = serial.Serial()  # no port yet = not opened
ser.port = "/dev/ttyUSB0"
ser.baudrate = 115200
ser.timeout = 1
ser.open()

with open("/etc/localtime", "rb") as f:
    tz = f.read().strip().split(b"\n")[-1].decode()

time.sleep(2)  # let the board finish booting, in case opening the port reset it
ser.reset_input_buffer()
ser.write(b"\n")


while True:  # outer: every 10 minutes
    msg = {"epoch": int(time.time()), "tz": tz}
    ser.write((json.dumps(msg) + "\n").encode())

    while True:  # inner: read the answer
        raw = ser.readline()  # b"" after 1 s of silence
        if not raw:
            break  # leaves only this inner loop
        print("board:", raw.decode(errors="replace").strip())

    time.sleep(5)
