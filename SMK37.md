# fm1-wheel on the M-VAVE SMK-37 Pro

The SMK-37 Pro is a 37-key MIDI keyboard around the same JieLi AC791N (WL82)
family as the FM-1. It has 16 RGB pads, four faders, eight encoders, a 240×240
screen, USB-MIDI and a TRS MIDI **out** jack. fm1-wheel has a second build
target for it; the app, the tonewheel engine and the drawbar state are shared
with the FM-1.

**Status:** the SMK-37 Pro build is installed on a physical SMK-37
(2026-11-10); the controls are under test. Treat the first install as a
bring-up, with a full flash backup and the stock package at hand.

## Build

Prerequisites are the same as for the FM-1: Linux or WSL2 with the JieLi
toolchain, the AC79 SDK, Python 3 and Pillow. On macOS the existing Colima +
Rosetta setup works.

```sh
make smk37                 # M-VAVE SMK-37 Pro (identity SMK-37 Pro_...)
make test-host-smk         # host simulator: pads, faders, 37 keys
make web-smk               # SMK-37 panel emulator (WebAssembly)
```

The package is written to:

```text
app/build/smk37/fm1-wheel-smk37.fwsc
```

For a release:

```sh
fm1-base/tools/build.py app --board smk37 --model pro --release 0.2.2
# app/build/smk37/fm1-wheel-smk37-0.2.2.fwsc
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
python3 fm1-base/tools/fm1_install.py app/build/smk37/fm1-wheel-smk37-0.2.2.fwsc
```

The site's SMK-37 page has a separate browser installer at `/smk37/install/`
(Chrome or Edge, Web MIDI). The command above and a compatible SMK updater are
the fallbacks. The FM-1 installer at `/install/` is not for the SMK; never
install an FM-1 package on an SMK, or an SMK package on an FM-1.

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
| Faders 1–4 | the last four bars: 2', 1⅗', 1⅓', 1' (reversed: minimum = fully out) |
| Pads 4–8 (top right) | push the first five bars in: 16', 5⅓', 8', 4', 2⅔' |
| Pads 12–16 (bottom right) | pull the same five bars out |
| Pads 1–3, 9–11 | factory registrations 1–6 |
| FADER BANK / LEFT / RIGHT | unused: the faders are not paged |
| GLOBE | unused: the pad/fader drawbars are always on |
| OCTAVE− / OCTAVE+ | transpose the keybed |
| FX / SEQ / PLAY | drive, reverb, rotor |
| ENV / ARP / LFO | percussion, 2nd/3rd, vibrato |
| PATCH / PARA / BT | home, edit/manual, save |
| SCALE / CHORD / NOTE REPEAT | SEL, ENV, LFO equivalents |

## Drawbars on the SMK

The values are still **0–8**, with **8 = fully out**, and the screen bars fill
from the top down.

| Control | Bars |
|---|---|
| Faders 1–4 | 2', 1⅗', 1⅓', 1' (reversed) |
| Pads 4–8 (top row, right) | 16', 5⅓', 8', 4', 2⅔' (push) |
| Pads 12–16 (bottom row, right) | the same five (pull) |

- The four faders are always the last four bars; no page change moves them.
- The 10 rightmost pads are the first five bars as five pairs: the top pad
  pushes a bar in, the bottom pad pulls it out, one step per tap, like the
  FM-1's black/white key pairs.
- The pads show the bar: the top pad lights with the travel left to push, the
  bottom with the value out. Pads 1–3 and 9–11 recall presets 1–6; the K8
  PRESETS encoder has all eight.
- Faders are absolute and reversed: the minimum position is fully out (8), the
  maximum fully in (0); moving one sets the bar directly.
- K1–K4 still edit the four bars on the current page; K6/SELECT changes that
  page. `GLOBE` no longer toggles the pad/fader drawbars (they are always on),
  and the 37 keys continue to play, unlike the FM-1's drawbar-key mode.
- Percussion is on the ENV/ARP/LFO buttons; the old page-C fader toggles are
  gone.

This mapping is implemented in `app/smk37.c`; the FM-1 key-pair drawbar mode is
unchanged in `app/app.c`.

## Differences and limits

- **No TRS MIDI in** on the SMK: its TRS jack is an output and PH8 is a matrix
  row. The FM-1 TRS-in driver is disabled for this board. USB-MIDI in and out
  work. TRS MIDI out is not implemented yet.
- The pitch and mod wheels are not assigned yet.
- The 37-key range extends beyond the FM-1's upper manual at the highest
  octave setting; keys above MIDI note 96 are ignored there.
- The SMK-37 browser emulator is at `/smk37/emu/` on the site (or
  `make web-smk`); it runs the host simulator with the SMK panel. It is not a
  cycle-accurate CPU emulator, and the physical unit is still needed for
  display, pads, faders, audio and update checks.
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
- pads 1–3 and 9–11 (presets) and 4–8, 12–16 (drawbars), and their colours;
- faders 1–4 (the last four bars), absolute;
- all encoders, buttons and their LEDs;
- USB-MIDI in/out; the USB product name;
- audio output, master volume and CPU load;
- save, power-cycle and reload;
- update from the running firmware and recovery;
- read `0xC1224` again: `trimdat` must still be there.
