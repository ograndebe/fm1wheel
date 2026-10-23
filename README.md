# WheelBox

A tonewheel organ firmware for the **M-VAVE FM-1**, after
[setBfree](https://github.com/pantherb/setBfree), on
[fm1-base](https://github.com/charlesvestal/fm1-base) and
[felucca-bsp](https://github.com/hugelton/felucca-bsp).

**Status: Phase 1a.** The repository, the platform build, the host simulator,
the setBfree reference tools and the **dry tone generator** are in place. The
app plays the organ on the host (27 keys, USB/TRS MIDI, drawbars, percussion,
settings). The scanner vibrato and the preamp/reverb/Leslie chain are the next
slices. The full analysis and roadmap are in [PLANO.md](PLANO.md).

## Layout

| Path | What |
|---|---|
| `app/` | The firmware app: `app.c` (UI/MIDI), `dsp/organ.c` (the engine), `fm1.toml` |
| `fm1-base/` | Platform, host simulator, browser emulator, installer (submodule, MPL-2.0) |
| `fm1-base/bsp/` | felucca-bsp: chip, board, loader, packaging (submodule, MPL-2.0) |
| `third_party/setBfree/` | The reference DSP (submodule, GPL-2.0-or-later) |
| `tools/setbfree-host/` | Host-only reference renderer and table dumper (GPL-2.0-or-later) |
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
make ref                 # setBfree reference renders (tools/setbfree-host/build)
make organ-test          # the same scripts through the WheelBox engine
make compare             # A/B: correlation, null depth, band levels
make tables              # regenerates app/dsp/organ_tables.h
```

The reference renderer writes a float32 WAV:

```sh
tools/setbfree-host/refrender tools/setbfree-host/events/demo.txt /tmp/demo.wav
afplay /tmp/demo.wav
```

## Device build

Needs Docker Desktop (the JieLi toolchain is Linux x86-64), the toolchain and
the AC79 SDK. See `fm1-base/README.md` and `fm1-base/BUILDING.md`:

```sh
fm1-base/tools/get_toolchain.sh
git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 \
    https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK
./.venv/bin/python fm1-base/tools/build.py app
python3 fm1-base/tools/fm1_install.py app/build/wheelbox.fwsc
```

Flashing is at your own risk. Keep the official firmware and the recovery
tools (`fm1-base/bsp/tools/fm1_rescue.py`, `FM-1-transporter`) at hand.

## Licence

The firmware as a whole is **GPL-3.0-or-later**: the setBfree-derived DSP is
GPL-2.0-or-later, fm1-base and felucca-bsp are MPL-2.0, and JieLi's SDK files
are Apache-2.0. File-level notices are kept; see [LICENSING.md](LICENSING.md).
"Hammond", "B3" and "Leslie" are trademarks of their owners; WheelBox is not
affiliated with or endorsed by them.
