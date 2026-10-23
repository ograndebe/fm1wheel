/* SPDX-License-Identifier: GPL-3.0-or-later
 * The freestanding libc the firmware links: the JieLi toolchain has no
 * <string.h>. The host build uses the real one. */
#pragma once

#ifdef PLAT_HOST
#include <string.h>
#else
void *memset(void *d, int c, unsigned n);
void *memcpy(void *d, const void *s, unsigned n);
#endif
