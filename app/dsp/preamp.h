/* SPDX-License-Identifier: GPL-3.0-or-later
 * fm1-wheel preamp/overdrive, after setBfree's b_overdrive/overdrive.c
 * (GPL-2.0-or-later). Static instance, no config/MIDI registration, no libm. */
#pragma once

void preamp_init(void);
void preamp_set_drive(float u);        /* 0 = clean; > 0 overdrive bias (0..1) */
void preamp_set_character(float u);    /* 0..1, fat/feedback character */
void preamp_run(float *buf, int n);    /* in place */
