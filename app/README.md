# app

The **fm1-wheel** firmware app: a tonewheel organ for the M-VAVE FM-1.

## Base projects

| Project | Role in fm1-wheel | Licence | Link |
|---|---|---|---|
| **setBfree** | tone generator, scanner, preamp, reverb and Leslie algorithms | GPL-2.0-or-later | https://github.com/pantherb/setBfree |
| **fm1-base** | platform layer, host simulator, browser emulator, installer and site tools | MPL-2.0 | https://github.com/charlesvestal/fm1-base |
| **felucca-bsp** | JieLi AC79 chip/board layer, boot, update loader and packaging | MPL-2.0 | https://github.com/hugelton/felucca-bsp |
| **Felucca** | reference firmware and UX; not a dependency | GPL-3.0-only | https://github.com/hugelton/Felucca |
| **JieLi AC79 SDK** | `uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin` fetched by the build | Apache-2.0 | https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK |

The DSP in `dsp/` is a port of setBfree's algorithms. The platform, host
simulator and browser emulator come from fm1-base, which pins felucca-bsp as
`fm1-base/bsp`. The exact licences and obligations are in `../LICENSING.md`.

## Files

| File | What |
|---|---|
| `app.c` | UI, keys/MIDI, drawbars/presets, settings; calls the engine in `app_render` |
| `app.h` | the app's half of the fm1-base contract (`APP_NAME`, `APP_BLOCK 128`) |
| `dsp/organ.c/.h` | 91 tonewheels, key matrix, percussion, scanner routing, drive/reverb/Leslie chain |
| `dsp/vibrato.c/.h` | scanner vibrato/chorus |
| `dsp/preamp.c/.h` | tube preamp / overdrive |
| `dsp/reverb.c/.h` | Schroeder reverb |
| `dsp/leslie.c/.h` | Leslie / rotary speaker |
| `dsp/organ_tables.h`, `dsp/whirl_tables.h` | tables generated from setBfree; committed |
| `fm1.toml` | build and site settings (`o2 = ["dsp/*.c"]`) |
| `tests/basic.txt` | host simulator smoke test |
| `docs/MANUAL.md` | pointer to the hardware manual in the repository root |
| `site/landing.md` | site front page |

## Controls

- 27 keys: upper manual, F3–G5; `OCT-` / `OCT+` transpose (−2..+2).
- `K1`–`K4`: the four drawbars of the current page (page 0 = 16', 5⅓', 8', 4';
  page 1 = 2⅔', 2', 1⅗', 1⅓'; page 2 = 1', percussion on/off, 2nd/3rd, fast/slow).
- `SEL`: page; `EDIT` or `ALGO`: manual U/L/P; `PRESET`: eight registrations.
- `ENV`: percussion on/off; `LFO`: vibrato/chorus cycle; `PLAY`: rotor
  stop/slow/fast; `ARP`: percussion 2nd/3rd; `FX`: drive on/off; `SEQ`:
  reverb 0/30/60/100%; `SAVE`: store settings; `HOME`: registration 0.
- `GLO`: drawbar-key mode (below).
- MIDI in (USB/TRS): channels 1/2/3 = upper/lower/pedal; CC 1/11 swell,
  70–78 drawbars, 80/81/82 percussion, 64 rotor, 65 drive, 92/95 vibrato,
  program change = registration.

The full chain (tonegen, scanner, preamp, reverb, Leslie) is in.

## Drawbar keys (the 27-key "programming" mode)

`GLO` toggles **drawbar keys**. While it is on, the 27 keys do not play: each
flat/natural pair steps one drawbar of the selected manual (`EDIT` changes
U/L/P). External MIDI keeps playing, so a controller can play while the FM-1
keyboard edits the registration.

| Keys | Drawbar | Keys | Drawbar |
|---|---|---|---|
| F#3 / G3 | 16' | F#4 / G4 | 2' |
| G#3 / A3 | 5 1/3' | G#4 / A4 | 1 3/5' |
| A#3 / B3 | 8' | A#4 / B4 | 1 1/3' |
| C#4 / D4 | 4' | C#5 / D5 | 1' |
| D#4 / E4 | 2 2/3' | | |

The flat (black) key raises the bar; the natural (white) key lowers it.
`F3`, `C4`, `F4` and `C5` are ignored; `D#5`..`G5` are spare. In drawbar mode
all 18 control keys light equally; the others stay dark. With the mode off,
held keys light as usual.

## Build and test

From the repository root:

```sh
make build-host      # host simulator
make test-host       # app tests
make web             # browser emulator (Emscripten)
make serve-web       # http://localhost:8790/
make compare         # A/B against setBfree
make device          # app/build/fm1-wheel.fwsc
make install         # writes to the connected FM-1
```

The platform, host simulator and browser emulator are in `../fm1-base/`. The
hardware manual is in [`../MANUAL.md`](../MANUAL.md) (pt-BR) and
[`../MANUAL.en.md`](../MANUAL.en.md) (English). See also the top-level
`README.md` and `PLANO.md`.
