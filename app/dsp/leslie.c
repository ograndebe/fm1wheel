/* SPDX-License-Identifier: GPL-3.0-or-later
 * WheelBox Leslie, after setBfree's b_whirl/whirl.c
 * Copyright (C) 2003-2004 Fredrik Kilander, 2008-2018 Robin Gareus,
 * 2010 Ken Restivo, 2012 Will Panther. GPL-2.0-or-later.
 *
 * The static tables come from the original init at 44.1 kHz and 1,024
 * displacement positions (whirl_tables.h). The runtime is the same horn/drum
 * Doppler, reflections, filters and mic mix; the speed state machine is float
 * only (no double, exp/fmod at runtime except the prefilters' exp at init). */
#include "wb_libc.h"

#include "fastmath.h"
#include "leslie.h"
#include "whirl_tables.h"

#define N WB_WHIRL_N
#define NMASK (WB_WHIRL_N - 1)
#define BUF 2048
#define BMASK (BUF - 1)
#define AGMASK 7

typedef struct {
    float adx0[8], adx1[8], adx2[8];
    int adi0, adi1, adi2;
    float hl[BUF], hr[BUF], dl[BUF], dr[BUF];
    float horn_angle, drum_angle;
    float horn_incr, drum_incr, horn_target, drum_target;
    signed char horn_acdc, drum_acdc;
    int outpos;
    float z[4];
    float hafw[8], hbfw[8], drfl[8], drfr[8];
    float lha, lhd, lda, ldd;
} leslie_t;

#if defined(__APPLE__)
#define LES_POOL __attribute__((section("__DATA,.pool")))
#elif defined(__EMSCRIPTEN__)
#define LES_POOL
#else
#define LES_POOL __attribute__((section(".pool")))
#endif
static leslie_t les LES_POOL;

static inline float fa(float x) { return x < 0 ? -x : x; }

static void copy_iir(float *d, const float *s)
{
    int i;
    for (i = 0; i < 8; i++)
        d[i] = s[i];
}

static void fix_nan(float *a)
{
    int i;
    for (i = 0; i < 8; i++)
        if (a[i] != a[i])
            a[i] = 0.0f;
}

void leslie_init(void)
{
    memset(&les, 0, sizeof les);
    copy_iir(les.hafw, wb_hafw);
    copy_iir(les.hbfw, wb_hbfw);
    copy_iir(les.drfl, wb_drfl);
    copy_iir(les.drfr, wb_drfr);
    les.lha = fm_expf(-1.0f / ((44100.0f / 128.0f) * WB_HORN_ACC));
    les.lhd = fm_expf(-1.0f / ((44100.0f / 128.0f) * WB_HORN_DEC));
    les.lda = fm_expf(-1.0f / ((44100.0f / 128.0f) * WB_DRUM_ACC));
    les.ldd = fm_expf(-1.0f / ((44100.0f / 128.0f) * WB_DRUM_DEC));
    les.horn_target = les.drum_target = 0.0f;
}

void leslie_set_speed(int mode)
{
    float hs = WB_HORN_RPM_SLOW / (44100.0f * 60.0f);
    float hf = WB_HORN_RPM_FAST / (44100.0f * 60.0f);
    float ds = WB_DRUM_RPM_SLOW / (44100.0f * 60.0f);
    float df = WB_DRUM_RPM_FAST / (44100.0f * 60.0f);
    switch (mode) {
    case LESLIE_FAST: les.horn_target = hf; les.drum_target = df; break;
    case LESLIE_SLOW: les.horn_target = hs; les.drum_target = ds; break;
    default: les.horn_target = 0.0f; les.drum_target = 0.0f; break;
    }
    les.horn_acdc = les.horn_target > les.horn_incr ? 1 : les.horn_target < les.horn_incr ? -1 : 0;
    les.drum_acdc = les.drum_target > les.drum_incr ? 1 : les.drum_target < les.drum_incr ? -1 : 0;
}

#define EQ(W, X, Y)                                                          \
    {                                                                        \
        float _t = (X) - (W)[1] * (W)[6] - (W)[2] * (W)[7];                  \
        (Y) = _t * (W)[3] + (W)[4] * (W)[6] + (W)[5] * (W)[7];              \
        (W)[7] = (W)[6];                                                     \
        (W)[6] = _t;                                                         \
    }

#define FILT(I)                                                              \
    {                                                                        \
        float _t = x;                                                        \
        x = 0.4f * x + 0.4f * les.z[I];                                     \
        les.z[I] = _t;                                                       \
    }

#define ADDH(DX, DI, XS)                                                     \
    {                                                                        \
        (DI) = ((DI) + AGMASK) & AGMASK;                                     \
        (DX)[(DI)] = (XS);                                                   \
    }

#define HN(P, HB, DSP, BW, DX, DI, ANG)                                      \
    {                                                                        \
        float _h1 = (ANG) * (float)N + (float)wb_horn_phase[P];              \
        float _hd = _h1 - (float)(int)_h1;                                   \
        unsigned _hl = (unsigned)_h1 & NMASK;                                \
        unsigned _hh = (_hl + 1) & NMASK;                                    \
        float _intp = (DSP)[_hl] * (1.0f - _hd) + (DSP)[_hh] * _hd;          \
        unsigned _k = (unsigned)(_h1 + 0.5f) & NMASK;                        \
        float _t = wb_horn_spacing[P] + _intp + (float)outpos;               \
        int _r = (int)_t;                                                    \
        float _xa = (BW)[_k][0] * x + (BW)[_k][1] * (DX)[(DI)] +             \
                    (BW)[_k][2] * (DX)[((DI) + 1) & AGMASK] +                \
                    (BW)[_k][3] * (DX)[((DI) + 2) & AGMASK] +                \
                    (BW)[_k][4] * (DX)[((DI) + 3) & AGMASK];                 \
        float _q = _xa * (_t - (float)_r);                                   \
        unsigned _n = (unsigned)_r & BMASK;                                  \
        (HB)[_n] += _xa - _q;                                                \
        _n = (_n + 1) & BMASK;                                               \
        (HB)[_n] += _q;                                                      \
    }

#define DR(P, DB, DSP)                                                       \
    {                                                                        \
        float _d1 = drum_angle * (float)N + (float)wb_drum_phase[P];         \
        float _dd = _d1 - (float)(int)_d1;                                   \
        unsigned _dl = (unsigned)_d1 & NMASK;                                \
        unsigned _dh = (_dl + 1) & NMASK;                                    \
        float _intp = (DSP)[_dl] * (1.0f - _dd) + (DSP)[_dh] * _dd;          \
        float _t = wb_drum_spacing[P] + _intp + (float)outpos;               \
        int _r = (int)_t;                                                    \
        float _q = x * (_t - (float)_r);                                     \
        unsigned _n = (unsigned)_r & BMASK;                                  \
        (DB)[_n] += x - _q;                                                  \
        _n = (_n + 1) & BMASK;                                               \
        (DB)[_n] += _q;                                                      \
    }

void leslie_run(const float *in, float *outL, float *outR, int n)
{
    float horn_angle = les.horn_angle, drum_angle = les.drum_angle;
    int outpos = les.outpos;
    int i;

    if (les.horn_acdc) {
        float l = les.horn_acdc > 0 ? les.lha : les.lhd;
        les.horn_incr += (1.0f - l) * (les.horn_target - les.horn_incr);
        if (fa(les.horn_target - les.horn_incr) < 0.05f / (60.0f * 44100.0f)) {
            les.horn_acdc = 0;
            les.horn_incr = les.horn_target;
        }
    }
    if (les.drum_acdc) {
        float l = les.drum_acdc > 0 ? les.lda : les.ldd;
        les.drum_incr += (1.0f - l) * (les.drum_target - les.drum_incr);
        if (fa(les.drum_target - les.drum_incr) < 0.05f / (60.0f * 44100.0f)) {
            les.drum_acdc = 0;
            les.drum_incr = les.drum_target;
        }
    }

    for (i = 0; i < n; i++) {
        float x = in[i] + 1e-14f;
        float xx = x;
        float leak;
        float yl, yr, hL, hR;

        EQ(les.hafw, x, x);
        EQ(les.hbfw, x, x);
        leak = x * (float)WB_LEAKAGE;

        HN(0, les.hl, wb_hn_fwd, wb_bbw, les.adx0, les.adi0, horn_angle);
        HN(1, les.hr, wb_hn_bwd, wb_bfw, les.adx0, les.adi0, horn_angle + 1.0f);
        ADDH(les.adx0, les.adi0, x);
        FILT(0);

        HN(2, les.hl, wb_hn_bwd, wb_bfw, les.adx1, les.adi1, horn_angle);
        HN(3, les.hr, wb_hn_fwd, wb_bbw, les.adx1, les.adi1, horn_angle + 1.0f);
        ADDH(les.adx1, les.adi1, x);
        FILT(1);

        HN(4, les.hl, wb_hn_fwd, wb_bbw, les.adx2, les.adi2, horn_angle);
        HN(5, les.hr, wb_hn_bwd, wb_bfw, les.adx2, les.adi2, horn_angle + 1.0f);
        ADDH(les.adx2, les.adi2, x);

        x = xx;
        DR(0, les.dl, wb_dr_fwd);
        DR(1, les.dr, wb_dr_bwd);
        FILT(2);
        DR(2, les.dl, wb_dr_bwd);
        DR(3, les.dr, wb_dr_fwd);
        FILT(3);
        DR(4, les.dl, wb_dr_fwd);
        DR(5, les.dr, wb_dr_bwd);

        EQ(les.drfl, les.dl[outpos], yl);
        EQ(les.drfr, les.dr[outpos], yr);
        hL = (float)WB_HORN_LEVEL * les.hl[outpos] + leak;
        hR = (float)WB_HORN_LEVEL * les.hr[outpos] + leak;
        outL[i] = hL * (float)WB_MIC_HLL + hR * (float)WB_MIC_HLR +
                  yl * (float)WB_MIC_DLL + yr * (float)WB_MIC_DLR;
        outR[i] = hL * (float)WB_MIC_HRL + hR * (float)WB_MIC_HRR +
                  yl * (float)WB_MIC_DRL + yr * (float)WB_MIC_DRR;
        les.hl[outpos] = les.hr[outpos] = les.dl[outpos] = les.dr[outpos] = 0.0f;
        outpos = (outpos + 1) & BMASK;
        horn_angle += les.horn_incr;
        horn_angle -= (float)(int)horn_angle;
        drum_angle += les.drum_incr;
        drum_angle -= (float)(int)drum_angle;
    }

    les.horn_angle = horn_angle;
    les.drum_angle = drum_angle;
    les.outpos = outpos;
    fix_nan(les.hafw);
    fix_nan(les.hbfw);
    fix_nan(les.drfl);
    fix_nan(les.drfr);
    for (i = 0; i < 4; i++)
        if (les.z[i] != les.z[i])
            les.z[i] = 0.0f;
}
