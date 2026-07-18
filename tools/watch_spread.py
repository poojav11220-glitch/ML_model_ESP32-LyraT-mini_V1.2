#!/usr/bin/env python3
"""Print only L_spread values live, with elapsed time, so mic response to
sound can be checked by eye (does the number jump when you speak?)."""
import argparse
import re
import time
import serial

ap = argparse.ArgumentParser()
ap.add_argument("--port", required=True)
ap.add_argument("--baud", type=int, default=115200)
ap.add_argument("--seconds", type=float, default=20)
args = ap.parse_args()

pat = re.compile(r"L_spread=(\d+)")

ser = serial.Serial(args.port, args.baud, timeout=1)
start = time.time()
while time.time() - start < args.seconds:
    raw = ser.readline()
    if not raw:
        continue
    line = raw.decode("utf-8", errors="replace").rstrip("\r\n")
    m = pat.search(line)
    if m:
        t = time.time() - start
        val = int(m.group(1))
        bar = "#" * min(val // 200, 80)
        print(f"[{t:5.1f}s] L_spread={val:6d} {bar}")
ser.close()
