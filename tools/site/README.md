# Static site

`make site` builds the release package and the browser emulator, asks
fm1-base's `make_site.py` for the installer/emulator/firmware/words assets,
and writes the landing and manual pages into `docs/`.

| File | What |
|---|---|
| `build_site.py` | the builder; `--no-build` reuses `app/build` and `build/site` |
| `root.html` | the root language detector (`/`); chooses `/pt/` or `/en/` |
| `landing.pt.html` | the Portuguese front page template |
| `landing.en.html` | the English front page template |
| `lang.js` | remembers a language chosen from a page's language link |
| `style.css` | the landing and manual styles |
| `favicon.svg` | the site icon |

The templates use `{{VERSION}}`, `{{DATE}}`, `{{SITE}}` and `{{REPO}}`.
The root page sends Portuguese browsers to `/pt/` and every other browser to
`/en/` (English is the fallback); a saved choice and `?lang=pt|en` override it.
The manuals stay at `/manual/` (Português) and `/manual/en/` (English).

The generated `docs/` is committed and served by GitHub Pages from
`main` / `docs` at https://ograndebe.github.io/fm1wheel/.

The normal build needs Colima/Docker, the JieLi toolchain and the AC79 SDK,
because it builds a release package. `--no-build` only rewrites the HTML/CSS
from the existing assets.
