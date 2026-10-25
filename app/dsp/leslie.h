/* SPDX-License-Identifier: GPL-3.0-or-later
 * fm1-wheel Leslie, after setBfree's b_whirl (GPL-2.0-or-later).
 * Tables: whirl_tables.h, generated at 1,024 displacement positions.
 * Static state in .pool; float-only runtime. */
#pragma once

enum { LESLIE_STOP, LESLIE_SLOW, LESLIE_FAST };

void leslie_init(void);
void leslie_set_speed(int mode);
void leslie_run(const float *in, float *outL, float *outR, int n);
