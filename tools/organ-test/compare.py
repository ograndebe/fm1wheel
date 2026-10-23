#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compare two float32 WAVs: correlation, null depth and 1/3-ish band levels.

  compare.py APP.wav REF.wav [--from 0.5] [--to 2.5]

Both files must be the same sample rate (44.1 kHz) and length (within a
sample). The app render and the setBfree reference are not sample-identical
(the phase accumulators and the click tables differ); the useful numbers are
the correlation, the null depth and the band differences.
"""
import argparse
import struct
import sys

import numpy as np


def read_float_wav(path):
    b = open(path, "rb").read()
    if b[:4] != b"RIFF" or b[8:12] != b"WAVE":
        sys.exit(f"{path}: not a RIFF/WAVE file")
    i = 12
    while i + 8 <= len(b):
        cid = b[i:i + 4]
        size = struct.unpack("<I", b[i + 4:i + 8])[0]
        body = b[i + 8:i + 8 + size]
        if cid == b"fmt ":
            fmt, ch, rate, _, _, bits = struct.unpack("<HHIIHH", body[:16])
            if fmt != 3 or ch != 2 or bits != 32:
                sys.exit(f"{path}: expected float32 stereo")
        elif cid == b"data":
            x = np.frombuffer(body, dtype="<f4").reshape(-1, 2)[:, 0].astype(np.float64)
            return rate, x
        i += 8 + size + (size & 1)
    sys.exit(f"{path}: no data chunk")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("app")
    ap.add_argument("ref")
    ap.add_argument("--from", dest="t0", type=float, default=0.5)
    ap.add_argument("--to", dest="t1", type=float, default=2.5)
    a = ap.parse_args()

    sr_a, x = read_float_wav(a.app)
    sr_r, y = read_float_wav(a.ref)
    if sr_a != sr_r:
        sys.exit(f"sample rate differs: {sr_a} vs {sr_r}")
    n = min(len(x), len(y))
    x, y = x[:n], y[:n]

    best = (-2.0, 0)
    for lag in range(-200, 201):
        if lag >= 0:
            xx, yy = x[lag:], y[:n - lag]
        else:
            xx, yy = x[:n + lag], y[-lag:]
        c = float(np.dot(xx, yy) / np.sqrt(np.dot(xx, xx) * np.dot(yy, yy) + 1e-30))
        if c > best[0]:
            best = (c, lag)
    c, lag = best
    xx = x[lag:] if lag >= 0 else x[:n + lag]
    yy = y[:n - lag] if lag >= 0 else y[-lag:]
    rms = np.sqrt(np.mean(xx * xx))
    null = np.sqrt(np.mean((xx - yy) ** 2))
    print(f"{a.app} vs {a.ref}")
    print(f"  correlation {c:.6f}  lag {lag}  null {20 * np.log10(null / rms + 1e-30):.1f} dB  "
          f"app rms {20 * np.log10(rms + 1e-30):.2f} dBFS")

    s0, s1 = int(a.t0 * sr_a), int(min(a.t1, n / sr_a) * sr_a)
    bands = [(50, 100), (100, 200), (200, 400), (400, 800), (800, 1600),
             (1600, 3200), (3200, 6400), (6400, 12000)]
    levels = []
    for sig in (x, y):
        seg = sig[s0:s1]
        seg = seg * np.hanning(len(seg))
        sp = np.abs(np.fft.rfft(seg))
        fr = np.fft.rfftfreq(len(seg), 1 / sr_a)
        levels.append([20 * np.log10(np.sqrt(np.mean(sp[(fr >= lo) & (fr < hi)] ** 2)) + 1e-30)
                       for lo, hi in bands])
    print("  band diff (app - ref): " + " ".join(f"{d:+.1f}" for d in (np.array(levels[0]) - np.array(levels[1]))))
    print("  bands: " + " ".join(f"{lo}-{hi}" for lo, hi in bands))


if __name__ == "__main__":
    main()
