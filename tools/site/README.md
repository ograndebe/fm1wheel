# Static site

`make site` builds the release package and the browser emulator, asks
fm1-base's `make_site.py` for the installer/emulator/firmware/words assets,
and writes the landing and manual pages into `docs/`.

| File | What |
|---|---|
| `build_site.py` | the builder; `--no-build` reuses `app/build` and `build/site` |
| `landing.html` | the Portuguese front page template (`{{VERSION}}`, `{{DATE}}`, `{{SITE}}`, `{{REPO}}`) |
| `style.css` | the landing and manual styles |
| `favicon.svg` | the site icon |

The generated `docs/` is committed and served by GitHub Pages from
`main` / `docs` at https://ograndebe.github.io/fm1wheel/.

The normal build needs Colima/Docker, the JieLi toolchain and the AC79 SDK,
because it builds a release package. `--no-build` only rewrites the HTML/CSS
from the existing assets.
