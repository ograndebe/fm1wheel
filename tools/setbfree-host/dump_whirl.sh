#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Build the Leslie tables from setBfree's whirl at 1,024 displacement positions.
# Host-only; writes app/dsp/whirl_tables.h.
set -e
cd "$(dirname "$0")"
ROOT=../..
SB=$ROOT/third_party/setBfree
B=build/whirl1024

rm -rf "$B"
mkdir -p "$B/b_whirl" "$B/src"
cp "$SB/b_whirl/whirl.c" "$SB/b_whirl/eqcomp.c" "$SB/b_whirl/eqcomp.h" "$B/b_whirl/"
cp "$SB/src/cfgParser.h" "$SB/src/midi.h" "$SB/src/midi_types.h" "$B/src/"
sed 's/#define WHIRL_DISPLC_SIZE ((unsigned int)(1 << 14))/#define WHIRL_DISPLC_SIZE ((unsigned int)(1 << 10))/' \
    "$SB/b_whirl/whirl.h" > "$B/b_whirl/whirl.h"

case "$(uname -s)" in
Darwin) LD=-Wl,-dead_strip ;;
*)      LD=-Wl,--gc-sections ;;
esac
cc -O2 -ffp-contract=off -ffunction-sections "$LD" -I. -I"$B/b_whirl" -I"$B/src" \
    -o "$B/dump_whirl" dump_whirl.c host.c "$B/b_whirl/whirl.c" "$B/b_whirl/eqcomp.c" -lm
"$B/dump_whirl" "$ROOT/app/dsp/whirl_tables.h"
