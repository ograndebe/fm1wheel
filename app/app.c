/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Rafael
 * fm1-wheel: FM-1 tonewheel organ app. Phase 1: the setBfree-derived tone
 * generator is live; this UI is a working drawbar page, not the final one.
 * The small text/note helpers are adapted from fm1-base's template app.c
 * (MPL-2.0, Copyright (C) 2026 Charles Vestal). */
#include "dsp/wb_libc.h"

#include "fastmath.h"
#include "dsp/organ.h"
#include "dsp/leslie.h"

#define SET_MAGIC 0x57424F5Bu   /* "WBOX5": the layout changed with the Leslie */

typedef struct {
    uint32_t magic;
    int8_t octave;              /* -2..+2 */
    uint8_t swell;              /* 0..127 */
    uint8_t perc;
    uint8_t perc_second;        /* 1 = 2nd, 0 = 3rd */
    uint8_t perc_fast;
    uint8_t preset;
    uint8_t vib_mode;           /* VIB_OFF .. VIB_C3 */
    uint8_t drive;              /* 0 clean .. 127 */
    uint8_t drive_char;         /* 0..127 */
    uint8_t reverb;             /* 0..127 */
    uint8_t leslie;             /* LESLIE_STOP/SLOW/FAST */
    uint8_t bars[ORGAN_BUSES];  /* 0..8, per manual */
} settings_t;

static settings_t set;
static volatile uint8_t set_dirty;

static const char *const REG_NAME[8] = {"3BAR", "BLUES", "ROCK", "TOPS", "FLUTE", "FULL", "BASS", "STRNG"};
static const char REG[8][10] = {
    "888000000", "888800000", "888000008", "800000888",
    "008000000", "888888888", "858000000", "006876540",
};
static const char *const MANUAL_NAME[3] = {"UPPER", "LOWER", "PEDAL"};
static const char *const VIB_NAME[7] = {"OFF", "V1", "C1", "V2", "C2", "V3", "C3"};
static const char *const FOOT[9] = {"16", "5.3", "8", "4", "2.6", "2", "1.6", "1.3", "1"};

static volatile int32_t last_note = -1;
static uint32_t prev_keys;
static uint32_t prev_btn;
static uint8_t drawbar_mode;             /* the 27 keys edit drawbars instead of playing */
static uint8_t led_key_count;            /* keys lit at the last frame (host test) */
static int16_t key_org[NKEYS];           /* 0: not held; else the organ key it started + 1 */

/* Pair -> drawbar and direction. -1: not a drawbar control.
 * F#/G, G#/A, A#/B, C#/D, D#/E, then the same an octave up.
 * F3, C4, F4 and C5 are ignored; D#5..G5 are spare for later functions. */
static const int8_t KEY_BAR[NKEYS] = {-1, 0, 0, 1, 1, 2, 2, -1, 3, 3, 4, 4, -1, 5, 5, 6, 6, 7, 7, -1, 8, 8, -1, -1, -1, -1, -1};
static const int8_t KEY_DIR[NKEYS] = { 0, 1,-1, 1,-1, 1,-1,  0, 1,-1, 1,-1,  0, 1,-1, 1,-1, 1,-1,  0, 1,-1,  0,  0,  0,  0,  0};

static struct {
    uint8_t page;               /* 0: bars 0-3, 1: 4-7, 2: 8 + percussion */
    uint8_t manual;             /* 0 upper, 1 lower, 2 pedal */
    uint8_t redraw;
    uint32_t msg_until;
    char msg[2][24];
} ui;

/* --------------------------------------------------------------- helpers --- */
static void text_cat(char *d, const char *s)
{
    while (*d)
        d++;
    while (*s)
        *d++ = *s++;
    *d = 0;
}

static void int_text(int32_t v, char *b)
{
    char t[12];
    int n = 0;
    uint32_t u = (uint32_t)(v < 0 ? -v : v);
    do
        t[n++] = (char)('0' + u % 10u);
    while ((u /= 10u) && n < 11);
    if (v < 0)
        *b++ = '-';
    while (n)
        *b++ = t[--n];
    *b = 0;
}

static void note_name(int32_t n, char *t)
{
    static const char *const N[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    if (n < 0) {
        t[0] = '-';
        t[1] = 0;
        return;
    }
    {
        int oct = n / 12 - 1, p = 0;
        t[p++] = N[n % 12][0];
        if (N[n % 12][1])
            t[p++] = N[n % 12][1];
        if (oct >= 10) {
            t[p++] = (char)('0' + oct / 10);
            t[p++] = (char)('0' + oct % 10);
        } else if (oct >= 0) {
            t[p++] = (char)('0' + oct);
        }
        t[p] = 0;
    }
}

static void say(const char *a, const char *b)
{
    uint32_t i;
    char t[48];
    for (i = 0; i < 23u && a && a[i]; i++)
        ui.msg[0][i] = a[i];
    ui.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++)
        ui.msg[1][i] = b[i];
    ui.msg[1][i] = 0;
    ui.msg_until = plat_ms() + 1200u;
    ui.redraw = 1;
    if (ann_on()) {
        t[0] = 0;
        text_cat(t, ui.msg[0]);
        if (ui.msg[1][0]) {
            text_cat(t, " ");
            text_cat(t, ui.msg[1]);
        }
        ann_say(t, 0, ANN_NOW);
    }
}

static void mark_dirty(void)
{
    set_dirty = 1;
    ui.redraw = 1;
}

static void save(void)
{
    if (plat_store_save(OBJ_SETTINGS, &set, sizeof set) == 0) {
        set_dirty = 0;
        say("SAVED", "");
    } else {
        say("SAVE", "FAILED");
    }
}

static void defaults(void)
{
    memset(&set, 0, sizeof set);
    set.magic = SET_MAGIC;
    set.octave = 0;
    set.swell = 127;
    set.perc = 0;
    set.perc_second = 0;
    set.perc_fast = 1;
    set.preset = 0;
    set.vib_mode = VIB_OFF;
    set.drive = 0;
    set.drive_char = 64;
    set.reverb = 40;
    set.leslie = LESLIE_SLOW;
    set.bars[0] = set.bars[1] = set.bars[2] = 8;   /* 888000000 */
}

static void apply_all(void)
{
    int m, i;
    for (m = 0; m < 3; m++)
        for (i = 0; i < 9; i++)
            organ_set_drawbar(m, i, set.bars[m * 9 + i]);
    organ_set_swell(set.swell);
    organ_set_perc_harmonic(set.perc_second);
    organ_set_perc_decay(set.perc_fast);
    organ_set_perc_volume(0);
    organ_set_percussion(set.perc);
    organ_set_vibrato_routing(set.vib_mode != VIB_OFF, 0);
    organ_set_vibrato_mode(set.vib_mode);
    organ_set_drive(set.drive);
    organ_set_drive_character(set.drive_char);
    organ_set_reverb(set.reverb);
    organ_set_leslie(set.leslie);
}

static void preset_apply(void)
{
    int i;
    for (i = 0; i < 9; i++) {
        int v = REG[set.preset][i] - '0';
        set.bars[ui.manual * 9 + i] = (uint8_t)v;
        organ_set_drawbar(ui.manual, i, v);
    }
    mark_dirty();
    say("PRESET", REG_NAME[set.preset]);
}

static int cc_to_bar(int v)
{
    int x = 127 - v;
    return (x * 8 + 63) / 127;
}

static int midi_key(int ch, int note)
{
    if (ch == 2 && note >= 36 && note <= 96)
        return note - 36 + 64;
    if (ch == 3 && note >= 24 && note <= 55)
        return note - 24 + 128;
    if (note >= 36 && note <= 96)
        return note - 36;
    return -1;
}

/* ----------------------------------------------------------------- input --- */
static void midi_in(void)
{
    uint32_t pkt;
    int port;
    while (plat_midi_in(&pkt, &port)) {
        uint32_t st = (pkt >> 8) & 0xFFu, d1 = (pkt >> 16) & 0x7Fu, d2 = pkt >> 24;
        int ch = (int)(st & 0x0Fu) + 1;
        if (st == 0xF8u || st == 0xFEu)
            continue;
        if ((st & 0xF0u) == 0x90u && d2) {
            int k = midi_key(ch, (int)d1);
            if (k >= 0) {
                organ_key_on(k);
                last_note = (int32_t)d1;
            }
        } else if ((st & 0xF0u) == 0x80u || ((st & 0xF0u) == 0x90u && !d2)) {
            int k = midi_key(ch, (int)d1);
            if (k >= 0)
                organ_key_off(k);
            if (last_note == (int32_t)d1)
                last_note = -1;
        } else if ((st & 0xF0u) == 0xB0u) {
            if (d1 == 1 || d1 == 11) {
                set.swell = (uint8_t)d2;
                organ_set_swell((int)d2);
                mark_dirty();
            } else if (d1 >= 70 && d1 <= 78) {
                int bar = (int)d1 - 70;
                int v = cc_to_bar((int)d2);
                set.bars[bar] = (uint8_t)v;
                organ_set_drawbar(0, bar, v);
                mark_dirty();
            } else if (d1 == 80) {
                set.perc = d2 >= 64;
                organ_set_percussion(set.perc);
                mark_dirty();
            } else if (d1 == 81) {
                set.perc_fast = d2 >= 64;
                organ_set_perc_decay(set.perc_fast);
                mark_dirty();
            } else if (d1 == 82) {
                set.perc_second = d2 >= 64;
                organ_set_perc_harmonic(set.perc_second);
                mark_dirty();
            } else if (d1 == 92) {
                set.vib_mode = (uint8_t)((d2 * 7u) / 128u);
                organ_set_vibrato_mode(set.vib_mode);
                mark_dirty();
            } else if (d1 == 95) {
                static const uint8_t R[4][2] = {{0, 0}, {0, 1}, {1, 0}, {1, 1}};
                int r = (int)(d2 / 32u);
                organ_set_vibrato_routing(R[r][0], R[r][1]);
                mark_dirty();
            } else if (d1 == 65) {
                set.drive = d2 >= 64 ? 80 : 0;
                organ_set_drive(set.drive);
                mark_dirty();
            } else if (d1 == 93) {
                set.drive_char = (uint8_t)d2;
                organ_set_drive_character(set.drive_char);
                mark_dirty();
            } else if (d1 == 64 && d2 >= 64) {
                set.leslie = set.leslie == LESLIE_SLOW ? LESLIE_FAST : LESLIE_SLOW;
                organ_set_leslie(set.leslie);
                mark_dirty();
            }
        } else if ((st & 0xF0u) == 0xC0u) {
            set.preset = (uint8_t)(d1 & 7u);
            preset_apply();
        }
    }
}

static void release_local_keys(void)
{
    uint32_t k;
    for (k = 0; k < NKEYS; k++) {
        if (key_org[k]) {
            organ_key_off(key_org[k] - 1);
            key_org[k] = 0;
        }
    }
    last_note = -1;
}

static void drawbar_step(int bar, int dir)
{
    int m = ui.manual;
    int v;
    if (bar < 0 || bar >= 9 || !dir)
        return;
    v = (int)set.bars[m * 9 + bar] + dir;
    if (v < 0)
        v = 0;
    if (v > 8)
        v = 8;
    if (v == set.bars[m * 9 + bar])
        return;
    set.bars[m * 9 + bar] = (uint8_t)v;
    organ_set_drawbar(m, bar, v);
    mark_dirty();
}

static void keys_poll(void)
{
    uint32_t now = plat_keys();
    uint32_t ch = now ^ prev_keys;
    uint32_t k;
    for (k = 0; k < NKEYS; k++) {
        int note, key;
        if (!(ch & (1u << k)))
            continue;
        if (drawbar_mode) {
            if (now & (1u << k))
                drawbar_step(KEY_BAR[k], KEY_DIR[k]);
            continue;
        }
        note = 53 + (int)k + (int)set.octave * 12;
        key = note - 36;
        if (now & (1u << k)) {
            if (key < 0 || key > 60)
                continue;
            organ_key_on(key);
            key_org[k] = (int16_t)(key + 1);
            last_note = note;
        } else if (key_org[k]) {
            organ_key_off(key_org[k] - 1);
            key_org[k] = 0;
            if (last_note == note)
                last_note = -1;
        }
    }
    prev_keys = now;
}

/* ------------------------------------------------------------------- UI --- */
static void octave_text(char *t)
{
    t[0] = set.octave < 0 ? '-' : '+';
    t[1] = (char)('0' + (set.octave < 0 ? -set.octave : set.octave));
    t[2] = 0;
}

static void draw(void)
{
    char t[24], o[4];
    int i, base = ui.page * 4;
    uint32_t now = plat_ms();

    cv_begin(240, 32, C_BLACK);
    if (now < ui.msg_until) {
        int32_t x = cv_text(8, 6, &FONT_B, ui.msg[0], C_WHITE);
        cv_text(x + 6, 6, &FONT_B, ui.msg[1], C_HI);
    } else {
        int32_t x = cv_text(8, 6, &FONT_B, APP_NAME, C_WHITE);
        note_name(last_note, t);
        cv_text(x + 8, 6, &FONT_B, t, C_HI);
        int_text((int32_t)plat_cpu_pct(), t);
        text_cat(t, "%");
        cv_text(206, 6, &FONT_XS, t, C_GRAY);
    }
    cv_blit(0, 0);

    cv_begin(240, 144, C_BLACK);
    cv_text(6, 2, &FONT_XS, MANUAL_NAME[ui.manual], C_AMB);
    cv_text(66, 2, &FONT_XS, REG_NAME[set.preset], C_GRAY);
    if (set.perc)
        cv_text(132, 2, &FONT_XS, set.perc_second ? "PERC 2ND" : "PERC 3RD", C_HI);
    else
        cv_text(132, 2, &FONT_XS, "PERC OFF", C_DIM);
    cv_text(204, 2, &FONT_XS, VIB_NAME[set.vib_mode], C_AMB);

    for (i = 0; i < 9; i++) {
        int32_t x = 6 + i * 25, y = 18, w = 20, h = 84;
        int v = set.bars[ui.manual * 9 + i];
        int fh = v * (h - 4) / 8;
        uint16_t edge = (i >= base && i < base + 4) ? C_AMB : C_LINE;
        cv_rect(x, y, w, h, C_BLACK);
        cv_rect(x, y, w, 1, edge);
        cv_rect(x, y + h - 1, w, 1, edge);
        cv_rect(x, y, 1, h, edge);
        cv_rect(x + w - 1, y, 1, h, edge);
        if (fh)
            cv_rect(x + 2, y + h - 2 - fh, w - 4, fh, C_HI);
        cv_text(x, y + h + 3, &FONT_XS, FOOT[i], C_GRAY);
        int_text(v, t);
        cv_text(x + (w - 5) / 2, y + h + 15, &FONT_XS, t, C_WHITE);
    }
    cv_blit(0, 36);

    cv_begin(240, 60, C_BLACK);
    octave_text(o);
    cv_text(6, 4, &FONT_XS, "OCT", C_GRAY);
    cv_text(34, 4, &FONT_XS, o, C_HI);
    cv_text(66, 4, &FONT_XS, "PAGE", C_GRAY);
    t[0] = (char)('1' + ui.page);
    t[1] = '/';
    t[2] = '3';
    t[3] = 0;
    cv_text(98, 4, &FONT_XS, t, C_HI);
    cv_text(130, 4, &FONT_XS, "VOL", C_GRAY);
    int_text(plat_master() * 100 / 4096, t);
    text_cat(t, "%");
    cv_text(156, 4, &FONT_XS, t, C_HI);
    if (drawbar_mode)
        cv_text(6, 22, &FONT_XS, "DRAWBAR KEYS  FLAT=UP  NAT=DOWN", C_HI);
    else {
        cv_text(6, 22, &FONT_XS, "K1-K4: bars   SEL: page   EDIT: manual", C_DIM);
        cv_text(168, 22, &FONT_XS, set.leslie == LESLIE_STOP ? "ROTOR STOP" :
                                    set.leslie == LESLIE_SLOW ? "ROTOR SLOW" : "ROTOR FAST", C_HI);
    }
    if (set_dirty)
        cv_text(6, 38, &FONT_XS, "SAVE has changes", C_AMB);
    else
        cv_text(6, 38, &FONT_XS, "SAVE clean", C_DIM);
    cv_text(120, 38, &FONT_XS, set.drive ? "DRIVE ON" : "DRIVE OFF", set.drive ? C_HI : C_DIM);
    int_text((int32_t)set.reverb * 100 / 127, t);
    text_cat(t, "%");
    cv_text(196, 38, &FONT_XS, t, C_AMB);
    cv_blit(0, 180);
}

static void octave_change(int d)
{
    int o = (int)set.octave + d;
    if (o < -2 || o > 2)
        return;
    set.octave = (int8_t)o;
    release_local_keys();
    prev_keys = drawbar_mode ? plat_keys() : 0;   /* play mode: held keys retrigger */
    last_note = -1;
    mark_dirty();
    say("OCTAVE", o > 0 ? "+1" : o < 0 ? "-1" : "0");
}

static void button(int b, uint32_t now)
{
    switch (b) {
    case B_OCTDN: octave_change(-1); break;
    case B_OCTUP: octave_change(+1); break;
    case B_ENV:
        set.perc = !set.perc;
        organ_set_percussion(set.perc);
        mark_dirty();
        say("PERCUSSION", set.perc ? "ON" : "OFF");
        break;
    case B_LFO:
        set.vib_mode = (uint8_t)((set.vib_mode + 1u) % 7u);
        organ_set_vibrato_routing(set.vib_mode != VIB_OFF, 0);
        organ_set_vibrato_mode(set.vib_mode);
        mark_dirty();
        say("VIBRATO", VIB_NAME[set.vib_mode]);
        break;
    case B_PLAY:
        set.leslie = (uint8_t)((set.leslie + 1u) % 3u);
        organ_set_leslie(set.leslie);
        mark_dirty();
        say("ROTOR", set.leslie == LESLIE_STOP ? "STOP" : set.leslie == LESLIE_SLOW ? "SLOW" : "FAST");
        break;
    case B_ARP:
        set.perc_second = !set.perc_second;
        organ_set_perc_harmonic(set.perc_second);
        mark_dirty();
        say("PERC", set.perc_second ? "2ND" : "3RD");
        break;
    case B_FX:
        set.drive = set.drive ? 0 : 80;
        organ_set_drive(set.drive);
        mark_dirty();
        say("DRIVE", set.drive ? "ON" : "OFF");
        break;
    case B_SEQ: {
        char t[8];
        set.reverb = set.reverb == 0 ? 40 : set.reverb == 40 ? 80 : set.reverb == 80 ? 127 : 0;
        organ_set_reverb(set.reverb);
        mark_dirty();
        int_text((int32_t)set.reverb * 100 / 127, t);
        text_cat(t, "%");
        say("REVERB", t);
        break;
    }
    case B_SEL:
        ui.page = (uint8_t)((ui.page + 1u) % 3u);
        ui.redraw = 1;
        break;
    case B_EDIT:
        ui.manual = (uint8_t)((ui.manual + 1u) % 3u);
        ui.redraw = 1;
        say("MANUAL", MANUAL_NAME[ui.manual]);
        break;
    case B_HOME:
        set.preset = 0;
        preset_apply();
        break;
    case B_GLO:
        drawbar_mode = (uint8_t)!drawbar_mode;
        release_local_keys();
        prev_keys = plat_keys();
        ui.redraw = 1;
        say("DRAWBAR KEYS", drawbar_mode ? "ON" : "OFF");
        break;
    case B_SAVE:
        save();
        break;
    default:
        break;
    }
    (void)now;
}

static void encoder(int role, int d)
{
    if (!d)
        return;
    if (role == EN_SELECT) {
        ui.page = (uint8_t)((ui.page + (d > 0 ? 1u : 2u)) % 3u);
        ui.redraw = 1;
    } else if (role == EN_ALGO) {
        ui.manual = (uint8_t)((ui.manual + (d > 0 ? 1u : 2u)) % 3u);
        ui.redraw = 1;
        say("MANUAL", MANUAL_NAME[ui.manual]);
    } else if (role == EN_PRESET) {
        set.preset = (uint8_t)((set.preset + (d > 0 ? 1u : 7u)) % 8u);
        preset_apply();
    } else if (role >= EN_K1 && role <= EN_K4) {
        int slot = ui.page * 4 + (role - EN_K1);
        if (slot < 9) {
            int v = (int)set.bars[ui.manual * 9 + slot] + d;
            if (v < 0)
                v = 0;
            if (v > 8)
                v = 8;
            set.bars[ui.manual * 9 + slot] = (uint8_t)v;
            organ_set_drawbar(ui.manual, slot, v);
            mark_dirty();
        } else if (slot == 9) {
            set.perc = !set.perc;
            organ_set_percussion(set.perc);
            mark_dirty();
        } else if (slot == 10) {
            set.perc_second = !set.perc_second;
            organ_set_perc_harmonic(set.perc_second);
            mark_dirty();
        } else if (slot == 11) {
            set.perc_fast = !set.perc_fast;
            organ_set_perc_decay(set.perc_fast);
            mark_dirty();
        }
    }
}

/* --------------------------------------------------------------- contract --- */
void app_init(void)
{
    organ_init();
    if (plat_store_load(OBJ_SETTINGS, &set, sizeof set) != (int)sizeof set || set.magic != SET_MAGIC) {
        defaults();
    } else if (set.octave < -2 || set.octave > 2 || set.swell > 127 || set.perc > 1 ||
               set.perc_second > 1 || set.perc_fast > 1 || set.preset > 7 || set.vib_mode > VIB_C3 ||
               set.drive > 127 || set.drive_char > 127 || set.reverb > 127 || set.leslie > LESLIE_FAST) {
        defaults();
    }
    apply_all();
    set_dirty = 0;
}

void app_ui_init(void)
{
    plat_glow(1);
    lcd_fill(0, 0, 240, 240, C_BLACK);
    ui.redraw = 1;
}

void app_panic(void)
{
    organ_reset();
    last_note = -1;
    prev_keys = 0;
}

void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    static float lt[ORGAN_BLOCK], rt[ORGAN_BLOCK];
    uint32_t done = 0, i;
    float g = (float)master_q12 / 4096.0f;
    midi_in();
    keys_poll();
    while (done < n) {
        uint32_t m = n - done > ORGAN_BLOCK ? ORGAN_BLOCK : n - done;
        organ_render(lt, rt, (int)m);
        for (i = 0; i < m; i++) {
            float xl = lt[i] * 4.0f, xr = rt[i] * 4.0f;
            xl = xl / (1.0f + fm_fabsf(xl));   /* soft clip: the transient is hot */
            xr = xr / (1.0f + fm_fabsf(xr));
            lr[2u * (done + i)] = (int32_t)(xl * 8388608.0f * g);
            lr[2u * (done + i) + 1u] = (int32_t)(xr * 8388608.0f * g);
        }
        done += m;
    }
}

void app_frame(void)
{
    uint32_t now = plat_ms(), b = plat_buttons(), i;
    uint32_t edge = b & ~prev_btn;
    for (i = 0; i < NB; i++)
        if (edge & (1u << i))
            button((int)i, now);
    prev_btn = b;

    encoder(EN_SELECT, plat_enc(EN_SELECT));
    encoder(EN_ALGO, plat_enc(EN_ALGO));
    encoder(EN_PRESET, plat_enc(EN_PRESET));
    encoder(EN_K1, plat_enc(EN_K1));
    encoder(EN_K2, plat_enc(EN_K2));
    encoder(EN_K3, plat_enc(EN_K3));
    encoder(EN_K4, plat_enc(EN_K4));

    {
        uint32_t leds = (set_dirty ? (1u << B_SAVE) : 0u) | (set.perc ? (1u << B_ENV) : 0u) |
                        (drawbar_mode ? (1u << B_GLO) : 0u);
        uint32_t keys = 0;
        if (drawbar_mode) {
            for (i = 0; i < NKEYS; i++)
                if (KEY_BAR[i] >= 0)
                    keys |= 1u << i;
        } else {
            keys = plat_keys();
        }
        led_key_count = 0;
        for (i = 0; i < NKEYS; i++)
            if (keys & (1u << i))
                led_key_count++;
        plat_lights(leds, keys, 0, 0, 1);
    }

    if (ui.redraw || (now & 0x7Fu) == 0) {
        ui.redraw = 0;
        draw();
    }
}

void app_input_only(void)
{
}

void app_say(const char *a, const char *b)
{
    say(a, b);
}

#ifdef APP_HOST_EXPECT
int app_expect(const char *what, int *got)
{
    if (!strcmp(what, "note"))
        *got = last_note;
    else if (!strcmp(what, "octave"))
        *got = set.octave;
    else if (!strcmp(what, "perc"))
        *got = set.perc;
    else if (!strcmp(what, "dirty"))
        *got = set_dirty;
    else if (!strcmp(what, "manual"))
        *got = ui.manual;
    else if (!strcmp(what, "page"))
        *got = ui.page;
    else if (!strcmp(what, "preset"))
        *got = set.preset;
    else if (!strcmp(what, "vib"))
        *got = set.vib_mode;
    else if (!strcmp(what, "drive"))
        *got = set.drive;
    else if (!strcmp(what, "reverb"))
        *got = set.reverb;
    else if (!strcmp(what, "leslie"))
        *got = set.leslie;
    else if (!strcmp(what, "drawbar"))
        *got = drawbar_mode;
    else if (!strcmp(what, "ledkeys"))
        *got = led_key_count;
    else if (!strncmp(what, "bar", 3) && what[3] >= '0' && what[3] <= '8')
        *got = set.bars[what[3] - '0'];   /* upper manual: the host tests use it */
    else
        return 0;
    return 1;
}
#endif
