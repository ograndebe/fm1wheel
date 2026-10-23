/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "host.h"

double SampleRateD = 44100.0;
int SampleRateI = 44100;

void useMIDIControlFunction(void *m, const char *name, void (*f)(void *, unsigned char), void *d)
{
    (void)m;
    (void)name;
    (void)f;
    (void)d;
}

void notifyControlChangeByName(void *m, const char *name, unsigned char v)
{
    (void)m;
    (void)name;
    (void)v;
}

void notifyControlChangeById(void *m, int id, unsigned char v)
{
    (void)m;
    (void)id;
    (void)v;
}
