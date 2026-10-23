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
- ENV: percussion on/off; LFO: 2nd/3rd; FX: fast/slow; SAVE: store settings.
- MIDI in (USB/TRS): channels 1/2/3 = upper/lower/pedal; CC 1/11 swell,
  70–78 drawbars, 80/81/82 percussion, program change = preset.

The scanner vibrato and the preamp/reverb/Leslie chain are the next slices.

## Build and test

From the repository root:

```sh
make build-host
make test-host
make compare      # the engine against tools/setbfree-host/refrender
```

The platform, host simulator and browser emulator are in `../fm1-base/`. See
the top-level `README.md` and `PLANO.md`.
