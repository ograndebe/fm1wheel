# fm1-wheel

**Site:** https://ograndebe.github.io/fm1wheel/ (Português/English, browser-language detection; SMK-37 at `/pt/smk37/` and `/en/smk37/`) · **Source:** https://github.com/ograndebe/fm1wheel

A tonewheel organ firmware for the **M-VAVE FM-1** and the **M-VAVE SMK-37
Pro**, after [setBfree](https://github.com/pantherb/setBfree), on
[fm1-base](https://github.com/charlesvestal/fm1-base) and
[felucca-bsp](https://github.com/hugelton/felucca-bsp).

**Status: Phase 1 complete on the host.** The repository, the platform build,
the host simulator, the setBfree reference tools, the tone generator, scanner,
preamp, reverb and the **Leslie** are in place. The app plays the full
tonewheel chain on the host (27 keys, USB/TRS MIDI, drawbars, percussion,
V1–V3/C1–C3, drive, reverb, slow/fast/stop rotor). The FM-1 device build works
through Colima + Rosetta; it has not been flashed yet. The SMK-37 Pro build
also compiles and passes its host tests; it has not been flashed either. Read
[MANUAL.md](MANUAL.md) (English) or [MANUAL.pt.md](MANUAL.pt.md) (Português)
for the controls and the drawbar-key mode, [SMK37.md](SMK37.md) for the SMK-37,
and [PLANO.md](PLANO.md) for the analysis and roadmap.

## Layout

| Path | What |
|---|---|
| `app/` | The firmware app: `app.c` (UI/MIDI), `dsp/organ.c` (the engine), `fm1.toml` |
| `fm1-base/` | Platform, host simulator, browser emulator, installer (submodule, MPL-2.0) |
| `fm1-base/bsp/` | felucca-bsp: chip, board, loader, packaging (submodule, MPL-2.0) |
| `third_party/setBfree/` | The reference DSP (submodule, GPL-2.0-or-later) |
| `tools/setbfree-host/` | Host-only reference renderer and table dumper (GPL-2.0-or-later) |
| `MANUAL.md` | Hardware manual (English): controls, drawbar-key mode, MIDI, settings |
| `MANUAL.pt.md` | Hardware manual (Português) |
| `SMK37.md` | SMK-37 Pro build, controls, installation and bring-up |
| `app/smk37.c` | SMK-37 pads, faders, 37-key keybed (shared app) |
| `docs/` | Static site (bilingual landing and manual, installer, emulator, firmware) |
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
make test-host           # app/build/host/fm1-wheel_host app/tests/basic.txt app/build/host
make test-host-smk       # SMK-37 host simulator: 37 keys, pads, faders
make web                 # the browser emulator in app/build/emu/
make web-smk             # the SMK-37 panel emulator in app/build/smk37/emu/
make serve-web           # builds it and serves http://localhost:8790/
make site                # static site in docs/ (GitHub Pages)
make ref                 # setBfree reference renders (tools/setbfree-host/build)
make organ-test          # the same scripts through the fm1-wheel engine
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

### Rendering a MIDI file

`tools/mid2organ.py` isolates the organ tracks of a MIDI file (programs 16/17
or drawbar/percussion names), writes an isolated `.mid`, and renders it through
the same engine the firmware and the emulator use:

```sh
./.venv/bin/python tools/mid2organ.py ~/Downloads/Song.mid \
    --out renders/song --stems
```

Options: `--drive 0..127`, `--rotor stop|slow|fast`, `--vib off|v1..c3`,
`--reverb 0..127`, `--upper/--lower/--pedal 888000000`, `--stems`,
`--seconds N` and `--normalize`. The WAVs are 16-bit stereo; `renders/` is
gitignored. It needs `mido` and `numpy` (the `.venv` has them) and the host
renderer (`make -C tools/organ-test`).

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
make device                 # app/build/fm1-wheel.fwsc
make install                # asks before writing; the FM-1 must be connected
make smk37                  # app/build/smk37/fm1-wheel-smk37.fwsc
make test-host-smk          # the SMK-37 host simulator
```

The 2026-10-26 build: FM-1 app 316,088 B, package 611,354 B, `.data+.bss`
77,364 B of 98,304, pool 180,844 B of 344,064. SMK-37 app 316,856 B, package
652,441 B, `.data+.bss` 78,684 B of 98,304. No soft-double routines.

Flashing is at your own risk. Keep the official firmware and the recovery
tools (`fm1-base/bsp/tools/fm1_rescue.py`, `FM-1-transporter`) at hand. If the
install is interrupted, the FM-1 stays in update mode and `make install`
finishes the write.

### SMK-37 Pro

`make smk37` builds the same app for the SMK-37 Pro with the SMK board layer
from fm1-base. The package is `app/build/smk37/fm1-wheel-smk37.fwsc`; the
command-line installer understands the SMK identity. The SMK has a protected
factory calibration, so make a full flash backup before the first install.
The SMK installer is at `docs/smk37/install/`; the FM-1 installer remains
`docs/install/`. The command-line installer also understands the SMK package. See
[SMK37.md](SMK37.md) for the controls and the bring-up checklist. The SMK
build is not flashed yet.

## Licence

The firmware as a whole is **GPL-3.0-or-later**: the setBfree-derived DSP is
GPL-2.0-or-later, fm1-base and felucca-bsp are MPL-2.0, and JieLi's SDK files
are Apache-2.0. File-level notices are kept; see [LICENSING.md](LICENSING.md).
"Hammond", "B3" and "Leslie" are trademarks of their owners; fm1-wheel is not
affiliated with or endorsed by them.

## Credits and AI assistance

The fm1-wheel code, DSP port, tests, tools and documentation were developed
with **OpenCode** using **DeepSeek V4.1 Flash**, under human direction and
review. The base projects and their authors are credited in
[`app/README.md`](app/README.md) and in the file headers; the licences are in
[`LICENSING.md`](LICENSING.md).
