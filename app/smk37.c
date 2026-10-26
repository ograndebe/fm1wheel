/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Rafael
 * fm1-wheel on the M-Vave SMK-37: the 37-key keybed, the drawbar faders and the pads.
 *
 * Included by app.c only when BOARD_SMK37: it uses the app's state and helpers, and the platform's SMK API
 * (plat_keybed, plat_pad_hits, plat_fader, plat_pad_rgb). The FM-1 build does not compile it.
 *
 * Drawbars: four faders on one of three pages. Pages 1/2 hold bars 16'..4' and 2⅔'..1⅓'. Page 3 holds the 1'
 * bar and the three percussion switches. Pads 1-8 (top row) recall the eight factory registrations; pads 9-11
 * (bottom-left) select the page; pads 12-16 are reserved. The keybed always plays; a drawbar fader only takes
 * over after it passes the bar's current value (no jumps). */
#define SMK_PAGE_PADS 8
#define SMK_PAGE_FIRST 8
#define SMK_FADER_TAKEOVER 3

static int16_t smk_org[SMK_NKEYBED];       /* 0: not held; else the organ key it started + 1 */
static uint8_t smk_fader_engage[4];
static int16_t smk_fader_last[4];
static uint32_t smk_btn_prev;

static const char *const SMK_PAGE_NAME[3] = {"A", "B", "C"};

static void smk_fader_reset(void)
{
    uint32_t i;
    for (i = 0; i < 4u; i++) {
        smk_fader_engage[i] = 0;
        smk_fader_last[i] = -1;
    }
}

static void smk_release_keys(void)
{
    uint32_t k;
    for (k = 0; k < SMK_NKEYBED; k++) {
        if (smk_org[k]) {
            organ_key_off(smk_org[k] - 1);
            smk_org[k] = 0;
        }
    }
}

static void smk_key_event(uint32_t k, uint32_t vel)
{
    int note, key;
    (void)vel;                                 /* the tonewheel model has no velocity; kept for the keybed's curves */
    note = 48 + (int)k + (int)set.octave * 12;
    key = note - 36;
    if (key < 0 || key > 60)                    /* the upper manual ends at MIDI 96 */
        return;
    organ_key_on(key);
    smk_org[k] = (int16_t)(key + 1);
    last_note = note;
}

static void smk_keys_poll(void)
{
    uint64_t now = plat_keybed(), hits = plat_keybed_hits();
    uint32_t k;
    for (k = 0; k < SMK_NKEYBED; k++) {
        uint64_t b = (uint64_t)1u << k;
        if ((hits & b) && !smk_org[k])
            smk_key_event(k, (uint32_t)plat_keybed_vel((int)k));
        if (!(now & b) && smk_org[k]) {
            organ_key_off(smk_org[k] - 1);
            smk_org[k] = 0;
        }
    }
}

static void smk_page_set(uint8_t page)
{
    ui.page = page;
    smk_fader_reset();
    ui.redraw = 1;
    say("FADERS", SMK_PAGE_NAME[page]);
}

static int smk_bar_of(int v)
{
    return (v * 8 + 63) / 127;
}

static void smk_fader_bar(uint32_t i, int v, int bar)
{
    int target = set.bars[ui.manual * 9 + bar] * 127 / 8;
    int value;
    if (!smk_fader_engage[i]) {
        int d = v - target;
        if (d < -SMK_FADER_TAKEOVER || d > SMK_FADER_TAKEOVER)
            return;                             /* soft takeover: wait until the fader reaches the bar */
        smk_fader_engage[i] = 1;
    }
    value = smk_bar_of(v);
    if (value == set.bars[ui.manual * 9 + bar])
        return;
    set.bars[ui.manual * 9 + bar] = (uint8_t)value;
    organ_set_drawbar(ui.manual, bar, value);
    mark_dirty();
}

static void smk_fader_toggle(uint32_t i, int v, int which)
{
    int last = smk_fader_last[i], on = v >= 64;
    smk_fader_last[i] = (int16_t)v;
    if (last < 0 || (last >= 64) == on)
        return;
    if (which == 0) {
        set.perc = (uint8_t)on;
        organ_set_percussion(on);
        say("PERCUSSION", on ? "ON" : "OFF");
    } else if (which == 1) {
        set.perc_second = (uint8_t)on;
        organ_set_perc_harmonic(on);
        say("PERC", on ? "2ND" : "3RD");
    } else {
        set.perc_fast = (uint8_t)on;
        organ_set_perc_decay(on);
        say("PERC", on ? "FAST" : "SLOW");
    }
    mark_dirty();
}

static void smk_pad(uint32_t p)
{
    if (p < SMK_PAGE_PADS) {                    /* top row: the eight factory registrations */
        set.preset = (uint8_t)p;
        preset_apply();
        smk_fader_reset();
    } else if (p < SMK_PAGE_FIRST + 3u) {        /* bottom-left: the fader page */
        smk_page_set((uint8_t)(p - SMK_PAGE_FIRST));
    } else {
        say("PAD", "RESERVED");
    }
}

static void smk_input(void)
{
    uint32_t hits, sb, down, i;
    int v;
    if (!drawbar_mode) {
        smk_btn_prev = plat_smk_buttons();
        (void)plat_pad_hits();                  /* drop presses made while the faders were off */
        return;
    }
    hits = plat_pad_hits();
    for (i = 0; i < SMK_NPADS; i++)
        if (hits & (1u << i))
            smk_pad(i);
    sb = plat_smk_buttons();
    down = sb & ~smk_btn_prev;
    smk_btn_prev = sb;
    if (down & (1u << SB_FADER_BANK))
        smk_page_set((uint8_t)((ui.page + 1u) % 3u));
    if (down & (1u << SB_LEFT))
        smk_page_set((uint8_t)((ui.page + 2u) % 3u));
    if (down & (1u << SB_RIGHT))
        smk_page_set((uint8_t)((ui.page + 1u) % 3u));
    for (i = 0; i < 4u; i++) {
        v = plat_fader((int)i);
        if (v < 0)
            continue;
        if (ui.page < 2u)
            smk_fader_bar(i, v, (int)(ui.page * 4u + i));
        else if (i == 0u)
            smk_fader_bar(i, v, 8);
        else
            smk_fader_toggle(i, v, (int)i - 1);
    }
}

static void smk_lights(void)
{
    uint32_t p, lit = 0;
    for (p = 0; p < SMK_NPADS; p++) {
        if (p < SMK_PAGE_PADS) {
            int on = p == set.preset;
            plat_pad_rgb(p, (uint8_t)(on ? 255 : 32), (uint8_t)(on ? 160 : 20), (uint8_t)(on ? 20 : 4));
        } else if (p < SMK_PAGE_FIRST + 3u) {
            int on = p - SMK_PAGE_FIRST == ui.page;
            plat_pad_rgb(p, (uint8_t)(on ? 40 : 8), (uint8_t)(on ? 180 : 30), (uint8_t)(on ? 255 : 50));
        } else {
            plat_pad_rgb(p, 4, 4, 4);
        }
    }
    if (ui.page)
        lit |= 1u << SB_FADER_BANK;
    plat_smk_leds(lit);
    plat_pad_lights(1);
}

static void smk_init(void)
{
    drawbar_mode = 1;                           /* the SMK's faders and pads are drawbar controls by default */
    ui.page = 0;
    smk_fader_reset();
    smk_release_keys();
    smk_btn_prev = plat_smk_buttons();
    plat_smk_pads(SMK_PADS_OWN);                /* the pads are not white keys here */
    plat_pad_lights(1);
}
