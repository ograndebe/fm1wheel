/* SPDX-License-Identifier: GPL-3.0-or-later
 * fm1-wheel organ engine: a port of setBfree's tone generator (GPL-2.0-or-later)
 * to phase-accumulator sine wheels. One instance, called from the audio block.
 *
 * Keys are setBfree's key numbers:
 *   upper 0..60   (MIDI 36..96)
 *   lower 64..124 (MIDI 36..96)
 *   pedal 128..159 (MIDI 24..55)
 * The app maps its 27 keys and MIDI channels to these.
 */
#pragma once

#include <stdint.h>
#include "vibrato.h"

#define ORGAN_BLOCK 128
#define ORGAN_MANUAL_UPPER 0
#define ORGAN_MANUAL_LOWER 1
#define ORGAN_MANUAL_PEDAL 2
#define ORGAN_BARS 9
#define ORGAN_BUSES 27

void organ_init(void);
void organ_reset(void);                       /* silence now (panic / update) */

void organ_key_on(int key);
void organ_key_off(int key);
void organ_all_notes_off(void);

void organ_set_drawbar(int manual, int bar, int value);   /* value 0..8 */
void organ_set_swell(int value);                          /* 0..127 */

void organ_set_percussion(int on);
void organ_set_perc_harmonic(int second);                 /* 1 = 2nd, 0 = 3rd */
void organ_set_perc_decay(int fast);
void organ_set_perc_volume(int soft);

void organ_set_vibrato_mode(int mode);       /* VIB_OFF, VIB_V1 .. VIB_C3 */
void organ_set_vibrato_routing(int upper, int lower);

void organ_set_drive(int value);              /* 0 = clean, 1..127 overdrive */
void organ_set_drive_character(int value);    /* 0..127 */
void organ_set_reverb(int value);             /* 0 dry .. 127 wet */
void organ_set_leslie(int mode);              /* LESLIE_STOP/SLOW/FAST */

/* Render n frames (n <= ORGAN_BLOCK) of the stereo organ mix. */
void organ_render(float *outL, float *outR, int n);
