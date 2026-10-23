// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later
// Peripherals used:
// - PA7
// - PB2

#pragma once

#include <stdint.h>

void pot_init();
void pot_set_tap(uint8_t value);
uint8_t pot_get_tap();
