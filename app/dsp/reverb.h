/* SPDX-License-Identifier: GPL-3.0-or-later
 * fm1-wheel reverb, after setBfree's b_reverb/reverb.c (GPL-2.0-or-later).
 * Static delay pool (in .pool) and no config/MIDI registration. */
#pragma once

#define RV_NZ 7

typedef struct {
    float *delays[RV_NZ];
    float *idx0[RV_NZ];
    float *idxp[RV_NZ];
    float *endp[RV_NZ];
    float gain[RV_NZ];
    float yy1, y_1;
    float inputGain, fbk, wet, dry;
} reverb_t;

void reverb_init(reverb_t *r);
void reverb_set_mix(reverb_t *r, float g);   /* 0 dry .. 1 wet */
void reverb_run(reverb_t *r, const float *in, float *out, int n);
