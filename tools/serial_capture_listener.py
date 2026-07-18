#!/usr/bin/env python3
"""
Listens to the ESP32's serial output, forwards your keystrokes to the board
(so you can still press 'r' + Enter to trigger a recording), and automatically
saves each completed BEGIN_CAPTURE/END_CAPTURE block straight to its own WAV
file — no manual copy-paste or per-file script runs needed.

This REPLACES `idf.py monitor` while you're collecting data — only one
program can own the serial port at a time, so close the monitor first.

Setup (once):
    pip install pyserial

Usage:
    python serial_capture_listener.py --port COM3 --label play
    python serial_capture_listener.py --port COM3 --label stop

Files are written as <outdir>/<label>_01.wav, <label>_02.wav, ... continuing
from whatever numbers already exist in the output folder, so you can stop and
resume without overwriting anything. Board logs still print to this terminal
as normal; type 'r' + Enter any time to trigger the next capture.
"""
import argparse
import glob
import os
import re
import struct
import sys
import threading
import wave

import serial

SAMPLE_RATE = 16000


def next_index(outdir, label):
    existing = glob.glob(os.path.join(outdir, f"{label}_*.wav"))
    nums = []
    for path in existing:
        m = re.search(rf"{re.escape(label)}_(\d+)\.wav$", os.path.basename(path))
        if m:
            nums.append(int(m.group(1)))
    return (max(nums) + 1) if nums else 1


def write_wav(path, samples):
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SAMPLE_RATE)
        w.writeframes(struct.pack(f"<{len(samples)}h", *samples))


def stdin_forward_thread(ser):
    """Forward keystrokes typed in this terminal to the board (mimics idf.py monitor)."""
    while True:
        try:
            line = input()
        except EOFError:
            return
        ser.write((line + "\n").encode("utf-8", errors="ignore"))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", required=True, help="Serial port, e.g. COM3")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--label", required=True, help="Class label, e.g. play or stop")
    ap.add_argument("--outdir", default=".", help="Directory to write WAV files into")
    args = ap.parse_args()

    os.makedirs(args.outdir, exist_ok=True)
    idx = next_index(args.outdir, args.label)

    ser = serial.Serial(args.port, args.baud, timeout=1)
    print(f"Connected to {args.port} @ {args.baud}. Label='{args.label}', next file index={idx}.")
    print("Type 'r' + Enter to trigger a capture. Ctrl+C to quit.\n")

    threading.Thread(target=stdin_forward_thread, args=(ser,), daemon=True).start()

    in_capture = False
    samples = []

    try:
        while True:
            raw = ser.readline()
            if not raw:
                continue
            try:
                line = raw.decode("utf-8", errors="replace").rstrip("\r\n")
            except Exception:
                continue

            if line.startswith("BEGIN_CAPTURE"):
                in_capture = True
                samples = []
                print(f"[capturing...]")
                continue

            if line.startswith("END_CAPTURE"):
                in_capture = False
                out_path = os.path.join(args.outdir, f"{args.label}_{idx:02d}.wav")
                write_wav(out_path, samples)
                print(f"[SAVED] {out_path} ({len(samples)} samples, {len(samples)/SAMPLE_RATE:.2f}s)\n")
                idx += 1
                continue

            if in_capture:
                for tok in line.split(","):
                    tok = tok.strip()
                    if tok:
                        try:
                            samples.append(int(tok))
                        except ValueError:
                            pass
                continue

            # Not part of a capture — just echo the board's normal log output.
            print(line)

    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        ser.close()


if __name__ == "__main__":
    main()
