#!/usr/bin/env python3
"""Read serial output for a fixed duration and print it. No TTY needed."""
import argparse
import time
import serial

ap = argparse.ArgumentParser()
ap.add_argument("--port", required=True)
ap.add_argument("--baud", type=int, default=115200)
ap.add_argument("--seconds", type=float, default=20)
args = ap.parse_args()

ser = serial.Serial(args.port, args.baud, timeout=1)
end = time.time() + args.seconds
while time.time() < end:
    raw = ser.readline()
    if raw:
        print(raw.decode("utf-8", errors="replace").rstrip("\r\n"))
ser.close()
