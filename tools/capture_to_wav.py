#!/usr/bin/env python3
"""
Convert a serial capture dump (from ML_voice.c's dataset-capture mode) into a
WAV file for uploading to Edge Impulse.

Usage:
    1. In idf.py monitor, press 'r' + Enter to record a 1s clip. Copy everything
       from "BEGIN_CAPTURE N" to "END_CAPTURE N" (inclusive or not, either is
       fine) into a text file, e.g. capture1.txt.
    2. python capture_to_wav.py capture1.txt play_01.wav
"""
import sys
import struct
import wave

SAMPLE_RATE = 16000


def parse_capture(path):
    samples = []
    with open(path, "r") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("BEGIN_CAPTURE") or line.startswith("END_CAPTURE"):
                continue
            for tok in line.split(","):
                tok = tok.strip()
                if not tok:
                    continue
                samples.append(int(tok))
    return samples


def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <capture.txt> <output.wav>")
        sys.exit(1)

    in_path, out_path = sys.argv[1], sys.argv[2]
    samples = parse_capture(in_path)

    if not samples:
        print("No samples parsed — check the input file contains the CSV capture lines.")
        sys.exit(1)

    print(f"Parsed {len(samples)} samples ({len(samples) / SAMPLE_RATE:.2f}s)")

    with wave.open(out_path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)  # 16-bit
        w.setframerate(SAMPLE_RATE)
        w.writeframes(struct.pack(f"<{len(samples)}h", *samples))

    print(f"Wrote {out_path}")


if __name__ == "__main__":
    main()
