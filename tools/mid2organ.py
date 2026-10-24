#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Isolate the organ tracks of a MIDI file and render them through WheelBox.

The renderer is tools/organ-test/render, the same engine the app/emulator use.
The selected tracks are merged into one performance (upper manual, with notes
24..35 on the pedal) and can also be rendered as individual stems.

  tools/mid2organ.py FILE.mid --out renders/song [--stems] [--seconds 30]
                       [--drive 80] [--rotor fast] [--vib c1] [--reverb 40]

Needs mido and numpy (the repository's .venv has them).
"""
import argparse
import os
import re
import struct
import subprocess
import sys
import wave

import mido
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RENDER = os.path.join(ROOT, "tools", "organ-test", "render")


def is_organ(track):
    progs = {m.program for m in track if m.type == "program_change"}
    name = (track.name or "").lower()
    return bool(progs & {16, 17}) or any(k in name for k in ("drawba", "percus", "organ"))


def track_label(i, track):
    progs = sorted({m.program for m in track if m.type == "program_change"})
    return f"{i}: {track.name or '(no name)'}  programs={progs}"


def target(note):
    """(channel, note): 1 upper, 3 pedal; keep 24..96, transpose other octaves."""
    while note < 24:
        note += 12
    while note > 96:
        note -= 12
    if note < 36:
        return 3, note
    return 1, note


def write_script(path, selected, args, duration):
    mid = mido.MidiFile(args.midi)
    new = mido.MidiFile(type=1, ticks_per_beat=mid.ticks_per_beat)
    new.tracks.append(mid.tracks[0])
    for i in selected:
        new.tracks.append(mid.tracks[i])

    active = {}
    events = []
    now = 0.0
    tempo = 500000
    selected_tracks = [mid.tracks[0]] + [mid.tracks[i] for i in selected]
    for msg in mido.merge_tracks(selected_tracks):
        now += mido.tick2second(msg.time, mid.ticks_per_beat, tempo)
        if msg.type == "set_tempo":
            tempo = msg.tempo
            continue
        if duration and now > duration:
            break
        if msg.type not in ("note_on", "note_off"):
            continue
        if msg.type == "note_off" or msg.velocity == 0:
            ch, note = target(msg.note)
            key = (ch, note)
            n = active.get(key, 0)
            if n:
                n -= 1
                active[key] = n
                if n == 0:
                    events.append((now, f"noteoff {ch} {note}"))
        else:
            ch, note = target(msg.note)
            key = (ch, note)
            if active.get(key, 0) == 0:
                events.append((now, f"noteon {ch} {note} {max(1, msg.velocity)}"))
            active[key] = active.get(key, 0) + 1

    lines = [
        f"drawbars U {args.upper}",
        f"drawbars L {args.lower}",
        f"drawbars P {args.pedal}",
        "swell 127",
        f"perc {args.perc}",
        f"drive {args.drive}",
        f"reverb {args.reverb}",
        f"leslie {args.rotor}",
    ]
    if args.vib != "off":
        lines += ["vib route upper", f"vib {args.vib}"]
    else:
        lines += ["vib off"]
    lines.append("wait 200")       # let the output gain settle before the first note

    last = 0.0
    for t, ev in events:
        ms = int(round(t * 1000.0))
        if ms > last:
            lines.append(f"wait {ms - int(last)}")
            last = ms
        lines.append(ev)
    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")
    return len(events), now


def read_float_wav(path):
    b = open(path, "rb").read()
    if b[:4] != b"RIFF" or b[8:12] != b"WAVE":
        raise SystemExit(f"{path}: not a WAV")
    i = 12
    rate = ch = 0
    while i + 8 <= len(b):
        cid = b[i:i + 4]
        size = int.from_bytes(b[i + 4:i + 8], "little")
        body = b[i + 8:i + 8 + size]
        if cid == b"fmt ":
            fmt, ch, rate, _, _, bits = struct.unpack("<HHIIHH", body[:16])
            if fmt != 3 or bits != 32:
                raise SystemExit(f"{path}: expected float32")
        elif cid == b"data":
            x = np.frombuffer(body, dtype="<f4").reshape(-1, ch).astype(np.float64)
            return rate, x
        i += 8 + size + (size & 1)
    raise SystemExit(f"{path}: no data")


def write_pcm16(path, rate, x):
    pcm = np.clip(x * 32767.0, -32768, 32767).astype("<i2")
    with wave.open(path, "wb") as w:
        w.setnchannels(x.shape[1])
        w.setsampwidth(2)
        w.setframerate(int(rate))
        w.writeframes(pcm.tobytes())


def convert(raw, out, normalize):
    rate, x = read_float_wav(raw)
    # The renderer already has the engine's own gain. Do not add the app's 4x
    # soft clip here: on a dense four-track organ mix it saturates. Keep a
    # linear -1 dBFS ceiling; normalize the stems/mix if asked.
    peak = float(np.max(np.abs(x))) if x.size else 0.0
    if peak > 0 and (normalize or peak > 0.89):
        x *= 0.89 / peak
    write_pcm16(out, rate, x)
    os.remove(raw)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("midi")
    ap.add_argument("--out", default=None)
    ap.add_argument("--tracks", default=None, help="comma-separated track numbers (overrides detection)")
    ap.add_argument("--upper", default="888000000")
    ap.add_argument("--lower", default="888000000")
    ap.add_argument("--pedal", default="888000000")
    ap.add_argument("--drive", type=int, default=80)
    ap.add_argument("--reverb", type=int, default=40)
    ap.add_argument("--rotor", choices=("slow", "fast", "stop"), default="slow")
    ap.add_argument("--vib", choices=("off", "v1", "c1", "v2", "c2", "v3", "c3"), default="off")
    ap.add_argument("--perc", choices=("on", "off"), default="off")
    ap.add_argument("--tail", type=int, default=1500)
    ap.add_argument("--seconds", type=float, default=0, help="render only the first N seconds")
    ap.add_argument("--stems", action="store_true", help="render each selected track too")
    ap.add_argument("--normalize", action="store_true", help="normalize each output to -1 dBFS (linear, no soft clip)")
    ap.add_argument("--no-render", action="store_true", help="write the isolated MIDI and scripts only")
    args = ap.parse_args()

    mid = mido.MidiFile(args.midi)
    if args.tracks:
        selected = [int(x) for x in args.tracks.split(",") if x.strip()]
    else:
        selected = [i for i in range(1, len(mid.tracks)) if is_organ(mid.tracks[i])]
    if not selected:
        sys.exit("no organ tracks found; use --tracks")

    base = os.path.splitext(os.path.basename(args.midi))[0]
    base = re.sub(r"[^A-Za-z0-9._-]+", "_", base).strip("_")
    out = args.out or os.path.join(ROOT, "renders", base)
    os.makedirs(out, exist_ok=True)

    print(f"{args.midi}: {len(mid.tracks)} tracks, {mid.length:.1f} s")
    for i in selected:
        print("  " + track_label(i, mid.tracks[i]))

    # the isolated MIDI: conductor + the organ tracks, original channels
    iso = mido.MidiFile(type=1, ticks_per_beat=mid.ticks_per_beat)
    iso.tracks.append(mid.tracks[0])
    for i in selected:
        iso.tracks.append(mid.tracks[i])
    iso_path = os.path.join(out, "hammond.mid")
    iso.save(iso_path)

    scripts = [(iso_path, os.path.join(out, "hammond.txt"), selected, "hammond")]
    if args.stems:
        for i in selected:
            name = re.sub(r"[^A-Za-z0-9._-]+", "_", mid.tracks[i].name or f"track{i}").strip("_")
            scripts.append((iso_path, os.path.join(out, f"{name}.txt"), [i], name))

    for iso_path, script, tracks, name in scripts:
        n, end = write_script(script, tracks, args, args.seconds)
        print(f"  {name}: {n} note events, {end:.1f} s -> {script}")

    if args.no_render:
        return
    if not os.path.exists(RENDER):
        sys.exit(f"{RENDER} missing; run make -C tools/organ-test")

    for iso_path, script, tracks, name in scripts:
        raw = os.path.join(out, name + ".f32.wav")
        wav = os.path.join(out, name + ".wav")
        cmd = [RENDER, script, raw, "--tail", str(args.tail)]
        print("  render:", " ".join(cmd))
        subprocess.run(cmd, check=True)
        convert(raw, wav, args.normalize)
        print("  wrote", wav)


if __name__ == "__main__":
    main()
