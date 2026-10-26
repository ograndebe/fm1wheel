#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build the fm1-wheel static site in docs/ (GitHub Pages).

  tools/site/build_site.py [--version 0.2.0] [--no-build]

--no-build reuses app/build and build/site (for a quick manual/landing rebuild).
The normal run builds the browser emulator and a release package first, then
uses fm1-base's make_site.py for the installer, emulator, firmware and words,
and writes the bilingual landing and manual pages: /pt/ and /en/, with / as a
browser-language detector and English as the fallback.
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


def render(text, version, date):
    return (text.replace("{{VERSION}}", version).replace("{{DATE}}", date)
            .replace("{{SITE}}", SITE_URL).replace("{{REPO}}", REPO))


def manual(src, dst, lang, root_rel, home_rel, old_lang_link, new_lang_link, version, date):
    text = (ROOT / src).read_text(encoding="utf-8")
    text = text.replace("docs/img/emulator.png", "img/emulator.png")
    text = text.replace("](LICENSING.md)", f"]({REPO}/blob/main/LICENSING.md)")
    text = text.replace("](LICENSES/)", f"]({REPO}/tree/main/LICENSES)")
    text = text.replace("](SMK37.md)", f"]({REPO}/blob/main/SMK37.md)")
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
    page = page.replace('<a class="home" href="../">', f'<a class="home" href="{home_rel}">')
    if lang == "pt-BR":
        page = page.replace(">Skip to content</a>", ">Ir para o conteúdo</a>")
        page = page.replace('<a href="en/">English</a>', '<a href="en/" data-lang="en">English</a>')
        canonical = f"{SITE_URL}manual/"
        nav = (
            '<nav class="topnav"><div class="wrap navwrap"><a class="brand" href="' + home_rel + '">fm1-wheel</a>'
            '<nav class="navlinks" aria-label="Site">'
            '<a href="./" data-lang="pt">Manual PT</a>'
            '<a href="en/" data-lang="en" hreflang="en" lang="en">English</a>'
            '<a href="../emu/">Experimentar</a>'
            '<a href="../install/">Instalar</a>'
            f'<a class="github" href="{REPO}">GitHub ↗</a>'
            '</nav></div></nav>\n'
        )
    else:
        page = page.replace('<a href="../">Português</a>', '<a href="../" data-lang="pt">Português</a>')
        canonical = f"{SITE_URL}manual/en/"
        nav = (
            '<nav class="topnav"><div class="wrap navwrap"><a class="brand" href="' + home_rel + '">fm1-wheel</a>'
            '<nav class="navlinks" aria-label="Site">'
            '<a href="../" data-lang="pt" hreflang="pt-BR" lang="pt-BR">Manual PT</a>'
            '<a href="./" data-lang="en">English</a>'
            '<a href="../../emu/">Try it</a>'
            '<a href="../../install/">Install</a>'
            f'<a class="github" href="{REPO}">GitHub ↗</a>'
            '</nav></div></nav>\n'
        )
    head = (
        f'<link rel="canonical" href="{canonical}">\n'
        f'<link rel="alternate" hreflang="pt-BR" href="{SITE_URL}manual/">\n'
        f'<link rel="alternate" hreflang="en" href="{SITE_URL}manual/en/">\n'
        f'<link rel="alternate" hreflang="x-default" href="{SITE_URL}manual/en/">\n'
        f'<link rel="icon" href="{root_rel}favicon.svg" type="image/svg+xml">\n'
        f'<link rel="stylesheet" href="{root_rel}style.css">\n'
        f'<script src="{root_rel}lang.js" defer></script>\n'
    )
    page = page.replace("</head>", head + "</head>")
    page = page.replace('<body class="manual">', '<body class="manual">\n' + nav)
    footer = (
        '<footer class="sitefoot"><div class="wrap">'
        f'<span>fm1-wheel {version} · {date}</span>'
        f'<span class="right"><a href="{REPO}">GitHub</a> · <a href="{home_rel}">Site</a> · '
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
    ap.add_argument("--version", default="0.2.0")
    ap.add_argument("--no-build", action="store_true")
    args = ap.parse_args()
    version = args.version
    date = datetime.date.today().isoformat()
    pkg = ROOT / "app" / "build" / f"fm1-wheel-{version}.fwsc"
    smk_pkg = ROOT / "app" / "build" / "smk37" / f"fm1-wheel-smk37-{version}.fwsc"
    smk_name = f"fm1-wheel-smk37-{version}.fwsc"

    if not args.no_build:
        env = os.environ.copy()
        env.setdefault("AC79_SDK", str(Path.home() / "fw-AC79_AIoT_SDK"))
        env.setdefault("JIELI_TOOLCHAIN", str(Path.home() / ".jieli" / "toolchain"))
        run([sys.executable, ROOT / "fm1-base" / "tools" / "build.py", "app", "--web"], env)
        run([sys.executable, ROOT / "fm1-base" / "tools" / "build.py", "app", "--release", version], env)
        run([sys.executable, ROOT / "fm1-base" / "tools" / "build.py", "app", "--board", "smk37",
             "--model", "pro", "--release", version], env)
        if not pkg.exists():
            sys.exit(f"missing release package: {pkg}")
        if not smk_pkg.exists():
            sys.exit(f"missing SMK release package: {smk_pkg}")
        if BUILD.exists():
            shutil.rmtree(BUILD)
        run([sys.executable, ROOT / "fm1-base" / "tools" / "make_site.py",
             "app", pkg, version, BUILD])
        shutil.copy2(smk_pkg, BUILD / "firmware" / smk_name)

    if not BUILD.exists():
        sys.exit(f"missing {BUILD}; run without --no-build")
    if not (BUILD / "firmware" / smk_name).exists():
        if not smk_pkg.exists():
            sys.exit(f"missing SMK release package: {smk_pkg}")
        shutil.copy2(smk_pkg, BUILD / "firmware" / smk_name)
    DOCS.mkdir(parents=True, exist_ok=True)

    if (DOCS / "manual").exists():
        shutil.rmtree(DOCS / "manual")
    for name in ("install", "emu", "words"):
        src = BUILD / name
        if src.exists():
            copy_tree(src, DOCS / name)
    # Keep published firmware packages: the build only adds the current one.
    fw_src = BUILD / "firmware"
    if fw_src.exists():
        fw_dst = DOCS / "firmware"
        fw_dst.mkdir(parents=True, exist_ok=True)
        for f in fw_src.iterdir():
            if f.is_file():
                shutil.copy2(f, fw_dst / f.name)
    shutil.copy2(BUILD / ".nojekyll", DOCS / ".nojekyll")
    shutil.copy2(TOOLS / "style.css", DOCS / "style.css")
    shutil.copy2(TOOLS / "lang.js", DOCS / "lang.js")
    shutil.copy2(TOOLS / "favicon.svg", DOCS / "favicon.svg")
    (DOCS / "backup").mkdir(parents=True, exist_ok=True)
    shutil.copy2(TOOLS / "backup.html", DOCS / "backup" / "index.html")

    (DOCS / "pt").mkdir(parents=True, exist_ok=True)
    (DOCS / "en").mkdir(parents=True, exist_ok=True)
    (DOCS / "pt" / "index.html").write_text(
        render((TOOLS / "landing.pt.html").read_text(encoding="utf-8"), version, date), encoding="utf-8")
    (DOCS / "en" / "index.html").write_text(
        render((TOOLS / "landing.en.html").read_text(encoding="utf-8"), version, date), encoding="utf-8")
    (DOCS / "pt" / "smk37").mkdir(parents=True, exist_ok=True)
    (DOCS / "en" / "smk37").mkdir(parents=True, exist_ok=True)
    (DOCS / "pt" / "smk37" / "index.html").write_text(
        render((TOOLS / "smk37.pt.html").read_text(encoding="utf-8"), version, date), encoding="utf-8")
    (DOCS / "en" / "smk37" / "index.html").write_text(
        render((TOOLS / "smk37.en.html").read_text(encoding="utf-8"), version, date), encoding="utf-8")
    (DOCS / "index.html").write_text(
        render((TOOLS / "root.html").read_text(encoding="utf-8"), version, date), encoding="utf-8")
    print("wrote", DOCS / "index.html", DOCS / "pt/index.html", DOCS / "en/index.html",
          DOCS / "pt/smk37/index.html", DOCS / "en/smk37/index.html")

    manual("MANUAL.pt.md", DOCS / "manual" / "index.html", "pt-BR", "../", "../pt/",
           "[English](MANUAL.md)", "[English](en/)", version, date)
    manual("MANUAL.md", DOCS / "manual" / "en" / "index.html", "en", "../../", "../../en/",
           "[Português](MANUAL.pt.md)", "[Português](../)", version, date)
    print("site:", SITE_URL)


if __name__ == "__main__":
    main()
