/* SPDX-License-Identifier: GPL-3.0-or-later
 * WheelBox scanner vibrato/chorus: a float port of setBfree's src/vibrato.c
 * (GPL-2.0-or-later, Copyright (C) 2003-2004 Fredrik Kilander, 2008-2018
 * Robin Gareus). The tables are built at init with fastmath's fm_sinf. */
#pragma once

#include <stdint.h>

#define VIB_TABLE_SIZE 2048
#define VIB_BUF_SIZE 1024

enum { VIB_OFF, VIB_V1, VIB_C1, VIB_V2, VIB_C2, VIB_V3, VIB_C3 };

typedef struct {
    uint32_t offset1[VIB_TABLE_SIZE];
    uint32_t offset2[VIB_TABLE_SIZE];
    uint32_t offset3[VIB_TABLE_SIZE];
    const uint32_t *offset;
    uint32_t stator, stator_inc;
    int out_pos;
    float buf[VIB_BUF_SIZE];
    uint8_t mixed;
} vib_t;

void vib_init(vib_t *v, float fs);
void vib_set_mode(vib_t *v, int mode);
void vib_proc(vib_t *v, const float *in, float *out, int n);
