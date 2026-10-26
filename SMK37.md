# fm1-wheel on the M-VAVE SMK-37 Pro

The SMK-37 Pro is a 37-key MIDI keyboard around the same JieLi AC791N (WL82)
family as the FM-1. It has 16 RGB pads, four faders, eight encoders, a 240×240
screen, USB-MIDI and a TRS MIDI **out** jack. fm1-wheel has a second build
target for it; the app, the tonewheel engine and the drawbar state are shared
with the FM-1.

**Status:** the SMK-37 Pro build compiles and passes the host tests. It has
**not been flashed or tested on a physical SMK-37** yet. Treat the first
install as a bring-up, with a full flash backup.

## Build

Prerequisites are the same as for the FM-1: Linux or WSL2 with the JieLi
toolchain, the AC79 SDK, Python 3 and Pillow. On macOS the existing Colima +
Rosetta setup works.

```sh
make smk37                 # M-VAVE SMK-37 Pro (identity SMK-37 Pro_...)
make test-host-smk         # host simulator: pads, faders, 37 keys
```

The package is written to:

```text
app/build/smk37/fm1-wheel-smk37.fwsc
```

For a release:

```sh
fm1-base/tools/build.py app --board smk37 --model pro --release 0.2.0
# app/build/smk37/fm1-wheel-smk37-0.2.0.fwsc
```

The FM-1 build is unchanged:

```sh
make device                # app/build/fm1-wheel.fwsc
```

One `build.py` drives both boards. `BOARD=smk37` selects the SMK board layer
and the SMK package layout; it does not select a second app.

## Before installing

1. Make a **full 1 MiB flash backup** of the unit with `jl-uboot-tool` or
   M-Flasher, and read it twice to compare.
2. Check that `0xC1224` in the dump reads `trimdat`. This is the factory
   calibration of the keys and pads.
3. Keep M-VAVE's stock SMK-37 Pro package at hand for the return to stock.
4. Do not write `0x00000–0x03FFF` (boot head), `0xC0000–0xC1FFF`
   (Bluetooth/calibration), `0xF4000–0xFDFFF` (stock resources) or
   `0xFF000+` (key/MAC).

Install with the command-line installer:

```sh
python3 fm1-base/tools/fm1_install.py app/build/smk37/fm1-wheel-smk37-0.2.0.fwsc
```

The web installer in this repository is currently the FM-1 installer. For the
SMK use the command above, or install the same `.fwsc` through a compatible
M-VAVE SMK updater. Never install an FM-1 package on an SMK, or an SMK package
on an FM-1.

The first SMK boot prepares its own settings area and leaves the stock
calibration, Bluetooth pairing and key data untouched.

## Controls

| Control | Function |
|---|---|
| 37 keys | C3–C6; the middle 27 keys are the FM-1's F3–G5 |
| K1–K4 | the four drawbar values on the current page |
| K5 | MASTER volume (a virtual pot) |
| K6 | SELECT: drawbar page |
| K7 | ALGORITHM: UPPER/LOWER/PEDAL |
| K8 | PRESETS: factory registration |
| FADER BANK / pads 9–11 | fader page A, B or C |
| Faders 1–4 | the four values on the current page |
| Pads 1–8 (top row) | factory registrations 1–8 |
| Pads 12–16 | reserved |
| GLOBE | turns the faders/pads on and off; keys keep playing |
| OCTAVE− / OCTAVE+ | transpose the keybed |
| FX / SEQ / PLAY | drive, reverb, rotor |
| ENV / ARP / LFO | percussion, 2nd/3rd, vibrato |
| PATCH / PARA / BT | home, edit/manual, save |
| SCALE / CHORD / NOTE REPEAT | SEL, ENV, LFO equivalents |

## Drawbars on the SMK

The values are still **0–8**, with **8 = fully out**, and the screen bars fill
from the top down.

| Page | Fader 1 | Fader 2 | Fader 3 | Fader 4 |
|---|---|---|---|---|
| A | 16' | 5⅓' | 8' | 4' |
| B | 2⅔' | 2' | 1⅗' | 1⅓' |
| C | 1' | percussion on/off | 2nd/3rd | fast/slow |

- Faders use **soft takeover**: after a page or preset change, a fader moves
  the bar only when it reaches the bar's current value. Nothing jumps.
- Pads 1–8 recall the same eight factory registrations as the PRESETS control.
- Pads 9–11 select page A/B/C; the FADER BANK button does the same in sequence.
- `GLOBE` toggles the fader/pad controls. The 37 keys continue to play, unlike
  the FM-1's drawbar-key mode.
- The three page-C faders switch percussion when they cross the midpoint.
- Page C's 1' bar is fader 1.

This mapping is implemented in `app/smk37.c`; the FM-1 key-pair drawbar mode is
unchanged in `app/app.c`.

## Differences and limits

- **No TRS MIDI in** on the SMK: its TRS jack is an output and PH8 is a matrix
  row. The FM-1 TRS-in driver is disabled for this board. USB-MIDI in and out
  work. TRS MIDI out is not implemented yet.
- The pitch and mod wheels are not assigned yet.
- The 37-key range extends beyond the FM-1's upper manual at the highest
  octave setting; keys above MIDI note 96 are ignored there.
- The browser emulator is the FM-1 panel. The SMK build is tested in the host
  simulator and needs a physical unit for display, pads, faders, audio and
  update checks.
- The SMK package is for the **SMK-37 Pro**. Elite and MKE-P37 packages need a
  shorter identity than the current 0.2.0 release format; they are not built
  yet.

## Platform and credits

The SMK-37 board layer is the MPL-2.0 fm1-base work by M. Geisler
(genkigenki/makkuro), including `boards/smk37`, `BOARD_SMK37` and the
`plat_smk_*` platform calls. The hardware map and porting notes are documented
by the [fm1-to-smk37](https://github.com/genkigenki/fm1-to-smk37) kit; its text
is not copied here. The SMK-37 series is not an M-VAVE or JieLi product.

fm1-wheel's app remains **GPL-3.0-or-later**. The MPL-2.0 board layer is
compatible with it. See [LICENSING.md](LICENSING.md).

## Hardware bring-up checklist

After the backup and install:

- boot, screen and backlight;
- all 37 keys, including the top and bottom octaves;
- pads 1–8, 9–11 and their colours;
- faders 1–4 on pages A, B and C, including soft takeover;
- all encoders, buttons and their LEDs;
- USB-MIDI in/out; the USB product name;
- audio output, master volume and CPU load;
- save, power-cycle and reload;
- update from the running firmware and recovery;
- read `0xC1224` again: `trimdat` must still be there.
