/* SPDX-License-Identifier: GPL-3.0-or-later
 * fm1-wheel reverb, after setBfree's b_reverb/reverb.c
 * Copyright (C) 2003-2004 Fredrik Kilander, 2008-2018 Robin Gareus,
 * 2012 Will Panther. GPL-2.0-or-later.
 *
 * The delay lengths are setBfree's at 44.1 kHz (end[i] * 2, odd, + 2). The
 * pool lives in the FM-1's .pool RAM (79 KB), not in the 96 KB .bss. */
#include "wb_libc.h"

#include "reverb.h"

#define RV_FS 44100
#define RV_N 19663

#if defined(__APPLE__)
#define RV_POOL __attribute__((section("__DATA,.pool")))
#elif defined(__EMSCRIPTEN__)
#define RV_POOL
#else
#define RV_POOL __attribute__((section(".pool")))
#endif
static float rv_pool[RV_N] RV_POOL;
static const unsigned short rv_size[RV_NZ] = {6001, 4665, 3789, 2197, 2105, 677, 229};

void reverb_init(reverb_t *r)
{
    int i, off = 0;
    memset(r, 0, sizeof *r);
    memset(rv_pool, 0, sizeof rv_pool);
    for (i = 0; i < RV_NZ; i++) {
        r->delays[i] = rv_pool + off;
        r->idx0[i] = r->idxp[i] = r->delays[i];
        r->endp[i] = r->delays[i] + rv_size[i] - 1;
        r->gain[i] = 0.70710678f;   /* 1/sqrt(2) */
        off += rv_size[i];
    }
    r->inputGain = 0.1f;
    r->fbk = -0.015f;
    r->wet = 0.1f;
    r->dry = 0.9f;
}

void reverb_set_mix(reverb_t *r, float g)
{
    float u = r->wet + r->dry;
    if (u <= 0.0f)
        u = 1.0f;
    r->wet = g * u;
    r->dry = u - (g * u);
}

void reverb_run(reverb_t *r, const float *in, float *out, int n)
{
    float **idxp = r->idxp;
    float **endp = r->endp;
    float **idx0 = r->idx0;
    const float *gain = r->gain;
    const float inputGain = r->inputGain;
    const float fbk = r->fbk;
    const float wet = r->wet;
    const float dry = r->dry;
    float y_1 = r->y_1, yy1 = r->yy1;
    int i;

    for (i = 0; i < n; i++) {
        int j;
        float y;
        const float xo = in[i];
        const float x = y_1 + inputGain * xo;
        float xa = 0.0f;
        for (j = 0; j < 4; j++) {
            y = *idxp[j];
            *idxp[j] = x + gain[j] * y;
            if (endp[j] <= ++idxp[j])
                idxp[j] = idx0[j];
            xa += y;
        }
        for (; j < RV_NZ; j++) {
            y = *idxp[j];
            *idxp[j] = gain[j] * (xa + y);
            if (endp[j] <= ++idxp[j])
                idxp[j] = idx0[j];
            xa = y - xa;
        }
        y = 0.5f * (xa + yy1);
        yy1 = y;
        y_1 = fbk * xa;
        out[i] = wet * y + dry * xo;
    }
    r->y_1 = y_1 + 1e-14f;
    r->yy1 = yy1 + 1e-14f;
}
