// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later
// Peripherals used:
// - PA6
// - PA4
// - PA5

#pragma once
#include <stdint.h>

void dac_spi_init();
void dac_spi_send(uint16_t value);
