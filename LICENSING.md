# Licences

fm1-wheel combines components under several free-software licences. The
combined firmware is distributed under **GPL-3.0-or-later**.

| Component | Files | Licence |
|---|---|---|
| fm1-wheel app, DSP port, tools | `app/`, `tools/` | GPL-3.0-or-later |
| setBfree | `third_party/setBfree/`, `tools/setbfree-host/` | GPL-2.0-or-later |
| fm1-base | `fm1-base/` | MPL-2.0 (file by file) |
| felucca-bsp | `fm1-base/bsp/` | MPL-2.0 (file by file) |
| SMK-37 board layer (`BOARD_SMK37`, `boards/smk37`, `plat_smk_*`) | `fm1-base/bsp/boards/smk37/`, guarded parts of `fm1-base/` | MPL-2.0, Copyright (C) 2026 M. Geisler (genkigenki/makkuro) |
| JieLi AC79 SDK files fetched by the build | not in this tree | Apache-2.0 |

## Why GPL-3.0-or-later

- setBfree is GPL-2.0-**or-later**; its "or later" clause allows the combined
  work to be GPLv3.
- MPL-2.0 (fm1-base, felucca-bsp) is compatible with GPLv3.
- The JieLi SDK is Apache-2.0, which is compatible with GPLv3 (not with
  GPLv2-only).

## Obligations

- Keep the copyright and licence headers in every file.
- Publish the source of the firmware you distribute, including the changes to
  setBfree's files and to fm1-base's files.
- fm1-base and felucca-bsp files stay MPL-2.0; changes to them are published
  under MPL-2.0. The app's own files may be GPL-3.0-or-later.
- The setBfree GUI textures, fonts and documentation are **not** used here.
  Do not import them without checking their own licences.
- The SMK-37 board layer was used from the MPL-2.0 fm1-base work by
  M. Geisler. The `fm1-to-smk37` guide is **CC BY-NC 4.0** and is referenced,
  not copied; no GPL-3.0-only Felucca SMK branch code was imported into this
  GPL-3.0-or-later app.
- Do not use "Hammond", "B3" or "Leslie" in the product name or in a way that
  suggests endorsement. "Tonewheel organ" and "rotary speaker" are neutral.

## Full texts

- `LICENSE` — GPL-3.0 (the app's licence)
- `LICENSES/GPL-2.0.txt` — setBfree
- `LICENSES/MPL-2.0.txt` — fm1-base and felucca-bsp
- `LICENSES/Apache-2.0.txt` — JieLi SDK files
- `third_party/setBfree/COPYING` — setBfree's own copy
