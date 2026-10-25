/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal
 * Copyright (C) 2026 Rafael */
/* fm1-wheel: the app's half of the fm1-base contract. Phase 1: the tone
 * generator is live; the full drawbar/MIDI UI comes in Phase 3. */
#pragma once
#include <stdint.h>
#include "plat.h"

#define APP_NAME "FM1-WHEEL"
#define APP_SPLASH_RGB RGB(242, 120, 72)
#define APP_BLOCK 128u                   /* 2.9 ms at 44.1 kHz: organ key latency */

/* the store: settings, the panel calibration and Bluetooth's address (the platform's) */
enum { OBJ_SETTINGS, OBJ_PANEL, OBJ_BLE, OBJ_NOBJ };
#define APP_STORE_MAP {0xFC000u, 0xDC000u, 0xDE000u}

void app_init(void);
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12);
void app_panic(void);
void app_ui_init(void);
void app_frame(void);
void app_input_only(void);
void app_say(const char *a, const char *b);

#ifdef PLAT_HOST
#define APP_HOST_EXPECT
int app_expect(const char *what, int *got);
#endif
