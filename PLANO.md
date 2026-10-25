# Clonewheel firmware for the M-VAVE FM-1 based on setBfree

Repository analysis and execution plan. Date: 2026-10-23.

**Status (2026-10-23):** Phase 0 and Phase 1 are complete on the host (dry +
scanner + preamp + reverb + Leslie).
The repository is assembled (`fm1-base` and `setBfree` as submodules), `app/`
builds and plays in the host simulator, and the tone generator, scanner,
preamp and reverb were ported with A/B against setBfree:

| Test | Correlation | Null | Bands |
|---|---:|---:|---|
| chord (888000000) | 0.99995 | -40 dB | ±0.0 dB |
| percussion (2nd/3rd) | 0.9996 | -31 dB | ±0.0 dB |
| drawbars (changes) | 0.9979 | -24 dB | lows/mids ±0.3 dB; highs -10 dB on the transient |
| scanner V1→C1→V3 | 0.9815 | -14.3 dB | ±0.5 dB up to 1.6 kHz; highs -3.7/+2.9 dB |
| preamp (drive 0→60→127) | 0.9999 | -37.8 dB | ±0.0 dB |
| reverb (demo, mix 40) | 0.9983 | -24.6 dB | ±0.0 dB |
| Leslie (stop→slow→fast) | 0.9995 | -30.4 dB | ±0.0 dB |
| full chain (demo) | 0.9991 | -27.6 dB | ±0.0 dB |

The **web emulator** builds with Emscripten and runs in `app/build/emu/`
(`make serve-web` → http://localhost:8790/), with the same chain and the same
device UI. The device build also runs on this machine through **Colima +
Rosetta** (`colima start --vz-rosetta`). Generated package:
`app/build/fm1-wheel.fwsc` (611,354 B), app 315,520 B, `.data+.bss` 77,300 B
of 98,304, pool 180,844 B of 344,064, no soft-double. The only remaining step
is connecting the FM-1 and flashing/measuring; until then, the web emulator is
the interactive work environment.

---

## 1. Summary and recommendation

**Recommendation:** create a new firmware on top of **fm1-base** (which already
uses **felucca-bsp**) and port the **setBfree** DSP core to it. Do not fork all
of Felucca.

- **Platform base:** `charlesvestal/fm1-base` (MPL-2.0) — screen, keyboard,
  LEDs, I2S audio, USB-MIDI, TRS-MIDI, storage, installer, **host simulator**
  and **browser emulator**.
- **DSP base:** `pantherb/setBfree` (GPL-2.0-or-later) — 91-wheel tone
  generator, scanner vibrato/chorus, percussion, key click, key compression,
  preamp/overdrive, reverb and Leslie.
- **App licence:** **GPL-3.0-or-later**. setBfree is GPL-2.0-or-later; fm1-base
  and the BSP are MPL-2.0; the JieLi SDK is Apache-2.0. The clean combination
  is GPLv3.
- **Product:** a **tonewheel organ module** with a built-in 27-key keyboard.
  The "real" use is with an external 61-key/drawbar MIDI controller over USB or
  TRS. This works around the FM-1's biggest physical limitation.
- **Realistic schedule:** 9 to 14 weeks of focused work for a solid MVP; 4 to
  6 months at a part-time pace.

What I **do not** recommend:

- Trying to run setBfree as-is: its wavetables take **604 KB** in float and the
  Leslie takes **929 KB**. It does not fit in the FM-1's 336 KB pool.
- Rewriting everything in fixed point: the FM-1 has a **single-precision FPU**
  (`-mfprev1`). The gain does not justify the risk of losing the timbre.
- Forking Felucca just to inherit its UI: its architecture is per-voice/part
  (8 voices × 4 parts), and setBfree is global (91 shared wheels). The
  architectural fight costs more than writing an organ UI from scratch.
- Promising a complete 61-key clonewheel: the FM-1 has **27 keys** (F3–G5), 7
  encoders and 14 buttons. The honest path is "sound module + convenience
  keyboard".

---

## 2. What each repository is (and is not)

| Repository | What it is | Licence | Role in the project |
|---|---|---|---|
| `pantherb/setBfree` | Tonewheel organ DSP in C float, with a JACK app, LV2 plugin and OpenGL GUI | GPL-2.0-or-later | **Source of the algorithm and timbre.** Port only the DSP. |
| `hugelton/Felucca` | Complete FM-1 firmware (multi-engine, sequencer, web editor) | GPL-3.0-only | **UX/audio/latency reference.** It is not a kit; it is a product. |
| `hugelton/felucca-bsp` | FM-1 hardware layer (clock, IRQ, flash, USB, UART, loader, packaging) | MPL-2.0 | Already inside fm1-base. No need to touch it. |
| `charlesvestal/fm1-base` | Firmware base with `plat.h`, template, host simulator, WASM emulator, installer and site | MPL-2.0 | **Project base.** |

The "kit for building custom firmwares" you suspected existed is the
combination **felucca-bsp + fm1-base**. Felucca is the first large product on
top of them, not the kit.

### 2.1 Why the existing clonewheel (Felucca's WHEEL) does not sound like setBfree

Felucca's `WHEEL` engine is a lightweight approximation:

- 9 sine partials **per voice**, 8 voices per part;
- 16 fixed registrations + 4 macros;
- percussion, click, drive and a cheap "rotary" (AM + Doppler without delay
  lines);
- fixed point, designed to be inexpensive.

setBfree models the instrument:

- **91 free-running wheels**, each with its mechanical frequency (gear ratios),
  not one sine per note;
- **contact matrix** of 3 manuals × 61 keys × 9 buses, with **taper** and
  **crosstalk** from terminals, compartments and wiring;
- **key compression** (volume drops as more keys are pressed);
- **percussion** with a trigger bus muting the 1' drawbar;
- real **scanner vibrato/chorus** (modulated delay line);
- **Leslie** with horn/drum, Doppler, reflections, filters and acceleration;
- tube **preamp/overdrive**.

It is a different class of model. The computational cost is surprisingly low
because the 91 wheels are **shared** between the keys: there is no per-voice
polyphony.

---

## 3. Feasibility on the FM-1 hardware

### 3.1 The hardware

| Item | Value | Note |
|---|---|---|
| SoC | JieLi AC79, `pi32v2` CPU | 320 MHz in the SDK; 360 MHz in the boot loader |
| FPU | single precision (`-mfprev1`), no FMA | `-ffp-contract=off`; no runtime `double` |
| RAM | 96 KB (`.data`/`.bss`) + 336 KB (pool) | fm1-base map |
| Flash | ~581 KB app slot + data area | XIP; code runs from flash |
| Audio | I2S, 44.1 kHz, stereo, 24-bit | 128- or 256-frame half buffer |
| Latency | 128 frames = 2.9 ms per half | with `APP_BLOCK 128` |
| Screen | 240×240 LCD (ST7789) | 240×144 usable canvas in fm1-base |
| Keyboard | 27 keys, F3–G5 | with optional velocity/pressure |
| Controls | 7 encoders + 14 buttons + MASTER pot | physical labels: FX, SEL, ENV, LFO, EDIT, GLO, HOME, SAVE, ARP, SEQ, PLAY, REC, OCT-, OCT+ |
| MIDI | USB device (in/out) + TRS **in** | **no MIDI DIN out** |
| USB | MIDI + audio (in/out) + serial | great for recording and comparing |

### 3.2 What I already validated in this analysis (host spikes)

I compiled setBfree's `tonegen.c`, `vibrato.c`, `whirl.c`, `overdrive.c` and
`reverb.c` **without JACK/LV2**, in a minimal harness on macOS, and rendered
WAVs. Results:

| Measurement | Result |
|---|---|
| Tonegen, 1 note (888000000) | 18 active wheels |
| Tonegen, 3 notes | 54 active wheels |
| Tonegen, 9 notes | 91 wheels (saturates) |
| Tonegen, 91 wheels | ~740× real time on the Mac (host; not the FM-1) |
| setBfree wavetables @44.1 kHz | **604 KB in float** (91 wheels) |
| `keyContrib` (3 manuals, 154 keys) | **20,488 entries**, max 194/key, ~120 KB at 6 B/entry |
| Original `struct b_whirl` | **929 KB** (16,384-position tables) |
| `b_whirl` with 1,024 positions | **89 KB**; **-50.2 dB** difference vs. the original on a chord with Leslie fast |
| Reverb | ~79 KB of delay lines; ~867× real time on the Mac |
| Preamp/overdrive | a few KB; ~1,424× real time on the Mac |

Conclusions:

1. **The tonegen is feasible.** Even with 91 wheels, the per-sample cost is
   dominated by 3 MACs + table lookup. The FPU handles it; the real bottleneck
   is memory, not CPU.
2. **The 604 KB wavetables do not fit.** The solution is a **phase accumulator
   + shared sine table**. This is faithful to setBfree because, in the default
   configuration, **the wheels are pure sines** (`wheel_Harmonics[0] = 1.0`,
   the rest 0; the extras in `default.cfg` are commented out). The timbre comes
   from routing/drawbars/crosstalk/percussion/Leslie, not from per-wheel
   harmonics.
3. **The Leslie is the largest RAM consumer.** Reducing
   `WHIRL_DISPLC_SIZE` from 16,384 to 1,024 drops it from 929 KB to 89 KB with
   a -50 dB difference. It is the sweet spot for the FM-1 pool.
4. **The reverb costs 79 KB** and is optional. It fits, but it is the first
   candidate to cut if the pool runs short.
5. **The contact matrix can live in flash** (120 KB) or be regenerated in a
   compact form from 9 taper entries per key (~6 KB). For the MVP, flash is
   simpler.

### 3.3 Proposed memory budget

| Block | Pool | Flash |
|---|---:|---:|
| UI canvas (240×144×2 B, the template's pool) | 68 KB | — |
| Leslie (1,024 positions) | 89 KB | ~40 KB of code |
| Reverb | 77 KB | ~5 KB |
| Tonegen (phases, gains, AOT, buffers) | ~20 KB | ~60 KB |
| Preamp/overdrive | ~3 KB | ~10 KB |
| Audio/UI buffers | ~5 KB | — |
| Wheel/contact tables | — | ~120 KB (or ~6 KB compact) |
| fm1-base platform (outside the canvas) | ~1 KB | ~77 KB |
| UI/app proper | ~10 KB | ~40 KB |
| **Approximate total** | **~273 KB of 336 KB** | **~350 KB of 581 KB** |

The pool is tight, but it fits. No Bluetooth/WiFi in v1. If BLE is needed
(+24 KB pool, +45 KB RAM, +124 KB flash), cut the reverb first or compact the
matrix.

### 3.4 CPU budget (estimate for 320 MHz)

| Block | Estimated cost | % of 320 MHz |
|---|---:|---:|
| Tonegen, 91 wheels | 40–100 M cycles/s | 12–31% |
| Scanner vibrato | ~1 M | <1% |
| Preamp | 10–30 M | 3–9% |
| Reverb | ~1–2 M | <1% |
| Leslie | 5–15 M | 2–5% |
| **Total audio** | **~60–150 M** | **~19–47%** |
| UI/main loop | separate | ~10–20% |

Reasonable margin, but the range is wide. **The first thing to measure on the
device is the worst case** (9 notes, 888888888, Leslie fast, drive + reverb,
91 wheels). If it exceeds 60% average, cut crosstalk, reduce the Leslie to 512
positions, or offload to the second core.

---

## 4. Proposed architecture

```
                 USB-MIDI / TRS-MIDI          27 keys + encoders/buttons
                        │                              │
                        ▼                              ▼
                 event queue (lock-free)         UI (app_frame, 60 Hz)
                        │                              │
                        ▼                              ▼
┌─────────────────────────────────────────────────────────────────────┐
│ app_render (audio ISR, 128-frame blocks)                            │
│                                                                     │
│  tonegen (91 wheels, 27 buses, taper/crosstalk)                     │
│      │  swell + vibrato + percussion (mono)                         │
│      ▼                                                              │
│  scanner vibrato/chorus                                             │
│      ▼                                                              │
│  preamp / overdrive (mono)                                          │
│      ▼                                                              │
│  reverb (mono)                                                      │
│      ▼                                                              │
│  Leslie (mono → stereo: horn L/R + drum L/R)                        │
│      ▼                                                              │
│  limiter/soft-clip + DC blocker + MASTER                            │
│      ▼                                                              │
│  int32 24-bit → I2S / USB audio                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### 4.1 Tonegen: the heart

Port from `setBfree/src/tonegen.c`:

- `aot[91]` (active oscillator table) and the contribution lists;
- `oscKeyOn` / `oscKeyOff` with the message queue;
- `keyContrib` (taper + crosstalk) — generated at build time or compacted;
- `drawBarGain[27]`, `drawBarLevel[27][9]`;
- percussion (normal/soft, fast/slow, 2nd/3rd harmonic, trigger bus);
- key click (4 envelope models, attack/release);
- key compression;
- scanner vibrato/chorus (V1/V2/V3/C1/C2/C3).

**Main change:** replace the wavetables with **91 phase accumulators** and one
shared sine table (e.g. 2,048 points, float or int16 with linear
interpolation).

- Frequencies: calculate setBfree's gear ratios at build time and generate
  Q32 phase increments.
- Per-wheel gain: setBfree's EQ `attenuation`, generated at build time.
- Initial phase: 0 for all (as in setBfree).
- Dither: setBfree adds 1 LSB of noise to the wavetables; irrelevant. Add a
  tiny LFSR if desired.

This preserves the **beating** between wheels (the part that makes the organ
"alive") and eliminates 604 KB.

### 4.2 Contact matrix

Two options:

1. **MVP — full table in flash:** generate `key_contrib[]` with setBfree's own
   code in a host tool and emit a `.h` with `const` arrays (120 KB). Read only
   on key-on; no audio cost.
2. **Optimisation — compact:** store the 9 direct contributions per key
   (terminal + level) and `terminalMix[91]`; rebuild the wiring crosstalk on
   key-on with the same formula (`defaultWiringCrosstalk * level / |Δbus|`).
   It drops to ~6 KB and remains faithful.

Start with 1; migrate to 2 if flash becomes tight (BLE, presets, samples).

### 4.3 Leslie

Port `b_whirl/whirl.c` with:

- `WHIRL_DISPLC_SIZE = 1024` (measured: -50.2 dB vs. original, 89 KB);
- 2,048-sample delay buffers (32 KB);
- IIR filters and reflections as they are;
- `setRevSelect` (STOP/SLOW/FAST) with acceleration ramps;
- single-microphone preset (horn/drum) to save memory.

If the pool allows, try 2,048 positions (153 KB) and compare by ear. 1,024 is
already enough for the FM-1.

### 4.4 Preamp, reverb and overdrive

- `b_overdrive/overdrive.c` + `filterTools.c`: port as-is (4× oversampling,
  33-tap filters). Cheap.
- `b_reverb/reverb.c`: port as-is; 79 KB. Default 20–30% mix.
- No cabinet IR (convolution) in v1.

### 4.5 Integration with fm1-base

- Copy `fm1-base/template/` to `app/`.
- `APP_BLOCK 128` (2.9 ms) for acceptable keyboard latency.
- `app_render`: drain MIDI, read keys, run the engine in 128-frame blocks,
  apply MASTER, convert to int32 24-bit.
- `app_frame`: UI at ~60 Hz; LEDs; persistence.
- `app_tick` (1 kHz) optional for the UI/sequencer clock; not for audio.
- State sharing: event queue for drawbars/percussion/Leslie; atomic 32-bit
  snapshots. No locks in the ISR.
- `fm1.toml`: `o2 = ["dsp/*.c"]` for DSP at -O2;
  `vendored = ["third_party/setbfree/"]` for the setBfree code; `gen` for the
  tables.

### 4.6 Control mapping (proposal)

**Encoders**

| Encoder | HOME | DRAWBARS | PERC/VIB | FX |
|---|---|---|---|---|
| SELECT | page | cursor (bar 1–9) | — | — |
| ALGO | model (B3/C3/A100) | manual (U/L/P) | — | — |
| PRESET | registration | — | — | — |
| K1 | 16' / 5⅓' | bar value | percussion on/off | drive |
| K2 | 8' / 4' | — | decay (fast/slow) | reverb mix |
| K3 | 2⅔' / 2' | — | harmonic (2nd/3rd) | Leslie speed |
| K4 | 1⅗' / 1⅓' / 1' | — | vibrato/chorus (V1..C3) | — |

**Buttons**

| Button | Function |
|---|---|
| FX | drive on/off |
| SEL | cursor / next page |
| ENV | percussion on/off |
| LFO | vibrato/chorus on/off |
| EDIT | advanced menu (tuning, MIDI, etc.) |
| GLO | LED brightness (platform) |
| HOME | main screen |
| SAVE | save preset |
| ARP | manual U/L/P (the physical label differs; show it on screen) |
| SEQ | split on/off (label differs) |
| PLAY | Leslie fast/slow (green/red LED) |
| REC | panic / MIDI learn |
| OCT- / OCT+ | keyboard transpose |

The physical labels do not match an organ. The screen should show the real
function; a printed sticker/overlay helps. For serious use, an external
controller solves it.

### 4.7 MIDI

Adopt setBfree's map for compatibility with existing controllers:

- Channels 1/2/3 = upper/lower/pedal.
- CC 1/11 = swell.
- CC 70–78 = drawbars 16', 5⅓', 8', 4', 2⅔', 2', 1⅗', 1⅓', 1' (inverted:
  0 = maximum).
- CC 64 = Leslie speed toggle.
- CC 80/81/82 = percussion enable/decay/harmonic.
- CC 83 = vibrato knob; CC 92 = vibrato routing.
- CC 93 = overdrive character.
- Program change = registration/preset.
- USB-MIDI out to a DAW/editor; TRS is input only.

This turns the FM-1 into a module compatible with drawbar controllers (Ocean
Beach, Crumar, Viscount, etc.) and DAWs.

### 4.8 Presets and storage

- 16–32 classic factory registrations (888000000, 888800000, 888000008,
  800000888, etc.).
- User presets in flash (use fm1-base's store; `APP_STORE_MAP`).
- Save: drawbars U/L/P, percussion, vibrato, drive, reverb, Leslie, tuning,
  MIDI settings.
- Backup/restore over USB: fm1-base already has `backup = true` (optional).

---

## 5. Licensing

| Component | Licence | Obligation |
|---|---|---|
| setBfree (ported DSP) | GPL-2.0-or-later | keep headers; publish the source of modifications |
| fm1-base | MPL-2.0 | modifications to its files stay MPL-2.0; the app may use another licence |
| felucca-bsp | MPL-2.0 | same |
| JieLi SDK (boot/cfg) | Apache-2.0 | compatible with GPLv3; keep notices |
| **Combined app/firmware** | **GPL-3.0-or-later** | publish the source and binary; no extra restrictions |

Cautions:

- **Do not copy Felucca code** (GPL-3.0-only) into the app if you want to keep
  the "or later" option; use it only as a reference. If you copy it, the
  combination becomes GPL-3.0-only (still compatible with setBfree "or later",
  but less flexible).
- **Do not use setBfree's GUI textures/fonts**; they have their own licences.
- **Trademarks:** avoid "Hammond", "B3" and "Leslie" in the product name and
  marketing. Describe it as a "tonewheel organ" and a "rotary speaker". In
  code, "leslie" as a technical term is acceptable, but the product must not
  be named that.
- If you ever enable JieLi Bluetooth/WiFi (Apache-2.0 binary libraries), review
  the combination with GPLv3 and the RAM cost. Do not enable it in v1.

---

## 6. Roadmap

Estimates for one focused person. Add 30–50% if working in parallel with other
tasks.

### Phase 0 — Base and reference (3–5 days)

- [x] Repository created; `fm1-base` (with the BSP) and `setBfree` pinned as
  submodules.
- [x] `app/` copied from the template; `APP_BLOCK 128`; `id_prefix FM-1_12`.
- [x] Host build and `tests/basic.txt` passing.
- [x] `tools/setbfree-host/refrender`: full setBfree chain (tonegen → preamp →
  reverb → whirl) to float WAV.
- [x] `tools/setbfree-host/dump_tables`: `wheel_inc`, `wheel_att`,
  `key_contrib`, `key_comp`, envelopes.
- [ ] Install Docker Desktop, the JieLi toolchain and the AC79 SDK.
- [x] Build the device firmware through Colima/Rosetta (`fm1-wheel.fwsc`
  package).
- [ ] Flash the FM-1 and test boot, screen, keys, audio, USB-MIDI, TRS, update
  and recovery.

**Acceptance:** host OK; device test pending (Docker/toolchain/SDK).

### Phase 1 — Tonegen port (2–3 weeks) — in progress

- [x] Dry tone generator: wheels as phase accumulators + sine table.
- [x] Drawbars, keyContrib, taper/crosstalk, key compression, click,
  percussion.
- [x] Host app playing: 27 keys, USB/TRS MIDI, drawbars, percussion, settings.
- [x] A/B against setBfree (`make compare`): chord 0.99995 / -40 dB;
  percussion 0.9996 / -31 dB.
- [x] Port `vibrato.c` (scanner V1–V3/C1–C3): 0.9815 / -14.3 dB.
- [x] Port preamp/overdrive: 0.9999 / -37.8 dB; reverb: 0.9983 / -24.6 dB.
- [x] Port the Leslie (whirl, 1,024 positions): 0.9995 / -30.4 dB.
- [ ] Measure CPU on the device and adjust the limiter.

**Acceptance:** the full chain matches setBfree on the host; device test
pending.

### Phase 2 — Device: audio and performance (1–2 weeks)

- Integrate into `app_render`; 24-bit output; limiter/soft-clip; DC blocker.
- Port preamp, reverb and Leslie (1,024 positions).
- Measure CPU on the FM-1: idle, 1, 3, 9, 27 notes; 888888888; Leslie fast;
  drive+reverb.
- Measure key latency and stability (30 min without dropout).
- Adjust levels and headroom.

**Acceptance:** plays on the FM-1; average CPU < 50% and peak < 80% in the
worst case; latency < 10 ms.

### Phase 3 — UI, MIDI and persistence (2–3 weeks)

- Screens: HOME (drawbars), DRAWBARS, PERC/VIB, FX, SETTINGS, PRESETS.
- Encoder/button/LED mapping.
- Complete MIDI (USB + TRS, channels, CCs, program change).
- Save/load presets and settings; factory reset.
- UI tests in the host simulator and screenshots.

**Acceptance:** can play and change registration without a computer; settings
survive a reboot; external MIDI controls everything.

### Phase 4 — Sound parity and polish (2–3 weeks)

- Full crosstalk (or the compact version), key compression, trigger bus.
- Calibrate Leslie (acceleration, brake, mic), drive, reverb.
- A/B against setBfree and reference recordings.
- Classic presets; user presets.
- Golden tests (render hash), CPU budget in CI.

**Acceptance:** subjective A/B approved; no timbre regressions; stable CPU.

### Phase 5 — Product (1–2 weeks)

- Site + installer (fm1-base), manual, recovery guide.
- Release 0.1.1; changelog; licences and credits.
- Optional: web editor over USB SysEx, BLE MIDI, second core.

**Acceptance:** installer works in Chrome/Edge; manual covers controls, MIDI
and recovery.

---

## 7. Risks and mitigation

| Risk | Prob. | Impact | Mitigation |
|---|---|---|---|
| CPU/FPU worse than estimated | medium | high | measure in Phase 2; reduce crosstalk; Leslie 1,024; offload to CPU1 (experimental) |
| Pool overflow | medium | high | cut reverb; compact keyContrib; smaller canvas; no BLE |
| Leslie at 1,024 sounds different | low | medium | A/B test; 2,048 if the pool allows; adjust filters |
| High key latency | medium | medium | `APP_BLOCK 128`; read keys in the ISR; avoid long queues |
| fm1-base API changes | high | low | pin the commit; minimal patches; vendor it |
| Subtle bug in the setBfree port | high | medium | port block by block; golden tests; A/B |
| Licence | low | high | GPL-3.0-or-later; headers; published source; no trademarks |
| Bricking during flashing | low | high | test recovery first; keep the official firmware; `fm1_rescue`/transporter |
| 27 keys limit the product | certain | medium | position it as a module; external MIDI; octave shift |

---

## 8. MVP vs. later

**MVP (v1):**

- Upper manual on the 27-key keyboard, with OCT-/OCT+.
- 9 editable drawbars + 16 registrations.
- Percussion (on/off, 2nd/3rd, fast/slow, normal/soft).
- Vibrato/chorus V1–V3/C1–C3.
- Drive, reverb, Leslie slow/fast.
- USB + TRS MIDI (upper/lower/pedal per channel; setBfree CC map).
- User presets and settings in flash.
- Host simulator + golden tests.
- No BLE, no WiFi, no second core, no web editor.

**Later:**

- Editable lower/pedal in the UI and keyboard split.
- Web editor (Web MIDI/SysEx).
- BLE MIDI (if the pool/flash allow).
- Second core for the Leslie or tonegen.
- Organ models (50 Hz, 82 wheels, 91FB00/12).
- Per-wheel harmonics ("dirty" wheels).
- Cabinet IR.
- Sequencer/arpeggiator (Felucca already has one; not the focus).

---

## 9. Concrete next steps (first 2 weeks)

1. **Device environment** (1–2 days) — pending
   ```sh
   # from the repository root; needs Docker Desktop running
   fm1-base/tools/get_toolchain.sh
   git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 \
       https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK
   make device
   python3 fm1-base/tools/fm1_install.py app/build/fm1-wheel.fwsc
   ```
   Requires Docker Desktop (the toolchain is Linux x86-64; on Apple Silicon it
   runs through Rosetta). Test boot, screen, keys, audio, USB-MIDI, TRS, update
   and recovery.

2. **Repository** (1 day) — done
   - `app/` copied from the template; `APP_BLOCK 128`; `id_prefix FM-1_12`.
   - `fm1.toml`: name, USB names, GPL-3.0-or-later licence.
   - `fm1-base` (with BSP) and `setBfree` submodules pinned.
   - `make build-host` and `make test-host` passing.

3. **Reference** (2–3 days) — done
   - `tools/setbfree-host/refrender` (tonegen → preamp → reverb → whirl).
   - `tools/setbfree-host/dump_tables` (`wheel_inc`, `wheel_att`,
     `key_contrib`, `key_comp`, envelopes).
   - `make ref` and `make tables`.

4. **Tonegen on the host** (1 week) — done (dry)
   - `app/dsp/organ.c`: phase wheels, drawbars, key matrix, percussion, click.
   - `app/app.c`: keyboard, MIDI, drawbars, presets, settings; plays in the
     simulator.
   - A/B: `make compare`.
   - Scanner, preamp, reverb and Leslie done; next: the device.

5. **First sound on the device** (2–3 days)
   - Integrate into `app_render`; 24-bit output; limiter. (already integrated)
   - Play a chord on the FM-1 and record through USB audio.
   - Measure CPU/latency.

## 10. Decisions I need from you

Defaults I adopted (tell me if you want another path):

1. **Base:** fm1-base + setBfree, new app. (Alternative: fork Felucca.)
2. **Positioning:** organ module with a 27-key keyboard; external MIDI for the
   full experience.
3. **Licence:** GPL-3.0-or-later.
4. **v1 scope:** upper manual, drawbars, percussion, vibrato/chorus, drive,
   reverb, Leslie; no BLE/WiFi/second core/web editor.
5. **Leslie:** 1,024 positions (89 KB, -50 dB) — if you have a demanding ear,
   we test 2,048.
6. **Product name:** **fm1-wheel**, a reference to tonewheels. The package
   `id_prefix` remains `FM-1_12` to avoid invalidating installs.

Phase 0 is done on the host. The next step is the device environment (Docker
+ toolchain + SDK), flashing the template to the FM-1, and then starting Phase
1 (tonegen port).

---

## Appendix A — Commands and reference files

- Toolchain/SDK: `fm1-base/tools/get_toolchain.sh`, `fm1-base/README.md`
  (Build).
- Build: `fm1-base/tools/build.py APP_DIR [--host|--web|--release X.Y]`.
- Install: `python3 fm1-base/tools/fm1_install.py build/NAME.fwsc`.
- Simulator: `APP_DIR/build/host/NAME_host APP_DIR/tests/basic.txt
  APP_DIR/build/host`.
- Web emulator: `fm1-base/tools/build.py APP_DIR --web`, serve
  `APP_DIR/build/emu/`.
- Memory map: `fm1-base/device/app.ld`.
- FM-1 HAL: `felucca-bsp/chip/`, `felucca-bsp/boards/fm1/`.
- CPU/FPU: `felucca-bsp/docs/CPU.md`, `fm1-base/include/fastmath.h`.
- setBfree: `src/tonegen.c`, `src/vibrato.c`, `b_whirl/whirl.c`,
  `b_overdrive/overdrive.c`, `b_reverb/reverb.c`, `cfg/default.cfg`.
- Felucca (UX reference): `firmware/src/audio.c`, `firmware/src/main.c`,
  `firmware/hal/fm1_input.h`.

## Appendix B — Why the sine table is faithful

In setBfree, `initOscillators` generates one wavetable per wheel from
`wheel_Harmonics[]`. The default is:

```c
t->wheel_Harmonics[0] = 1.0;   /* fundamental */
/* all others = 0 */
```

And the extra harmonics in `cfg/default.cfg` are commented out. That is:
**each wheel is a pure sine**, with a mechanical frequency (gear ratio) and
attenuation (EQ). The "organ sound" comes from:

- 9 drawbars per manual, each connecting to a different wheel (16', 5⅓', 8',
  4', 2⅔', 2', 1⅗', 1⅓', 1');
- contact taper and crosstalk;
- key click and key compression;
- percussion;
- scanner vibrato/chorus;
- preamp, reverb and Leslie.

Therefore, replacing the wavetables with `sin(2π · phase)` at the same
frequency/attenuation preserves the default model. If we ever want wheels with
harmonics, we only need to generate a sine table per profile (12 harmonics) and
share it between wheels of the same profile — without going back to 604 KB.

## Appendix C — Reproducing the spikes

The harness used in this analysis:

- Compiles `tonegen.c` + `vibrato.c` without JACK/LV2, defines `SampleRateD`,
  and calls `allocTonegen`, `initToneGenerator`, `oscKeyOn`,
  `oscGenerateFragment`.
- Compiles `whirl.c` + `eqcomp.c` and calls `allocWhirl`, `initWhirl`,
  `whirlProc3`.
- Compiles `overdrive.c` + `filterTools.c` and `reverb.c` to measure
  memory/CPU.
- For the Leslie test, changes `WHIRL_DISPLC_SIZE` from `1 << 14` to `1 << 10`
  and compares the WAVs (-50.2 dB difference).

These files live in `tools/setbfree-host/` in the repository and are the basis
of the golden tests.
