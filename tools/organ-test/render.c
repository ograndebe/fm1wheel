/* SPDX-License-Identifier: GPL-3.0-or-later
 * Host renderer for the WheelBox organ engine. Same event format as
 * tools/setbfree-host/refrender (the subset the dry engine implements).
 *
 * Build: make -C tools/organ-test
 * Run:   tools/organ-test/render events/chord.txt build/chord.wav
 */
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "organ.h"

#define BLOCK ORGAN_BLOCK
#define MAX_EVENTS 8192

enum { EV_NOTE_ON, EV_NOTE_OFF, EV_DRAW, EV_SWELL, EV_PERC, EV_VIB, EV_DRIVE, EV_REVERB, EV_LESLIE };

typedef struct {
    double t;
    int kind, a, b, c;
    char s[16];
} event_t;

static event_t events[MAX_EVENTS];
static int nevents;
static double script_end;

static void die(const char *m)
{
    fprintf(stderr, "render: %s\n", m);
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
    case 1: return note >= 36 && note <= 96 ? note - 36 : -1;
    case 2: return note >= 36 && note <= 96 ? note - 36 + 64 : -1;
    case 3: return note >= 24 && note <= 55 ? note - 24 + 128 : -1;
    default: return -1;
    }
}

static int parse_int(const char *s, const char *what, int lo, int hi)
{
    char *end;
    long v = strtol(s, &end, 10);
    if (!*s || *end || v < lo || v > hi) {
        fprintf(stderr, "render: bad %s '%s'\n", what, s);
        exit(1);
    }
    return (int)v;
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
        char *p = line, *cmd, *a1, *a2, *a3;
        while (isspace((unsigned char)*p))
            p++;
        if (!*p || *p == '#')
            continue;
        cmd = strtok(p, " \t\r\n");
        a1 = strtok(NULL, " \t\r\n");
        a2 = strtok(NULL, " \t\r\n");
        a3 = strtok(NULL, " \t\r\n");
        if (!cmd)
            continue;
        if (!strcmp(cmd, "wait") || !strcmp(cmd, "render")) {
            if (!a1)
                die("wait needs ms");
            now += parse_int(a1, "milliseconds", 0, 600000);
        } else if (!strcmp(cmd, "drawbars")) {
            if (!a1 || !a2 || strlen(a2) != 9)
                die("drawbars needs U|L|P and 9 digits");
            add_event(now, EV_DRAW, manual_offset(a1[0]), 0, 0, a2);
        } else if (!strcmp(cmd, "swell")) {
            if (!a1)
                die("swell needs 0..127");
            add_event(now, EV_SWELL, parse_int(a1, "swell", 0, 127), 0, 0, NULL);
        } else if (!strcmp(cmd, "perc")) {
            if (!a1)
                die("perc needs on|off|harmonic|decay|volume");
            add_event(now, EV_PERC, 0, 0, 0, a1);
            if (a2)
                add_event(now, EV_PERC, 1, 0, 0, a2);
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
            add_event(now, EV_LESLIE, !strcmp(a1, "fast") ? 2 : !strcmp(a1, "slow") ? 1 : 0, 0, 0, NULL);
        } else if (!strcmp(cmd, "vib")) {
            if (!a1)
                die("vib needs off|v1..c3|route");
            if (!strcmp(a1, "route")) {
                if (!a2)
                    die("vib route needs off|upper|lower|both");
                add_event(now, EV_VIB, 1, 0, 0, a2);
            } else {
                add_event(now, EV_VIB, 0, 0, 0, a1);
            }
        } else if (!strcmp(cmd, "noteon") || !strcmp(cmd, "noteoff")) {
            if (!a1 || !a2)
                die("noteon/noteoff needs channel and note");
            add_event(now, !strcmp(cmd, "noteon") ? EV_NOTE_ON : EV_NOTE_OFF,
                      parse_int(a1, "channel", 1, 3), parse_int(a2, "note", 0, 127),
                      a3 ? parse_int(a3, "velocity", 1, 127) : 100, NULL);
        } else if (!strcmp(cmd, "leslie")) {
            /* dry test: the FX chain is not part of this renderer */
        } else {
            fprintf(stderr, "render: unknown command '%s'\n", cmd);
            exit(1);
        }
    }
    fclose(f);
    script_end = now;
}

static void apply(const event_t *e)
{
    switch (e->kind) {
    case EV_NOTE_ON: {
        int k = midi_to_key(e->a, e->b);
        if (k >= 0)
            organ_key_on(k);
        break;
    }
    case EV_NOTE_OFF: {
        int k = midi_to_key(e->a, e->b);
        if (k >= 0)
            organ_key_off(k);
        break;
    }
    case EV_DRAW: {
        int i, m = e->a;
        if (m < 0 || m > 18)
            die("drawbars manual must be U, L or P");
        for (i = 0; i < 9; i++) {
            int v = e->s[i] - '0';
            if (v < 0 || v > 8)
                die("drawbar digit must be 0..8");
            organ_set_drawbar(m / 9, i, v);
        }
        break;
    }
    case EV_SWELL:
        organ_set_swell(e->a);
        break;
    case EV_DRIVE:
        organ_set_drive(e->a);
        break;
    case EV_REVERB:
        organ_set_reverb(e->a);
        break;
    case EV_LESLIE:
        organ_set_leslie(e->a);
        break;
    case EV_VIB:
        if (e->a == 0) {
            int m = !strcmp(e->s, "v1") ? VIB_V1 : !strcmp(e->s, "c1") ? VIB_C1 :
                    !strcmp(e->s, "v2") ? VIB_V2 : !strcmp(e->s, "c2") ? VIB_C2 :
                    !strcmp(e->s, "v3") ? VIB_V3 : !strcmp(e->s, "c3") ? VIB_C3 : VIB_OFF;
            organ_set_vibrato_mode(m);
        } else {
            organ_set_vibrato_routing(!strcmp(e->s, "upper") || !strcmp(e->s, "both"),
                                      !strcmp(e->s, "lower") || !strcmp(e->s, "both"));
        }
        break;
    case EV_PERC:
        if (e->a == 0) {
            if (!strcmp(e->s, "on"))
                organ_set_percussion(1);
            else if (!strcmp(e->s, "off"))
                organ_set_percussion(0);
        } else {
            if (!strcmp(e->s, "2"))
                organ_set_perc_harmonic(1);
            else if (!strcmp(e->s, "3"))
                organ_set_perc_harmonic(0);
            else if (!strcmp(e->s, "fast"))
                organ_set_perc_decay(1);
            else if (!strcmp(e->s, "slow"))
                organ_set_perc_decay(0);
            else if (!strcmp(e->s, "normal"))
                organ_set_perc_volume(0);
            else if (!strcmp(e->s, "soft"))
                organ_set_perc_volume(1);
        }
        break;
    }
}

static void write_wav(const char *path, const float *lr, uint32_t frames, uint32_t rate)
{
    FILE *f = fopen(path, "wb");
    uint32_t data = frames * 8u, riff = 38u + data, fmtsz = 18, rate32 = rate, bytes = rate * 8u;
    uint16_t fmt = 3, ch = 2, bits = 32, block = 8, cb = 0;
    uint8_t h[46] = {0};
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
    memcpy(h + 36, &cb, 2);
    memcpy(h + 38, "data", 4);
    memcpy(h + 42, &data, 4);
    fwrite(h, 1, sizeof h, f);
    fwrite(lr, sizeof(float), (size_t)frames * 2u, f);
    fclose(f);
}

int main(int argc, char **argv)
{
    const char *script, *out;
    double tail = 500.0;
    uint32_t frames, done = 0, k;
    float lt[BLOCK], rt[BLOCK];
    float *lr;
    double peak = 0, sumsq = 0;
    int ei = 0, i;
    double now = 0.0;

    if (argc < 3) {
        fprintf(stderr, "usage: %s SCRIPT OUT.wav [--tail MS]\n", argv[0]);
        return 1;
    }
    script = argv[1];
    out = argv[2];
    for (i = 3; i + 1 < argc; i++)
        if (!strcmp(argv[i], "--tail"))
            tail = atof(argv[++i]);

    parse_script(script);
    frames = (uint32_t)((script_end + tail) * 44100.0 / 1000.0) + 1u;
    lr = calloc((size_t)frames * 2u, sizeof(float));
    if (!lr)
        die("out of memory");
    organ_init();

    while (done < frames) {
        uint32_t n = frames - done < BLOCK ? frames - done : BLOCK;
        while (ei < nevents && events[ei].t <= now) {
            apply(&events[ei]);
            ei++;
        }
        organ_render(lt, rt, (int)n);
        for (k = 0; k < n; k++) {
            float l = lt[k], r = rt[k];
            lr[2u * (done + k)] = l;
            lr[2u * (done + k) + 1u] = r;
            if (fabsf(l) > peak)
                peak = fabsf(l);
            if (fabsf(r) > peak)
                peak = fabsf(r);
            sumsq += (double)l * l + (double)r * r;
        }
        done += n;
        now += 1000.0 * (double)n / 44100.0;
    }
    write_wav(out, lr, frames, 44100);
    printf("%s: %.2f s, peak %.1f dBFS, rms %.1f dBFS, %d events\n", out,
           (double)frames / 44100.0, 20.0 * log10(peak > 0 ? peak : 1e-30),
           10.0 * log10(sumsq / (2.0 * frames) > 0 ? sumsq / (2.0 * frames) : 1e-30), nevents);
    free(lr);
    return 0;
}
