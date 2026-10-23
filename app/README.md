# app

The WheelBox firmware app. Phase 0 still uses fm1-base's template app as a
placeholder: it builds, runs in the host simulator and on the FM-1, and proves
the platform path. Phase 1 replaces `app.c` and adds `dsp/` (the setBfree port).

- `app.h`: the app's half of the fm1-base contract (`APP_NAME`, `APP_BLOCK 128`).
- `app.c`: the placeholder (keys, MIDI, screen, settings).
- `fm1.toml`: build and site settings.
- `dsp/`: the organ DSP (Phase 1; see `dsp/README.md`).
- `tests/basic.txt`: the host simulator smoke test.
- `docs/MANUAL.md`, `site/landing.md`: rewrite with the real UI in Phase 3.

Build from the repository root:

```sh
make build-host
make test-host
```

The platform code and the host simulator are in `../fm1-base/`. See the
top-level `README.md` and `PLANO.md`.
