// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <stdint.h>
#include <stdbool.h>

#define STATUS_PIN_LED	PA7
#define STATUS_PORT_LED PORTA
#define STATUS_DDR_LED	DDRA

void status_init();

// return if we need to keep processing main loop
bool status_process();

void status_blip();
