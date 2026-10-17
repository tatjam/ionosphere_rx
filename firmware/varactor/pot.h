// Copyright (C) 2026 tatjam
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <avr/io.h>
#include <stdint.h>

#define POT_PIN_UD  PA7
#define POT_PORT_UD PORTA
#define POT_DDR_UD  DDRA

#define POT_PIN_CS  PB2
#define POT_PORT_CS PORTB
#define POT_DDR_CS  DDRB

#define POT_MAX_TAP   0x3F
#define POT_START_TAP 0x1F

void pot_init();
void pot_set_tap(uint8_t value);
uint8_t pot_get_tap();
