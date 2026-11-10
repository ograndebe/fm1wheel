/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Rafael
 * fm1-wheel on the M-Vave SMK-37: the 37-key keybed, the drawbar faders and the pads.
 *
 * Included by app.c only when BOARD_SMK37: it uses the app's state and helpers, and the platform's SMK API
 * (plat_keybed, plat_pad_hits, plat_fader, plat_pad_rgb). The FM-1 build does not compile it.
 *
 * Drawbars: the four faders are the last four bars (2', 1⅗', 1⅓', 1'), fader 4 the highest (1'), and reversed:
 * the minimum position is the maximum volume. The five rightmost pad columns are the first five (16', 5⅓', 8',
 * 4', 2⅔'): the top pad pushes a bar in, the bottom pad pulls it out, as the FM-1's key pairs do. The six pads
 * on the left (top 1-3, bottom 9-11) recall presets 1-6. The keybed always plays; the pad/fader drawbars are
 * always on (GLO does not toggle them). */
#define SMK_PAD_BARS 5
#define SMK_PAD_BAR_FIRST 3                 /* top row pads 4-8: push the first five bars */
#define SMK_PAD_BAR_LOW 11                  /* bottom row pads 12-16: pull them */

static int16_t smk_org[SMK_NKEYBED];       /* 0: not held; else the organ key it started + 1 */

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

static int smk_bar_of(int v)
{
    return 8 - (v * 8 + 63) / 127;             /* the faders are reversed: minimum is fully out */
}

static void smk_fader_bar(int v, int bar)
{
    int value = smk_bar_of(v);
    if (value == set.bars[ui.manual * 9 + bar])
        return;
    set.bars[ui.manual * 9 + bar] = (uint8_t)value;
    organ_set_drawbar(ui.manual, bar, value);
    mark_dirty();
}

static int smk_pad_bar(uint32_t p)
{
    if (p >= SMK_PAD_BAR_FIRST && p < SMK_PAD_BAR_FIRST + SMK_PAD_BARS)
        return (int)(p - SMK_PAD_BAR_FIRST);
    if (p >= SMK_PAD_BAR_LOW && p < SMK_PAD_BAR_LOW + SMK_PAD_BARS)
        return (int)(p - SMK_PAD_BAR_LOW);
    return -1;
}

static void smk_pad(uint32_t p)
{
    int bar = smk_pad_bar(p);
    if (bar >= 0) {
        drawbar_step(bar, p < SMK_PAD_BAR_LOW ? -1 : 1);   /* top row pushes, bottom row pulls */
    } else {
        set.preset = (uint8_t)(p < 3u ? p : p - 5u);       /* top-left 1-3, bottom-left 9-11: presets 1-6 */
        preset_apply();
    }
}

static void smk_input(void)
{
    uint32_t hits, i;
    int v;
    hits = plat_pad_hits();
    for (i = 0; i < SMK_NPADS; i++)
        if (hits & (1u << i))
            smk_pad(i);
    for (i = 0; i < 4u; i++) {
        v = plat_fader((int)i);
        if (v >= 0)
            smk_fader_bar(v, 5 + (int)i);       /* faders 1-4: the last four bars */
    }
}

static void smk_lights(void)
{
    uint32_t p;
    for (p = 0; p < SMK_NPADS; p++) {
        int bar = smk_pad_bar(p);
        if (bar >= 0) {
            int v = set.bars[ui.manual * 9 + bar];
            int k = p < SMK_PAD_BAR_LOW ? 8 - v : v;   /* push: travel left; pull: value out */
            plat_pad_rgb(p, (uint8_t)(255 * k / 8), (uint8_t)(150 * k / 8), (uint8_t)(20 * k / 8));
        } else {
            int preset = p < 3u ? (int)p : (int)p - 5;
            int on = preset == set.preset;
            plat_pad_rgb(p, (uint8_t)(on ? 255 : 32), (uint8_t)(on ? 160 : 20), (uint8_t)(on ? 20 : 4));
        }
    }
    plat_smk_leds(0);                           /* the fader-page LEDs are not used */
    plat_pad_lights(1);
}

static void smk_init(void)
{
    drawbar_mode = 1;                           /* the SMK's faders and pads are drawbar controls by default */
    ui.page = 0;
    smk_release_keys();
    plat_smk_pads(SMK_PADS_OWN);                /* the pads are not white keys here */
    plat_pad_lights(1);
}
