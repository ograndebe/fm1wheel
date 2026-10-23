/* SPDX-License-Identifier: GPL-2.0-or-later
 * Reference renderer for WheelBox: the unmodified setBfree DSP (tonegen,
 * scanner, preamp, reverb, Leslie) driven by a small text script.
 *
 * This is the "golden" side of the A/B tests. It is a host tool, not firmware.
 *
 * Build:  make -C tools/setbfree-host
 * Run:    tools/setbfree-host/refrender events/demo.txt /tmp/demo.wav
 *
 * Script (one event per line; "#" comments):
 *   wait 500                 advance the timeline (ms); events apply at block starts
 *   render 2000              same as wait (the final render covers the whole timeline)
 *   drawbars U 888000000     upper/lower/pedal: 9 digits, 0..8
 *   swell 127                swell pedal, 0..127
 *   perc on|off
 *   perc harmonic 2|3
 *   perc decay fast|slow
 *   perc volume normal|soft
 *   vib off|v1|c1|v2|c2|v3|c3
 *   vib route off|upper|lower|both
 *   drive 0..127             0 = clean, higher = more preamp character
 *   reverb 0..127            dry .. wet
 *   leslie stop|slow|fast
 *   noteon <ch> <note> <vel> ch 1 upper, 2 lower, 3 pedals
 *   noteoff <ch> <note>
 */
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "host.h"
#include "tonegen.h"
#include "overdrive.h"
#include "reverb.h"
#include "whirl.h"

#define BLOCK 128
#define MAX_EVENTS 8192

enum { EV_NOTE_ON, EV_NOTE_OFF, EV_DRAW, EV_SWELL, EV_PERC, EV_VIB, EV_DRIVE, EV_REVERB, EV_LESLIE };

typedef struct {
    double t;
    int kind;
    int a, b, c;
    char s[16];
} event_t;

static event_t events[MAX_EVENTS];
static int nevents;
static double script_end;

static void die(const char *msg)
{
    fprintf(stderr, "refrender: %s\n", msg);
    exit(1);
}

static int manual_offset(char m)
{
    switch (toupper((unsigned char)m)) {
    case 'U': return 0;
    case 'L': return 9;
    case 'P': return 18;
    default: return -1;
    }
}

static int midi_to_key(int ch, int note)
{
    switch (ch) {
    case 1: return note >= 36 && note <= 96 ? note - 36 : -1;       /* upper: keys 0..60 */
    case 2: return note >= 36 && note <= 96 ? note - 36 + 64 : -1;  /* lower: keys 64..124 */
    case 3: return note >= 24 && note <= 55 ? note - 24 + 128 : -1; /* pedals: keys 128..159 */
    default: return -1;
    }
}

static void add_event(double t, int kind, int a, int b, int c, const char *s)
{
    if (nevents >= MAX_EVENTS)
        die("too many events");
    events[nevents].t = t;
    events[nevents].kind = kind;
    events[nevents].a = a;
    events[nevents].b = b;
    events[nevents].c = c;
    events[nevents].s[0] = 0;
    if (s)
        snprintf(events[nevents].s, sizeof events[nevents].s, "%s", s);
    nevents++;
}

static int parse_int(const char *s, const char *what, int lo, int hi)
{
    char *end;
    long v = strtol(s, &end, 10);
    if (!*s || *end || v < lo || v > hi) {
        fprintf(stderr, "refrender: bad %s '%s'\n", what, s);
        exit(1);
    }
    return (int)v;
}

static void parse_script(const char *path)
{
    FILE *f = fopen(path, "r");
    char line[512];
    double now = 0.0;

    if (!f) {
        perror(path);
        exit(1);
    }
    while (fgets(line, sizeof line, f)) {
        char *p = line, *cmd, *a1, *a2, *a3, *a4;
        while (isspace((unsigned char)*p))
            p++;
        if (!*p || *p == '#')
            continue;
        cmd = strtok(p, " \t\r\n");
        a1 = strtok(NULL, " \t\r\n");
        a2 = strtok(NULL, " \t\r\n");
        a3 = strtok(NULL, " \t\r\n");
        a4 = strtok(NULL, " \t\r\n");
        if (!cmd)
            continue;

        if (!strcmp(cmd, "wait") || !strcmp(cmd, "render")) {
            if (!a1 || a2)
                die("wait/render needs one number (ms)");
            now += parse_int(a1, "milliseconds", 0, 600000);
        } else if (!strcmp(cmd, "drawbars")) {
            if (!a1 || !a2 || strlen(a2) != 9)
                die("drawbars needs U|L|P and 9 digits");
            add_event(now, EV_DRAW, manual_offset(a1[0]), 0, 0, a2);
        } else if (!strcmp(cmd, "swell")) {
            if (!a1)
                die("swell needs 0..127");
            add_event(now, EV_SWELL, parse_int(a1, "swell", 0, 127), 0, 0, NULL);
        } else if (!strcmp(cmd, "noteon") || !strcmp(cmd, "noteoff")) {
            if (!a1 || !a2)
                die("noteon/noteoff needs channel and note");
            add_event(now, !strcmp(cmd, "noteon") ? EV_NOTE_ON : EV_NOTE_OFF,
                      parse_int(a1, "channel", 1, 3), parse_int(a2, "note", 0, 127),
                      a3 ? parse_int(a3, "velocity", 1, 127) : 100, NULL);
        } else if (!strcmp(cmd, "perc")) {
            if (!a1)
                die("perc needs on|off|harmonic|decay|volume");
            add_event(now, EV_PERC, 0, 0, 0, a1);
            if (a2)
                add_event(now, EV_PERC, 1, 0, 0, a2);
        } else if (!strcmp(cmd, "vib")) {
            if (!a1)
                die("vib needs off|v1..c3|route");
            add_event(now, EV_VIB, 0, 0, 0, a1);
            if (a2)
                add_event(now, EV_VIB, 1, 0, 0, a2);
        } else if (!strcmp(cmd, "drive")) {
            if (!a1)
                die("drive needs 0..127");
            add_event(now, EV_DRIVE, parse_int(a1, "drive", 0, 127), 0, 0, NULL);
        } else if (!strcmp(cmd, "reverb")) {
            if (!a1)
                die("reverb needs 0..127");
            add_event(now, EV_REVERB, parse_int(a1, "reverb", 0, 127), 0, 0, NULL);
        } else if (!strcmp(cmd, "leslie")) {
            if (!a1)
                die("leslie needs stop|slow|fast");
            add_event(now, EV_LESLIE, 0, 0, 0, a1);
        } else {
            fprintf(stderr, "refrender: unknown command '%s'\n", cmd);
            exit(1);
        }
        (void)a4;
    }
    fclose(f);
    script_end = now;
}

static void apply_event(struct b_tonegen *t, void *pa, struct b_reverb *rv, struct b_whirl *wh,
                        const event_t *e)
{
    switch (e->kind) {
    case EV_NOTE_ON: {
        int key = midi_to_key(e->a, e->b);
        if (key >= 0)
            oscKeyOn(t, (unsigned char)key, 255);
        break;
    }
    case EV_NOTE_OFF: {
        int key = midi_to_key(e->a, e->b);
        if (key >= 0)
            oscKeyOff(t, (unsigned char)key, 255);
        break;
    }
    case EV_DRAW: {
        int i, bus = e->a;
        if (bus < 0 || bus > 18)
            die("drawbars manual must be U, L or P");
        for (i = 0; i < 9; i++) {
            int v = e->s[i] - '0';
            if (v < 0 || v > 8)
                die("drawbar digit must be 0..8");
            t->drawBarGain[bus + i] = t->drawBarLevel[bus + i][v];
            t->drawBarChange = 1;
        }
        break;
    }
    case EV_SWELL:
        t->swellPedalGain = (t->outputLevelTrim * (double)e->a) / 127.0;
        break;
    case EV_PERC:
        if (e->a == 0) {
            if (!strcmp(e->s, "on"))
                setPercussionEnabled(t, 1);
            else if (!strcmp(e->s, "off"))
                setPercussionEnabled(t, 0);
            else if (strcmp(e->s, "harmonic") && strcmp(e->s, "decay") && strcmp(e->s, "volume"))
                die("perc needs on|off|harmonic|decay|volume");
        } else {
            if (!strcmp(e->s, "2"))
                setPercussionFirst(t, 1);
            else if (!strcmp(e->s, "3"))
                setPercussionFirst(t, 0);
            else if (!strcmp(e->s, "fast"))
                setPercussionFast(t, 1);
            else if (!strcmp(e->s, "slow"))
                setPercussionFast(t, 0);
            else if (!strcmp(e->s, "normal"))
                setPercussionVolume(t, 0);
            else if (!strcmp(e->s, "soft"))
                setPercussionVolume(t, 1);
            else
                die("bad perc argument");
        }
        break;
    case EV_VIB: {
        struct b_vibrato *v = &t->inst_vibrato;
        if (e->a == 0) {
            if (!strcmp(e->s, "off")) {
                v->effectEnabled = 0;
            } else if (!strcmp(e->s, "v1") || !strcmp(e->s, "c1") ||
                       !strcmp(e->s, "v2") || !strcmp(e->s, "c2") ||
                       !strcmp(e->s, "v3") || !strcmp(e->s, "c3")) {
                v->effectEnabled = 1;
                v->mixedBuffers = e->s[0] == 'c';
                v->offsetTable = e->s[1] == '1' ? v->offset1Table : e->s[1] == '2' ? v->offset2Table : v->offset3Table;
            } else if (strcmp(e->s, "route")) {
                die("vib needs off|v1..c3|route");
            }
        } else {
            if (!strcmp(e->s, "off")) {
                setVibratoUpper(t, 0);
                setVibratoLower(t, 0);
            } else if (!strcmp(e->s, "upper")) {
                setVibratoUpper(t, 1);
                setVibratoLower(t, 0);
            } else if (!strcmp(e->s, "lower")) {
                setVibratoUpper(t, 0);
                setVibratoLower(t, 1);
            } else if (!strcmp(e->s, "both")) {
                setVibratoUpper(t, 1);
                setVibratoLower(t, 1);
            } else {
                die("vib route needs off|upper|lower|both");
            }
        }
        break;
    }
    case EV_DRIVE:
        if (e->a == 0) {
            setClean(pa, 1);
        } else {
            setClean(pa, 0);
            fctl_biased(pa, (float)e->a / 127.0f);
        }
        break;
    case EV_REVERB:
        setReverbMix(rv, (float)e->a / 127.0f);
        break;
    case EV_LESLIE:
        if (!strcmp(e->s, "stop"))
            setRevSelect(wh, WHIRL_STOP);
        else if (!strcmp(e->s, "slow"))
            setRevSelect(wh, WHIRL_SLOW);
        else if (!strcmp(e->s, "fast"))
            setRevSelect(wh, WHIRL_FAST);
        else
            die("leslie needs stop|slow|fast");
        break;
    }
}

/* WAVE_FORMAT_IEEE_FLOAT, 32-bit stereo. */
static void write_float_wav(const char *path, const float *lr, uint32_t frames, uint32_t rate)
{
    FILE *f = fopen(path, "wb");
    uint32_t data = frames * 8u;
    uint8_t h[46] = {0};
    uint16_t fmt = 3, ch = 2, bits = 32, block = 8, cbsize = 0;
    uint32_t riff = 38u + data, rate32 = rate, bytes = rate * 8u;
    uint32_t fmtsz = 18;

    if (!f) {
        perror(path);
        exit(1);
    }
    memcpy(h, "RIFF", 4);
    memcpy(h + 4, &riff, 4);
    memcpy(h + 8, "WAVEfmt ", 8);
    memcpy(h + 16, &fmtsz, 4);
    memcpy(h + 20, &fmt, 2);
    memcpy(h + 22, &ch, 2);
    memcpy(h + 24, &rate32, 4);
    memcpy(h + 28, &bytes, 4);
    memcpy(h + 32, &block, 2);
    memcpy(h + 34, &bits, 2);
    memcpy(h + 36, &cbsize, 2);
    memcpy(h + 38, "data", 4);
    memcpy(h + 42, &data, 4);
    fwrite(h, 1, sizeof h, f);
    fwrite(lr, sizeof(float), (size_t)frames * 2u, f);
    fclose(f);
}

int main(int argc, char **argv)
{
    const char *script, *out;
    double gain = 1.0, tail_ms = 500.0, sr = 44100.0;
    int i, dry = 0;

    if (argc < 3) {
        fprintf(stderr, "usage: %s SCRIPT OUT.wav [--gain G] [--tail MS] [--sr HZ]\n", argv[0]);
        return 1;
    }
    script = argv[1];
    out = argv[2];
    for (i = 3; i < argc; i++) {
        if (!strcmp(argv[i], "--gain") && i + 1 < argc)
            gain = atof(argv[++i]);
        else if (!strcmp(argv[i], "--tail") && i + 1 < argc)
            tail_ms = atof(argv[++i]);
        else if (!strcmp(argv[i], "--dry"))
            dry = 1;
        else if (!strcmp(argv[i], "--sr") && i + 1 < argc)
            sr = atof(argv[++i]);
        else {
            fprintf(stderr, "refrender: unknown option %s\n", argv[i]);
            return 1;
        }
    }

    SampleRateD = sr;
    SampleRateI = (int)(sr + 0.5);
    parse_script(script);

    {
        struct b_tonegen *t = allocTonegen();
        void *pa = allocPreamp();
        struct b_reverb *rv = allocReverb();
        struct b_whirl *wh = allocWhirl();
        float bufA[BLOCK], bufB[BLOCK], bufC[BLOCK];
        float left[BLOCK], right[BLOCK], tmpL[BLOCK], tmpR[BLOCK];
        double total_ms = script_end + tail_ms;
        uint32_t frames = (uint32_t)(total_ms * sr / 1000.0) + 1u;
        float *outlr = calloc((size_t)frames * 2u, sizeof(float));
        uint32_t done = 0, k;
        int ei = 0;
        double now = 0.0;
        double peak = 0.0, sumsq = 0.0;

        if (!t || !pa || !rv || !wh || !outlr)
            die("out of memory");

        initToneGenerator(t, NULL);
        initPreamp(pa, NULL);
        initReverb(rv, NULL, sr);
        initWhirl(wh, NULL, sr);

        /* defaults: 888000000 upper, clean preamp, a little reverb, slow Leslie */
        for (i = 0; i < NOF_BUSES; i++) {
            t->drawBarGain[i] = 0;
            t->drawBarChange = 1;
        }
        for (i = 0; i < 9; i++) {
            t->drawBarGain[i] = t->drawBarLevel[i][8];
            t->drawBarChange = 1;
        }
        t->swellPedalGain = t->outputLevelTrim;
        setClean(pa, 1);
        setReverbMix(rv, 0.2f);
        setRevSelect(wh, WHIRL_SLOW);

        while (done < frames) {
            uint32_t n = frames - done < BLOCK ? frames - done : BLOCK;
            while (ei < nevents && events[ei].t <= now) {
                apply_event(t, pa, rv, wh, &events[ei]);
                ei++;
            }
            oscGenerateFragment(t, bufA, BLOCK);
            if (dry) {
                memcpy(left, bufA, sizeof left);
                memcpy(right, bufA, sizeof right);
            } else {
                preamp(pa, bufA, bufB, BLOCK);
                reverb(rv, bufB, bufC, BLOCK);
                whirlProc3(wh, bufC, left, right, tmpL, tmpR, BLOCK);
            }
            for (k = 0; k < n; k++) {
                float l = left[k] * (float)gain, r = right[k] * (float)gain;
                outlr[2u * (done + k)] = l;
                outlr[2u * (done + k) + 1u] = r;
                if (fabsf(l) > peak)
                    peak = fabsf(l);
                if (fabsf(r) > peak)
                    peak = fabsf(r);
                sumsq += (double)l * l + (double)r * r;
            }
            done += n;
            now += 1000.0 * (double)n / sr;
        }

        write_float_wav(out, outlr, frames, SampleRateI);
        printf("%s: %.2f s, peak %.1f dBFS, rms %.1f dBFS, %d active wheels, %d events\n", out,
               (double)frames / sr, 20.0 * log10(peak > 0 ? peak : 1e-30),
               10.0 * log10(sumsq / (2.0 * frames) > 0 ? sumsq / (2.0 * frames) : 1e-30),
               t->activeOscLEnd, nevents);
        free(outlr);
    }
    return 0;
}
