#!/bin/sh
# Release build for the signed GitHub workflow (.github/workflows/release.yml):
#   ./build.sh 0.2.1
# The shared workflow checks the repository out without its submodules, so this
# script initialises them first. $JIELI_TOOLCHAIN and $AC79_SDK are set by it.
set -eu
version="$1"
AC79_SDK="${AC79_SDK:-$HOME/fw-AC79_AIoT_SDK}"
JIELI_TOOLCHAIN="${JIELI_TOOLCHAIN:-$HOME/.jieli/toolchain}"
export AC79_SDK JIELI_TOOLCHAIN
git submodule update --init --recursive
python3 fm1-base/tools/build.py app --release "$version"
python3 fm1-base/tools/build.py app --board smk37 --model pro --release "$version"
