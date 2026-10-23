/* SPDX-License-Identifier: GPL-3.0-or-later
 * Scanner vibrato/chorus, after setBfree (GPL-2.0-or-later).
 * The fixed-point delay write/read is kept as in setBfree; the tables use
 * float math from fastmath so the firmware needs no libm or double. */
#include <string.h>

#include "fastmath.h"
#include "vibrato.h"

#define INCTBL_MASK 0x07FFFFFFu
#define BUF_MASK 0x000003FFu
#define POSN_MASK 0x03FFFFFFu

static void set_tables(uint32_t *tab, float amp)
{
    int i;
    for (i = 0; i < VIB_TABLE_SIZE; i++) {
        float m = fm_sinf(6.28318530718f * (float)i / (float)VIB_TABLE_SIZE);
        tab[i] = (uint32_t)((1.0f + amp + m * amp) * 65536.0f);
    }
}

void vib_init(vib_t *v, float fs)
{
    memset(v, 0, sizeof *v);
    v->out_pos = (int)(BUF_MASK / 2u);
    v->stator_inc = (uint32_t)((7.25f * (float)VIB_TABLE_SIZE / fs) * 65536.0f);
    set_tables(v->offset1, 3.0f);
    set_tables(v->offset2, 6.0f);
    set_tables(v->offset3, 9.0f);
    v->offset = v->offset3;
    v->mixed = 0;
}

void vib_set_mode(vib_t *v, int mode)
{
    switch (mode) {
    case VIB_V1: v->offset = v->offset1; v->mixed = 0; break;
    case VIB_C1: v->offset = v->offset1; v->mixed = 1; break;
    case VIB_V2: v->offset = v->offset2; v->mixed = 0; break;
    case VIB_C2: v->offset = v->offset2; v->mixed = 1; break;
    case VIB_V3: v->offset = v->offset3; v->mixed = 0; break;
    case VIB_C3: v->offset = v->offset3; v->mixed = 1; break;
    default: break;
    }
}

void vib_proc(vib_t *v, const float *in, float *out, int n)
{
    const float fnorm = 1.0f / 65536.0f;
    const float mixnorm = 0.7071067811865475f;
    int i;
    for (i = 0; i < n; i++) {
        float x = in[i];
        uint32_t j = (((uint32_t)v->out_pos << 16) + v->offset[v->stator >> 16]) & POSN_MASK;
        int h = (int)(j >> 16);
        int k = (h + 1) & (int)BUF_MASK;
        float f = fnorm * (float)(j & 0xFFFFu);
        float g = f * x;
        v->buf[h] += x - g;
        v->buf[k] += g;
        if (v->mixed)
            out[i] = (x + v->buf[v->out_pos]) * mixnorm;
        else
            out[i] = v->buf[v->out_pos];
        v->buf[v->out_pos] = 0.0f;
        v->out_pos = (v->out_pos + 1) & (int)BUF_MASK;
        v->stator = (v->stator + v->stator_inc) & INCTBL_MASK;
    }
}
