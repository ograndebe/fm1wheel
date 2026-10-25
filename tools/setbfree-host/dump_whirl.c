/* SPDX-License-Identifier: GPL-2.0-or-later
 * Host-only dumper for the fm1-wheel Leslie: initializes setBfree's whirl at
 * 1,024 displacement positions and writes the static tables the firmware
 * needs. Not firmware. */
#include <stdio.h>
#include <stdlib.h>

#include "whirl.h"

static void floats(FILE *f, const char *name, const float *a, int n, int per)
{
    int i;
    fprintf(f, "static const float %s[%d] = {\n", name, n);
    for (i = 0; i < n; i++)
        fprintf(f, "%#.9gf%s", a[i], i == n - 1 ? "\n" : (i % per == per - 1 ? ",\n" : ", "));
    fprintf(f, "};\n\n");
}

static void ints(FILE *f, const char *name, const int *a, int n)
{
    int i;
    fprintf(f, "static const int %s[%d] = {", name, n);
    for (i = 0; i < n; i++)
        fprintf(f, "%d%s", a[i], i == n - 1 ? "" : ", ");
    fprintf(f, "};\n\n");
}

int main(int argc, char **argv)
{
    struct b_whirl *w;
    FILE *f;
    int i, j;

    if (argc != 2) {
        fprintf(stderr, "usage: %s OUT.h\n", argv[0]);
        return 1;
    }
    w = allocWhirl();
    initWhirl(w, NULL, 44100.0);

    f = fopen(argv[1], "w");
    if (!f) {
        perror(argv[1]);
        return 1;
    }
    fprintf(f, "/* Generated from setBfree b_whirl at %u positions, 44.1 kHz.\n"
               " * GPL-2.0-or-later. Do not edit. */\n"
               "#ifndef WB_WHIRL_TABLES_H\n#define WB_WHIRL_TABLES_H\n\n"
               "#define WB_WHIRL_N %u\n\n",
            (unsigned)WHIRL_DISPLC_SIZE, (unsigned)WHIRL_DISPLC_SIZE);
    floats(f, "wb_hn_fwd", w->hnFwdDispl, (int)WHIRL_DISPLC_SIZE, 8);
    floats(f, "wb_hn_bwd", w->hnBwdDispl, (int)WHIRL_DISPLC_SIZE, 8);
    floats(f, "wb_dr_fwd", w->drFwdDispl, (int)WHIRL_DISPLC_SIZE, 8);
    floats(f, "wb_dr_bwd", w->drBwdDispl, (int)WHIRL_DISPLC_SIZE, 8);
    fprintf(f, "static const float wb_bfw[WB_WHIRL_N][5] = {\n");
    for (i = 0; i < (int)WHIRL_DISPLC_SIZE; i++)
        fprintf(f, "{%#.9gf,%#.9gf,%#.9gf,%#.9gf,%#.9gf}%s\n", w->bfw[i].b[0], w->bfw[i].b[1], w->bfw[i].b[2],
                w->bfw[i].b[3], w->bfw[i].b[4], i == (int)WHIRL_DISPLC_SIZE - 1 ? "" : ",");
    fprintf(f, "};\n\nstatic const float wb_bbw[WB_WHIRL_N][5] = {\n");
    for (i = 0; i < (int)WHIRL_DISPLC_SIZE; i++)
        fprintf(f, "{%#.9gf,%#.9gf,%#.9gf,%#.9gf,%#.9gf}%s\n", w->bbw[i].b[0], w->bbw[i].b[1], w->bbw[i].b[2],
                w->bbw[i].b[3], w->bbw[i].b[4], i == (int)WHIRL_DISPLC_SIZE - 1 ? "" : ",");
    fprintf(f, "};\n\n");
    floats(f, "wb_horn_spacing", w->hornSpacing, 6, 6);
    floats(f, "wb_drum_spacing", w->drumSpacing, 6, 6);
    ints(f, "wb_horn_phase", w->hornPhase, 6);
    ints(f, "wb_drum_phase", w->drumPhase, 6);
    floats(f, "wb_hafw", w->hafw, 8, 8);
    floats(f, "wb_hbfw", w->hbfw, 8, 8);
    floats(f, "wb_drfl", w->drfL, 8, 8);
    floats(f, "wb_drfr", w->drfR, 8, 8);
    fprintf(f, "#define WB_HORN_LEVEL %#.9gf\n#define WB_LEAKAGE %#.9gf\n", w->hornLevel, w->leakage);
    fprintf(f, "#define WB_HORN_RPM_SLOW %#.9gf\n#define WB_HORN_RPM_FAST %#.9gf\n"
               "#define WB_DRUM_RPM_SLOW %#.9gf\n#define WB_DRUM_RPM_FAST %#.9gf\n"
               "#define WB_HORN_ACC %#.9gf\n#define WB_HORN_DEC %#.9gf\n"
               "#define WB_DRUM_ACC %#.9gf\n#define WB_DRUM_DEC %#.9gf\n",
            w->hornRPMslow, w->hornRPMfast, w->drumRPMslow, w->drumRPMfast,
            w->hornAcc, w->hornDec, w->drumAcc, w->drumDec);
    fprintf(f, "#define WB_MIC_HLL %#.9gf\n#define WB_MIC_HLR %#.9gf\n"
               "#define WB_MIC_HRL %#.9gf\n#define WB_MIC_HRR %#.9gf\n"
               "#define WB_MIC_DLL %#.9gf\n#define WB_MIC_DLR %#.9gf\n"
               "#define WB_MIC_DRL %#.9gf\n#define WB_MIC_DRR %#.9gf\n\n"
               "#endif /* WB_WHIRL_TABLES_H */\n",
            w->hornMic_hll, w->hornMic_hlr, w->hornMic_hrl, w->hornMic_hrr,
            w->drumMic_dll, w->drumMic_dlr, w->drumMic_drl, w->drumMic_drr);
    fclose(f);
    printf("wrote %s (%u positions)\n", argv[1], (unsigned)WHIRL_DISPLC_SIZE);
    return 0;
}
