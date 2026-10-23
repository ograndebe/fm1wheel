# WheelBox

A tonewheel organ firmware for the **M-VAVE FM-1**, after
[setBfree](https://github.com/pantherb/setBfree), on
[fm1-base](https://github.com/charlesvestal/fm1-base) and
[felucca-bsp](https://github.com/hugelton/felucca-bsp).

**Status: Phase 1 complete on the host.** The repository, the platform build,
the host simulator, the setBfree reference tools, the tone generator, scanner,
preamp, reverb and the **Leslie** are in place. The app plays the full
tonewheel chain on the host (27 keys, USB/TRS MIDI, drawbars, percussion,
V1–V3/C1–C3, drive, reverb, slow/fast/stop rotor). The device build works
through Colima + Rosetta; the FM-1 has not been flashed yet. Read
[MANUAL.md](MANUAL.md) for the controls and the drawbar-key mode, and
[PLANO.md](PLANO.md) for the analysis and roadmap.

## Layout

| Path | What |
|---|---|
| `app/` | The firmware app: `app.c` (UI/MIDI), `dsp/organ.c` (the engine), `fm1.toml` |
| `fm1-base/` | Platform, host simulator, browser emulator, installer (submodule, MPL-2.0) |
| `fm1-base/bsp/` | felucca-bsp: chip, board, loader, packaging (submodule, MPL-2.0) |
| `third_party/setBfree/` | The reference DSP (submodule, GPL-2.0-or-later) |
| `tools/setbfree-host/` | Host-only reference renderer and table dumper (GPL-2.0-or-later) |
| `MANUAL.md` | Hardware manual: controls, drawbar-key mode, MIDI, settings |
| `PLANO.md` | Analysis, architecture, risks, roadmap |

## Host quick start

Needs Python 3, a C compiler, Pillow and fontTools. On macOS with Homebrew,
Pillow must be built with `libraqm` (the font generator uses font features):

```sh
brew install libraqm
python3 -m venv .venv
./.venv/bin/pip install Pillow fonttools   # add --no-binary Pillow if raqm is missing
```

Then:

```sh
make build-host          # fm1-base/tools/build.py app --host
make test-host           # app/build/host/wheelbox_host app/tests/basic.txt app/build/host
make web                 # the browser emulator in app/build/emu/
make serve-web           # builds it and serves http://localhost:8790/
make ref                 # setBfree reference renders (tools/setbfree-host/build)
make organ-test          # the same scripts through the WheelBox engine
make compare             # A/B: correlation, null depth, band levels
make tables              # regenerates app/dsp/organ_tables.h
make whirl-tables        # regenerates app/dsp/whirl_tables.h
```

The browser emulator is the same app (screen, panel, keys, audio) compiled to
WebAssembly; it needs Emscripten (`brew install emscripten`). Use it for
interactive work, and the host simulator for automated tests. The page keeps
the flash store in the browser.

```sh
make serve-web
open http://localhost:8790/
```

The reference renderer writes a float32 WAV:

```sh
tools/setbfree-host/refrender tools/setbfree-host/events/demo.txt /tmp/demo.wav
afplay /tmp/demo.wav
```

## Device build

The JieLi toolchain is Linux x86-64. On macOS, Colima works with Rosetta
(`colima start --vz-rosetta`; the build runs `linux/amd64` containers through
Rosetta). Docker Desktop also works. Then:

```sh
fm1-base/tools/get_toolchain.sh
git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 \
    https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK
make device                 # app/build/wheelbox.fwsc
make install                # asks before writing; the FM-1 must be connected
```

The 2026-10-23 build: app 315,520 B, package 611,354 B, `.data+.bss` 77,300 B
of 98,304, pool 180,844 B of 344,064, no soft-double routines.

Flashing is at your own risk. Keep the official firmware and the recovery
tools (`fm1-base/bsp/tools/fm1_rescue.py`, `FM-1-transporter`) at hand. If the
install is interrupted, the FM-1 stays in update mode and `make install`
finishes the write.

## Licence

The firmware as a whole is **GPL-3.0-or-later**: the setBfree-derived DSP is
GPL-2.0-or-later, fm1-base and felucca-bsp are MPL-2.0, and JieLi's SDK files
are Apache-2.0. File-level notices are kept; see [LICENSING.md](LICENSING.md).
"Hammond", "B3" and "Leslie" are trademarks of their owners; WheelBox is not
affiliated with or endorsed by them.
