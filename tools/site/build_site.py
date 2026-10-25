#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build the fm1-wheel static site in docs/ (GitHub Pages).

  tools/site/build_site.py [--version 0.1] [--no-build]

--no-build reuses app/build and build/site (for a quick manual/landing rebuild).
The normal run builds the browser emulator and a release package first, then
uses fm1-base's make_site.py for the installer, emulator, firmware and words,
and writes our own landing and manual pages.
"""
import argparse
import datetime
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / "tools" / "site"
DOCS = ROOT / "docs"
BUILD = ROOT / "build" / "site"
SITE_URL = "https://ograndebe.github.io/fm1wheel/"
REPO = "https://github.com/ograndebe/fm1wheel"


def run(cmd, env=None):
    print("+", " ".join(str(c) for c in cmd))
    subprocess.run([str(c) for c in cmd], check=True, env=env)


def copy_tree(src, dst):
    if dst.exists():
        shutil.rmtree(dst)
    shutil.copytree(src, dst)


def manual(src, dst, lang, root_rel, old_lang_link, new_lang_link, version, date):
    text = (ROOT / src).read_text(encoding="utf-8")
    text = text.replace("docs/img/emulator.png", "img/emulator.png")
    text = text.replace("](LICENSING.md)", f"]({REPO}/blob/main/LICENSING.md)")
    text = text.replace("](LICENSES/)", f"]({REPO}/tree/main/LICENSES)")
    text = text.replace(old_lang_link, new_lang_link)
    tmp = BUILD / ("manual-" + lang.replace("-", "_") + ".md")
    tmp.write_text(text, encoding="utf-8")

    dst.parent.mkdir(parents=True, exist_ok=True)
    run([sys.executable, ROOT / "fm1-base" / "tools" / "manual_html.py", tmp, dst,
         "--name", "fm1-wheel", "--repo", REPO, "--tagline",
         "a tonewheel organ for the M-VAVE FM-1, after setBfree"])

    page = dst.read_text(encoding="utf-8")
    page = page.replace('<html lang="en">', f'<html lang="{lang}">')
    page = page.replace("<body>", '<body class="manual">')
    head = (
        f'<link rel="icon" href="{root_rel}favicon.svg" type="image/svg+xml">\n'
        f'<link rel="stylesheet" href="{root_rel}style.css">\n'
    )
    page = page.replace("</head>", head + "</head>")
    nav = (
        '<nav class="topnav"><div class="wrap navwrap"><a class="brand" href="' + root_rel + '">fm1-wheel</a>'
        '<nav class="navlinks" aria-label="Site">'
        f'<a href="{root_rel}manual/">Manual PT</a>'
        f'<a href="{root_rel}manual/en/">English</a>'
        f'<a href="{root_rel}emu/">Experimentar</a>'
        f'<a href="{root_rel}install/">Instalar</a>'
        f'<a class="github" href="{REPO}">GitHub ↗</a>'
        '</nav></div></nav>\n'
    )
    page = page.replace('<body class="manual">', '<body class="manual">\n' + nav)
    footer = (
        '<footer class="sitefoot"><div class="wrap">'
        f'<span>fm1-wheel {version} · {date}</span>'
        f'<span class="right"><a href="{REPO}">GitHub</a> · <a href="{root_rel}">Site</a> · '
        'GPL-3.0-or-later · OpenCode + DeepSeek V4.1 Flash</span>'
        '</div></footer>\n'
    )
    page = page.replace("</body>", footer + "</body>")
    dst.write_text(page, encoding="utf-8")

    img = dst.parent / "img"
    img.mkdir(parents=True, exist_ok=True)
    shutil.copy2(DOCS / "img" / "emulator.png", img / "emulator.png")
    print("wrote", dst)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--version", default="0.1")
    ap.add_argument("--no-build", action="store_true")
    args = ap.parse_args()
    version = args.version
    date = datetime.date.today().isoformat()
    pkg = ROOT / "app" / "build" / f"fm1-wheel-{version}.fwsc"

    if not args.no_build:
        env = os.environ.copy()
        env.setdefault("AC79_SDK", str(Path.home() / "fw-AC79_AIoT_SDK"))
        env.setdefault("JIELI_TOOLCHAIN", str(Path.home() / ".jieli" / "toolchain"))
        run([sys.executable, ROOT / "fm1-base" / "tools" / "build.py", "app", "--web"], env)
        run([sys.executable, ROOT / "fm1-base" / "tools" / "build.py", "app", "--release", version], env)
        if not pkg.exists():
            sys.exit(f"missing release package: {pkg}")
        if BUILD.exists():
            shutil.rmtree(BUILD)
        run([sys.executable, ROOT / "fm1-base" / "tools" / "make_site.py",
             "app", pkg, version, BUILD])

    if not BUILD.exists():
        sys.exit(f"missing {BUILD}; run without --no-build")
    DOCS.mkdir(parents=True, exist_ok=True)

    if (DOCS / "manual").exists():
        shutil.rmtree(DOCS / "manual")
    for name in ("install", "emu", "firmware", "words"):
        src = BUILD / name
        if src.exists():
            copy_tree(src, DOCS / name)
    shutil.copy2(BUILD / ".nojekyll", DOCS / ".nojekyll")
    shutil.copy2(TOOLS / "style.css", DOCS / "style.css")
    shutil.copy2(TOOLS / "favicon.svg", DOCS / "favicon.svg")
    (DOCS / "backup").mkdir(parents=True, exist_ok=True)
    shutil.copy2(TOOLS / "backup.html", DOCS / "backup" / "index.html")

    landing = (TOOLS / "landing.html").read_text(encoding="utf-8")
    landing = (landing.replace("{{VERSION}}", version).replace("{{DATE}}", date)
               .replace("{{SITE}}", SITE_URL).replace("{{REPO}}", REPO))
    (DOCS / "index.html").write_text(landing, encoding="utf-8")
    print("wrote", DOCS / "index.html")

    manual("MANUAL.md", DOCS / "manual" / "index.html", "pt-BR", "../",
           "[English](MANUAL.en.md)", "[English](../en/)", version, date)
    manual("MANUAL.en.md", DOCS / "manual" / "en" / "index.html", "en", "../../",
           "[Português](MANUAL.md)", "[Português](../)", version, date)
    print("site:", SITE_URL)


if __name__ == "__main__":
    main()
