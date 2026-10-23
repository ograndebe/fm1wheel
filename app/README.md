# app

The WheelBox firmware app.

| File | What |
|---|---|
| `app.c` | UI, keys/MIDI, drawbars/presets, settings; calls the engine in `app_render` |
| `app.h` | the app's half of the fm1-base contract (`APP_NAME`, `APP_BLOCK 128`) |
| `dsp/organ.c/.h` | the setBfree-derived tone generator (Phase 1a: dry) |
| `dsp/organ_tables.h` | generated from setBfree by `tools/setbfree-host/dump_tables`; committed |
| `fm1.toml` | build and site settings (`o2 = ["dsp/*.c"]`) |
| `tests/basic.txt` | host simulator smoke test |
| `docs/MANUAL.md`, `site/landing.md` | draft; rewritten with the final UI |

## Controls (Phase 1a)

- 27 keys: upper manual, F3–G5; OCT- / OCT+ transpose (-2..+2).
- K1..K4: the four drawbars of the current page (page 0 = 16', 5⅓', 8', 4';
  page 1 = 2⅔', 2', 1⅗', 1⅓'; page 2 = 1', percussion on/off, 2nd/3rd, fast/slow).
- SEL: page; EDIT: manual (upper/lower/pedal); PRESET: 8 registrations.
- ENV: percussion on/off; LFO: vibrato/chorus cycle (OFF, V1, C1, V2, C2, V3, C3);
  PLAY: rotor stop/slow/fast; ARP: percussion 2nd/3rd; FX: drive on/off;
  SEQ: reverb 0/30/60/100%; SAVE: store settings.
- MIDI in (USB/TRS): channels 1/2/3 = upper/lower/pedal; CC 1/11 swell,
  70–78 drawbars, 80/81/82 percussion, program change = preset.

The full chain (tonegen, scanner, preamp, reverb, Leslie) is in.

## Drawbar keys (the 27-key "programming" mode)

`GLO` toggles **drawbar keys**. While it is on, the 27 keys do not play:
each flat/natural pair steps one drawbar of the selected manual (`EDIT`
changes U/L/P). External MIDI keeps playing, so a controller can play while
the FM-1 keyboard edits the registration.

| Keys | Drawbar | Keys | Drawbar |
|---|---|---|---|
| F#3 / G3 | 16' | F#4 / G4 | 2' |
| G#3 / A3 | 5 1/3' | G#4 / A4 | 1 3/5' |
| A#3 / B3 | 8' | A#4 / B4 | 1 1/3' |
| C#4 / D4 | 4' | C#5 / D5 | 1' |
| D#4 / E4 | 2 2/3' | | |

The flat (black) key raises the bar; the natural (white) key lowers it.
`F3`, `C4`, `F4` and `C5` are ignored; `D#5`..`G5` are spare. In drawbar mode
the 18 control keys light (flat key bright, natural key dim); the others stay
dark. With the mode off, held keys light as usual.

The full chain is in.

## Build and test

From the repository root:

```sh
make build-host
make test-host
make compare      # the engine against tools/setbfree-host/refrender
```

The platform, host simulator and browser emulator are in `../fm1-base/`. See
the top-level `README.md` and `PLANO.md`.
