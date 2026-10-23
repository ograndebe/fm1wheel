/* SPDX-License-Identifier: GPL-2.0-or-later
 * Host-only harness for the setBfree DSP: the globals and MIDI stubs the DSP
 * files expect when they are built without JACK/LV2. Not part of the firmware. */
#pragma once

#include <stdint.h>

extern double SampleRateD;
extern int SampleRateI;

/* setBfree calls these from its MIDI-control registration; the harness drives
 * the setters directly instead, so the stubs do nothing. */
void useMIDIControlFunction(void *m, const char *name, void (*f)(void *, unsigned char), void *d);
void notifyControlChangeByName(void *m, const char *name, unsigned char v);
void notifyControlChangeById(void *m, int id, unsigned char v);
