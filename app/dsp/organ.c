/* SPDX-License-Identifier: GPL-3.0-or-later
 * WheelBox organ engine.
 *
 * The tone generator is a port of setBfree's src/tonegen.c (GPL-2.0-or-later),
 * Copyright (C) 2003-2004 Fredrik Kilander, 2008-2018 Robin Gareus,
 * 2012 Will Panther. The key-contact matrix, wheel frequencies and envelopes
 * come from tools/setbfree-host/dump_tables (organ_tables.h, generated).
 *
 * Deviation from setBfree: each wheel is a phase accumulator over one shared
 * sine table instead of a per-wheel wavetable. setBfree's default wheels are
 * pure sines (wheel_Harmonics[0] = 1.0), so this preserves the default model
 * while dropping ~604 KB of wavetables. The scanner vibrato and the FX chain
 * are added in the next slices.
 */
#include <stdint.h>
#include <string.h>

#include "fastmath.h"
#include "organ.h"
#include "organ_tables.h"
#include "vibrato.h"
#include "preamp.h"
#include "reverb.h"
#include "leslie.h"

#define NSINE 2048
#define SINE_MASK (NSINE - 1)
#define LEVEL_SCALE (1.0f / 16777216.0f)   /* wb_contrib[].level is Q24 */
#define COMP_SCALE (1.0f / 32768.0f)
#define SWELL_TRIM 0.07f                   /* setBfree outputLevelTrim */

typedef struct {
    float bus[ORGAN_BUSES];
    float g0[3], gd[3], target[3];
    const int16_t *env;
    uint32_t phase;
    uint16_t refs;
    uint8_t active, removing, releasing, added, modified, env_pos;
} wheel_t;

static struct {
    wheel_t w[WB_WHEELS];
    uint8_t active[WB_WHEELS];
    int nactive;
    uint8_t key_down[WB_KEYS];
    int keys_down, upper_keys;
    float drawbar[ORGAN_BUSES];
    float swell;
    float current_gain;
    float key_comp;
    int perc_on, perc_send, perc_soft, perc_fast;
    float perc_env, perc_decay, perc_reset, perc_drawbar_gain, pz;
    int perc_trig_bus;
    float perc_trig_restore;
    uint8_t drawbar_changed;
    int vib_mode, vib_upper, vib_lower;
    vib_t vib;
    reverb_t rev;
    int leslie_mode;
    float sine[NSINE];
} o;

static inline float sine_at(uint32_t phase)
{
    uint32_t i = phase >> 21;
    float f = (float)(phase & 0x1FFFFF) * (1.0f / 2097152.0f);
    float a = o.sine[i], b = o.sine[(i + 1) & SINE_MASK];
    return a + (b - a) * f;
}

/* setBfree's *default* decay constants: initToneGenerator selects fast/normal
 * before computePercResets() ever runs, so the placeholders are what you hear.
 * (The configured 1 s / 4 s times only apply after a decay-time change.) */
static float perc_decay_for(int fast)
{
    return fast ? 0.9995f : 0.9999f;
}

static void wheel_sums(wheel_t *w)
{
    float upper = 0, lower = 0, pedal = 0;
    int b;
    for (b = 0; b < 9; b++)
        upper += w->bus[b] * o.drawbar[b];
    for (b = 9; b < 18; b++)
        lower += w->bus[b] * o.drawbar[b];
    for (b = 18; b < 27; b++)
        pedal += w->bus[b] * o.drawbar[b];
    w->target[0] = pedal;
    w->target[1] = 0.0f;
    if (o.vib_mode != VIB_OFF && o.vib_upper)
        w->target[1] += upper;
    else
        w->target[0] += upper;
    if (o.vib_mode != VIB_OFF && o.vib_lower)
        w->target[1] += lower;
    else
        w->target[0] += lower;
    w->target[2] = o.perc_on ? w->bus[o.perc_send] : 0.0f;
}

static float gain_now(const wheel_t *w, int k)
{
    if (w->env_pos >= ORGAN_BLOCK || !w->env)
        return w->g0[k];
    return w->g0[k] + w->gd[k] * ((float)w->env[w->env_pos] * COMP_SCALE);
}

static void attack(wheel_t *w, int idx)
{
    wheel_sums(w);
    w->releasing = 0;
    w->g0[0] = w->g0[1] = w->g0[2] = 0.0f;
    w->gd[0] = w->target[0];
    w->gd[1] = w->target[1];
    w->gd[2] = w->target[2];
    w->env = wb_attack_env[idx & 7];
    w->env_pos = 0;
}

static void release(wheel_t *w, int idx)
{
    int k;
    for (k = 0; k < 3; k++) {
        w->g0[k] = gain_now(w, k);
        w->gd[k] = -w->g0[k];
        w->target[k] = 0.0f;
    }
    w->env = wb_release_env[idx & 7];
    w->env_pos = 0;
}

static void jump(wheel_t *w)
{
    wheel_sums(w);
    w->g0[0] = w->target[0];
    w->g0[1] = w->target[1];
    w->g0[2] = w->target[2];
    w->gd[0] = w->gd[1] = w->gd[2] = 0.0f;
    w->env = 0;
    w->env_pos = ORGAN_BLOCK;
}

static void wheel_activate(wheel_t *w)
{
    if (!w->active) {
        w->active = 1;
        o.active[o.nactive++] = (uint8_t)(w - o.w);
    }
}

void organ_key_on(int key)
{
    uint32_t i;
    if (key < 0 || key >= WB_KEYS || o.key_down[key])
        return;
    o.key_down[key] = 1;
    o.keys_down++;
    if (key < 64)
        o.upper_keys++;
    for (i = wb_contrib_index[key]; i < wb_contrib_index[key + 1]; i++) {
        wheel_t *w = &o.w[wb_contrib[i].wheel - 1];
        float lv = (float)wb_contrib[i].level * LEVEL_SCALE;
        if (w->refs == 0) {
            wheel_activate(w);
            w->added = 1;
            w->removing = 0;
            w->releasing = 0;
            w->modified = 0;
            w->env = 0;
            w->env_pos = ORGAN_BLOCK;
        } else if (w->removing) {
            w->removing = 0;
            w->releasing = 0;
            w->added = 1;
            w->modified = 0;
            w->env = 0;
            w->env_pos = ORGAN_BLOCK;
        } else if (!w->added) {
            w->modified = 1;
        }
        w->bus[wb_contrib[i].bus] += lv;
        w->refs++;
    }
}

void organ_key_off(int key)
{
    uint32_t i;
    if (key < 0 || key >= WB_KEYS || !o.key_down[key])
        return;
    o.key_down[key] = 0;
    o.keys_down--;
    if (key < 64)
        o.upper_keys--;
    for (i = wb_contrib_index[key]; i < wb_contrib_index[key + 1]; i++) {
        wheel_t *w = &o.w[wb_contrib[i].wheel - 1];
        float lv = (float)wb_contrib[i].level * LEVEL_SCALE;
        w->bus[wb_contrib[i].bus] -= lv;
        w->refs--;
        if (w->refs == 0) {
            w->removing = 1;
            w->releasing = 0;
            w->added = 0;
            w->modified = 0;
        } else {
            w->modified = 1;
        }
    }
}

void organ_all_notes_off(void)
{
    int i;
    for (i = 0; i < o.nactive; i++) {
        wheel_t *w = &o.w[o.active[i]];
        memset(w->bus, 0, sizeof w->bus);
        w->refs = 0;
        w->removing = 1;
        w->releasing = 0;
        w->added = 0;
        w->modified = 0;
    }
    memset(o.key_down, 0, sizeof o.key_down);
    o.keys_down = 0;
    o.upper_keys = 0;
}

void organ_set_drawbar(int manual, int bar, int value)
{
    int bus;
    float g;
    if (manual < 0 || manual > 2 || bar < 0 || bar >= ORGAN_BARS)
        return;
    if (value < 0)
        value = 0;
    if (value > 8)
        value = 8;
    bus = manual * 9 + bar;
    g = (float)value / 8.0f;
    if (bus == o.perc_trig_bus && o.perc_on) {
        o.perc_trig_restore = g;
        return;
    }
    o.drawbar[bus] = g;
    o.drawbar_changed = 1;
}

void organ_set_swell(int value)
{
    if (value < 0)
        value = 0;
    if (value > 127)
        value = 127;
    o.swell = SWELL_TRIM * (float)value / 127.0f;
}

void organ_set_percussion(int on)
{
    on = on != 0;
    if (on == o.perc_on)
        return;
    if (on) {
        o.perc_trig_restore = o.drawbar[o.perc_trig_bus];
        o.drawbar[o.perc_trig_bus] = 0.0f;
    } else {
        o.drawbar[o.perc_trig_bus] = o.perc_trig_restore;
    }
    o.perc_on = on;
    o.drawbar_changed = 1;
}

void organ_set_perc_harmonic(int second)
{
    o.perc_send = second ? 3 : 4;   /* 2nd = 4', 3rd = 2 2/3' */
    o.drawbar_changed = 1;
}

void organ_set_perc_decay(int fast)
{
    o.perc_fast = fast != 0;
    o.perc_decay = perc_decay_for(o.perc_fast);
}

void organ_set_vibrato_mode(int mode)
{
    if (mode < VIB_OFF)
        mode = VIB_OFF;
    if (mode > VIB_C3)
        mode = VIB_C3;
    o.vib_mode = mode;
    vib_set_mode(&o.vib, mode);
    o.drawbar_changed = 1;
}

void organ_set_vibrato_routing(int upper, int lower)
{
    o.vib_upper = upper != 0;
    o.vib_lower = lower != 0;
    o.drawbar_changed = 1;
}

void organ_set_drive(int value)
{
    if (value < 0)
        value = 0;
    if (value > 127)
        value = 127;
    preamp_set_drive((float)value / 127.0f);
}

void organ_set_drive_character(int value)
{
    if (value < 0)
        value = 0;
    if (value > 127)
        value = 127;
    preamp_set_character((float)value / 127.0f);
}

void organ_set_reverb(int value)
{
    if (value < 0)
        value = 0;
    if (value > 127)
        value = 127;
    reverb_set_mix(&o.rev, (float)value / 127.0f);
}

void organ_set_leslie(int mode)
{
    if (mode < LESLIE_STOP)
        mode = LESLIE_STOP;
    if (mode > LESLIE_FAST)
        mode = LESLIE_FAST;
    o.leslie_mode = mode;
    leslie_set_speed(mode);
}

void organ_set_perc_volume(int soft)
{
    o.perc_soft = soft != 0;
    o.perc_reset = 11.0f * (o.perc_soft ? 0.5012f : 1.0f);
    o.perc_drawbar_gain = o.perc_soft ? 1.0f : 0.60512f;
    if (o.upper_keys == 0)
        o.perc_env = o.perc_reset;
}

static void render_block(float *mono, float *outL, float *outR, int n)
{
    float swell[ORGAN_BLOCK], scan[ORGAN_BLOCK], vibout[ORGAN_BLOCK], perc[ORGAN_BLOCK];
    int i, k, wr = 0;

    memset(swell, 0, sizeof(float) * (size_t)n);
    memset(scan, 0, sizeof(float) * (size_t)n);
    memset(vibout, 0, sizeof(float) * (size_t)n);
    memset(perc, 0, sizeof(float) * (size_t)n);

    if (o.drawbar_changed) {
        for (i = 0; i < o.nactive; i++) {
            wheel_t *w = &o.w[o.active[i]];
            if (!w->added && !w->removing)
                w->modified = 1;
        }
        o.drawbar_changed = 0;
    }

    for (i = 0; i < o.nactive; i++) {
        wheel_t *w = &o.w[o.active[i]];
        if (w->added) {
            attack(w, i);
            w->added = 0;
        } else if (w->removing && !w->releasing) {
            release(w, i);
            w->releasing = 1;
        } else if (w->modified) {
            jump(w);
            w->modified = 0;
        }
    }

    for (i = 0; i < o.nactive; i++) {
        wheel_t *w = &o.w[o.active[i]];
        uint32_t phase = w->phase, inc = wb_wheel_inc[o.active[i] + 1];
        for (k = 0; k < n; k++) {
            float x = sine_at(phase);
            float gs, gv, gp;
            if (w->env_pos < ORGAN_BLOCK && w->env) {
                float e = (float)w->env[w->env_pos] * COMP_SCALE;
                gs = w->g0[0] + w->gd[0] * e;
                gv = w->g0[1] + w->gd[1] * e;
                gp = w->g0[2] + w->gd[2] * e;
                w->env_pos++;
                if (w->env_pos >= ORGAN_BLOCK) {
                    w->g0[0] = w->target[0];
                    w->g0[1] = w->target[1];
                    w->g0[2] = w->target[2];
                    w->gd[0] = w->gd[1] = w->gd[2] = 0.0f;
                }
            } else {
                gs = w->g0[0];
                gv = w->g0[1];
                gp = w->g0[2];
            }
            swell[k] += x * gs;
            scan[k] += x * gv;
            perc[k] += x * gp;
            phase += inc;
        }
        w->phase = phase;
    }

    /* remove the wheels whose release is over */
    for (i = 0; i < o.nactive; i++) {
        wheel_t *w = &o.w[o.active[i]];
        if (w->removing && w->env_pos >= ORGAN_BLOCK) {
            w->active = 0;
            w->removing = 0;
            w->releasing = 0;
            w->refs = 0;
        } else {
            o.active[wr++] = o.active[i];
        }
    }
    o.nactive = wr;

    if (o.vib_mode != VIB_OFF)
        vib_proc(&o.vib, scan, vibout, n);

    /* percussion high-pass (setBfree's HIPASS_PERCUSSION) */
    if (o.perc_on && n > 0) {
        float temp = perc[n - 1];
        for (i = n - 1; i > 0; i--)
            perc[i] = perc[i - 1] - perc[i];
        perc[0] = o.pz - perc[0];
        o.pz = temp;
    }

    {
        float target = o.swell * (o.perc_on ? o.perc_drawbar_gain : 1.0f);
        float comp_target = (float)wb_key_comp[o.keys_down > 127 ? 127 : o.keys_down] * COMP_SCALE;
        float a = 156.825f / WB_FS;
        float comp_step = (comp_target - o.key_comp) / (float)ORGAN_BLOCK;
        for (i = 0; i < n; i++) {
            float p = o.perc_on ? perc[i] * o.perc_env : 0.0f;
            o.current_gain += a * (target - o.current_gain);
            o.key_comp += comp_step;
            mono[i] = o.current_gain * o.key_comp * (swell[i] + vibout[i] + p);
            o.perc_env *= o.perc_decay;
        }
    }
    if (o.upper_keys == 0)
        o.perc_env = o.perc_reset;
    preamp_run(mono, n);
    reverb_run(&o.rev, mono, mono, n);
    leslie_run(mono, outL, outR, n);
}

void organ_render(float *outL, float *outR, int n)
{
    float mono[ORGAN_BLOCK];
    while (n > 0) {
        int m = n > ORGAN_BLOCK ? ORGAN_BLOCK : n;
        render_block(mono, outL, outR, m);
        outL += m;
        outR += m;
        n -= m;
    }
}

void organ_reset(void)
{
    memset(o.w, 0, sizeof o.w);
    memset(o.active, 0, sizeof o.active);
    memset(o.key_down, 0, sizeof o.key_down);
    o.nactive = 0;
    o.keys_down = 0;
    o.upper_keys = 0;
    o.current_gain = 0.0f;
    o.key_comp = 1.0f;
    o.perc_env = 0.0f;
    o.pz = 0.0f;
    vib_init(&o.vib, WB_FS);
    preamp_init();
    reverb_init(&o.rev);
    leslie_init();
    leslie_set_speed(LESLIE_SLOW);
    o.leslie_mode = LESLIE_SLOW;
    o.drawbar_changed = 1;
}

void organ_init(void)
{
    int i;
    memset(&o, 0, sizeof o);
    for (i = 0; i < NSINE; i++)
        o.sine[i] = fm_sinf(6.28318530718f * (float)i / (float)NSINE);
    for (i = 0; i < ORGAN_BUSES; i++)
        o.drawbar[i] = 0.0f;
    o.drawbar[0] = o.drawbar[1] = o.drawbar[2] = 1.0f;   /* 888000000 */
    o.swell = SWELL_TRIM;
    o.current_gain = 1.0f;               /* setBfree starts here and glides to swell (~25 Hz) */
    o.key_comp = 1.0f;
    o.perc_trig_bus = 8;
    o.perc_send = 4;
    o.perc_fast = 1;
    o.perc_soft = 0;
    o.perc_reset = 11.0f;
    o.perc_drawbar_gain = 0.60512f;
    o.perc_decay = perc_decay_for(1);
    o.perc_env = 0.0f;              /* a silent block re-arms it (setBfree does the same) */
    o.vib_mode = VIB_OFF;
    vib_init(&o.vib, WB_FS);
    preamp_init();
    reverb_init(&o.rev);
    leslie_init();
    leslie_set_speed(LESLIE_SLOW);
    o.leslie_mode = LESLIE_SLOW;
    o.drawbar_changed = 1;
}
